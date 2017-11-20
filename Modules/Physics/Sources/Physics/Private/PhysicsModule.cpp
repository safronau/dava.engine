#include "Physics/PhysicsModule.h"
#include "Physics/StaticBodyComponent.h"
#include "Physics/DynamicBodyComponent.h"
#include "Physics/BoxShapeComponent.h"
#include "Physics/CapsuleShapeComponent.h"
#include "Physics/PlaneShapeComponent.h"
#include "Physics/SphereShapeComponent.h"
#include "Physics/MeshShapeComponent.h"
#include "Physics/ConvexHullShapeComponent.h"
#include "Physics/HeightFieldShapeComponent.h"
#include "Physics/VehicleCarComponent.h"
#include "Physics/VehicleTankComponent.h"
#include "Physics/VehicleChassisComponent.h"
#include "Physics/VehicleWheelComponent.h"
#include "Physics/BoxCharacterControllerComponent.h"
#include "Physics/CapsuleCharacterControllerComponent.h"
#include "Physics/WASDPhysicsControllerComponent.h"
#include "Physics/PhysicsGeometryCache.h"
#include "Physics/Private/PhysicsMath.h"

#include <Engine/Engine.h>
#include <Engine/EngineContext.h>
#include <Logger/Logger.h>
#include <Render/3D/PolygonGroup.h>
#include <Render/Highlevel/Landscape.h>
#include <Render/Highlevel/Heightmap.h>
#include <MemoryManager/MemoryManager.h>
#include <Reflection/ReflectionRegistrator.h>
#include <Math/MathConstants.h>

#include <physx/PxPhysicsAPI.h>
#include <PxShared/pvd/PxPvd.h>

namespace DAVA
{
namespace PhysicsModuleDetail
{
physx::PxPvd* CreatePvd(physx::PxFoundation* foundation)
{
    IModule* physicsDebugModule = GetEngineContext()->moduleManager->GetModule("PhysicsDebugModule");
    if (physicsDebugModule == nullptr)
    {
        return nullptr;
    }

    Reflection moduleRef = Reflection::Create(ReflectedObject(physicsDebugModule));
    AnyFn fn = moduleRef.GetMethod("CreatePvd");
    DVASSERT(fn.IsValid() == true);
#if defined(__DAVAENGINE_DEBUG__)
    AnyFn::Params params = fn.GetInvokeParams();
    DVASSERT(params.retType == Type::Instance<physx::PxPvd*>());
    DVASSERT(params.argsType.size() == 1);
    DVASSERT(params.argsType[0] == Type::Instance<physx::PxFoundation*>());
#endif

    return fn.Invoke(foundation).Cast<physx::PxPvd*>(nullptr);
}

void ReleasePvd()
{
    IModule* physicsDebugModule = GetEngineContext()->moduleManager->GetModule("PhysicsDebugModule");
    if (physicsDebugModule == nullptr)
    {
        return;
    }

    Reflection moduleRef = Reflection::Create(ReflectedObject(physicsDebugModule));
    AnyFn fn = moduleRef.GetMethod("ReleasePvd");
    DVASSERT(fn.IsValid() == true);
    fn.Invoke();
}

class AssertHandler : public physx::PxAssertHandler
{
public:
    void operator()(const char* exp, const char* file, int line, bool& ignore)
    {
        Assert::FailBehaviour result = HandleAssert(exp, file, line);
        if (result != Assert::FailBehaviour::Continue)
        {
            DVASSERT_HALT();
        }
    }
};

void BuildPhysxMeshInfo(const Vector<PolygonGroup*>& polygons, Vector<physx::PxVec3>& vertices, Vector<physx::PxU32>& indices)
{
    uint32 indexOffset = 0;
    for (PolygonGroup* polygon : polygons)
    {
        int32 vertexCount = polygon->vertexCount;
        vertices.reserve(vertices.size() + vertexCount);

        for (int32 i = 0; i < vertexCount; ++i)
        {
            Vector3 coord;
            polygon->GetCoord(i, coord);
            vertices.push_back(PhysicsMath::Vector3ToPxVec3(coord));
        }

        int32 indexCount = polygon->indexCount;
        indices.reserve(indices.size() + indexCount);
        for (int32 i = 0; i < indexCount; ++i)
        {
            int32 index;
            polygon->GetIndex(i, index);
            indices.push_back(static_cast<uint32>(index) + indexOffset);
        }

        indexOffset = static_cast<uint32>(vertices.size());
    }
}
}

class PhysicsModule::PhysicsAllocator : public physx::PxAllocatorCallback
{
public:
    void* allocate(size_t size, const char* typeName, const char* filename, int line) override
    {
// MemoryManager temporary disabled as AlignedAllocate produce heap corruption on Deallocation
#if defined(DAVA_MEMORY_PROFILING_ENABLE)
        int32 alignment = 16;
        int32 offset = alignment - 1 + sizeof(void*);
        void* p1 = MemoryManager::Instance()->Allocate(size + offset, ALLOC_POOL_PHYSICS);
        if (p1 == nullptr)
        {
            return nullptr;
        }

        void** p2 = reinterpret_cast<void**>((reinterpret_cast<size_t>(p1) + offset) & ~(alignment - 1));
        p2[-1] = p1;
        return p2;
#else
        return defaultAllocator.allocate(size, typeName, filename, line);
#endif
    }

    void deallocate(void* ptr) override
    {
#if defined(DAVA_MEMORY_PROFILING_ENABLE)
        if (ptr)
        {
            MemoryManager::Instance()->Deallocate(static_cast<void**>(ptr)[-1]);
        }
#else
        defaultAllocator.deallocate(ptr);
#endif
    }

private:
    physx::PxDefaultAllocator defaultAllocator;
};

class PhysicsModule::PhysicsErrotCallback : public physx::PxErrorCallback
{
public:
    void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line) override
    {
        defaultErrorCallback.reportError(code, message, file, line);
    }

private:
    physx::PxDefaultErrorCallback defaultErrorCallback;
};

PhysicsModule::PhysicsModule(Engine* engine)
    : IModule(engine)
{
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(PhysicsModule);

    bodyComponents.reserve(2);
    bodyComponents.push_back(Component::STATIC_BODY_COMPONENT);
    bodyComponents.push_back(Component::DYNAMIC_BODY_COMPONENT);

    shapeComponents.reserve(7);
    shapeComponents.push_back(Component::BOX_SHAPE_COMPONENT);
    shapeComponents.push_back(Component::CAPSULE_SHAPE_COMPONENT);
    shapeComponents.push_back(Component::SPHERE_SHAPE_COMPONENT);
    shapeComponents.push_back(Component::PLANE_SHAPE_COMPONENT);
    shapeComponents.push_back(Component::MESH_SHAPE_COMPONENT);
    shapeComponents.push_back(Component::CONVEX_HULL_SHAPE_COMPONENT);
    shapeComponents.push_back(Component::HEIGHT_FIELD_SHAPE_COMPONENT);

    vehicleComponents.reserve(4);
    vehicleComponents.push_back(Component::VEHICLE_CAR_COMPONENT);
    vehicleComponents.push_back(Component::VEHICLE_TANK_COMPONENT);
    vehicleComponents.push_back(Component::VEHICLE_CHASSIS_COMPONENT);
    vehicleComponents.push_back(Component::VEHICLE_WHEEL_COMPONENT);

    characterControllerComponents.reserve(2);
    characterControllerComponents.push_back(Component::BOX_CHARACTER_CONTROLLER_COMPONENT);
    characterControllerComponents.push_back(Component::CAPSULE_CHARACTER_CONTROLLER_COMPONENT);
}

void PhysicsModule::Init()
{
    using namespace physx;

    allocator = new PhysicsAllocator();
    errorCallback = new PhysicsErrotCallback();

    PxTolerancesScale toleranceScale;

    foundation = PxCreateFoundation(PX_FOUNDATION_VERSION, *allocator, *errorCallback);
    DVASSERT(foundation);

    physx::PxPvd* pvd = PhysicsModuleDetail::CreatePvd(foundation);
    physics = PxCreateBasePhysics(PX_PHYSICS_VERSION, *foundation, toleranceScale, true, pvd);
    DVASSERT(physics);
    PxRegisterHeightFields(*physics);
    PxRegisterParticles(*physics); // For correct rigidDynamic->setGloblaPose after simulation stop

    PxCookingParams cookingParams(toleranceScale);
    cooking = PxCreateCooking(PX_PHYSICS_VERSION, *foundation, cookingParams);
    DVASSERT(cooking);

    PxInitVehicleSDK(*physics);
    PxVehicleSetBasisVectors(PxVec3(0.0f, 0.0f, 1.0f), PxVec3(1.0f, 0.0f, 0.0f));
    PxVehicleSetUpdateMode(PxVehicleUpdateMode::eVELOCITY_CHANGE);

    static PhysicsModuleDetail::AssertHandler assertHandler;
    PxSetAssertHandler(assertHandler);

    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(StaticBodyComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(DynamicBodyComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(BoxShapeComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(CapsuleShapeComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(SphereShapeComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(PlaneShapeComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(ConvexHullShapeComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(MeshShapeComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(HeightFieldShapeComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(VehicleCarComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(VehicleTankComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(VehicleChassisComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(VehicleWheelComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(BoxCharacterControllerComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(CapsuleCharacterControllerComponent);
    DAVA_REFLECTION_REGISTER_PERMANENT_NAME(WASDPhysicsControllerComponent);
}

void PhysicsModule::Shutdown()
{
    physx::PxCloseVehicleSDK();

    if (defaultMaterial != nullptr)
    {
        defaultMaterial->release();
    }

    if (cpuDispatcher != nullptr)
    {
        cpuDispatcher->release();
    }

    cooking->release();
    physics->release();
    PhysicsModuleDetail::ReleasePvd(); // PxPvd should be released between PxPhysics and PxFoundation
    foundation->release();
    SafeDelete(allocator);
    SafeDelete(errorCallback);
}

bool PhysicsModule::IsInitialized() const
{
    return foundation != nullptr && physics != nullptr;
}

void* PhysicsModule::Allocate(size_t size, const char* typeName, const char* filename, int line)
{
    return allocator->allocate(size, typeName, filename, line);
}

void PhysicsModule::Deallocate(void* ptr)
{
    allocator->deallocate(ptr);
}

physx::PxScene* PhysicsModule::CreateScene(const PhysicsSceneConfig& config, physx::PxSimulationFilterShader filterShader, physx::PxSimulationEventCallback* callback) const
{
    using namespace physx;

    DVASSERT(physics != nullptr);

    PxSceneDesc sceneDesc(physics->getTolerancesScale());
    sceneDesc.flags = PxSceneFlag::eENABLE_ACTIVE_ACTORS;
    sceneDesc.gravity = PhysicsMath::Vector3ToPxVec3(config.gravity);
    sceneDesc.filterShader = filterShader;
    sceneDesc.simulationEventCallback = callback;

    if (cpuDispatcher == nullptr)
    {
        cpuDispatcher = PxDefaultCpuDispatcherCreate(config.threadCount);
    }
    DVASSERT(cpuDispatcher);
    sceneDesc.cpuDispatcher = cpuDispatcher;

    PxScene* scene = physics->createScene(sceneDesc);
    DVASSERT(scene);

    return scene;
}

physx::PxActor* PhysicsModule::CreateStaticActor() const
{
    return physics->createRigidStatic(physx::PxTransform(physx::PxIDENTITY::PxIdentity));
}

physx::PxActor* PhysicsModule::CreateDynamicActor() const
{
    return physics->createRigidDynamic(physx::PxTransform(physx::PxIDENTITY::PxIdentity));
}

physx::PxShape* PhysicsModule::CreateBoxShape(const Vector3& halfSize) const
{
    return physics->createShape(physx::PxBoxGeometry(PhysicsMath::Vector3ToPxVec3(halfSize)), *GetDefaultMaterial(), true);
}

physx::PxShape* PhysicsModule::CreateCapsuleShape(float32 radius, float32 halfHeight) const
{
    return physics->createShape(physx::PxCapsuleGeometry(radius, halfHeight), *GetDefaultMaterial(), true);
}

physx::PxShape* PhysicsModule::CreateSphereShape(float32 radius) const
{
    return physics->createShape(physx::PxSphereGeometry(radius), *GetDefaultMaterial(), true);
}

physx::PxShape* PhysicsModule::CreatePlaneShape() const
{
    return physics->createShape(physx::PxPlaneGeometry(), *GetDefaultMaterial(), true);
}

physx::PxShape* PhysicsModule::CreateMeshShape(Vector<PolygonGroup*>&& polygons, const Vector3& scale, PhysicsGeometryCache* cache) const
{
    using namespace physx;

    std::sort(polygons.begin(), polygons.end());
    polygons.erase(std::unique(polygons.begin(), polygons.end()), polygons.end());

    DVASSERT(cache != nullptr);
    PxBase* mesh = cache->GetTriangleMeshEntry(polygons);
    if (mesh == nullptr)
    {
        Vector<PxVec3> vertices;
        Vector<PxU32> indices;
        PhysicsModuleDetail::BuildPhysxMeshInfo(polygons, vertices, indices);

        PxTriangleMeshDesc desc;
        desc.points.count = static_cast<PxU32>(vertices.size());
        desc.points.stride = sizeof(PxVec3);
        desc.points.data = vertices.data();
        desc.triangles.count = static_cast<PxU32>(indices.size() / 3);
        desc.triangles.stride = 3 * sizeof(PxU32);
        desc.triangles.data = indices.data();
        desc.flags = PxMeshFlags(0);

        physx::PxTriangleMeshCookingResult::Enum condition;
        PxDefaultMemoryOutputStream outStream;
        if (cooking->cookTriangleMesh(desc, outStream, &condition) == false)
        {
            Logger::Error("[Physics::CreateMeshShape] Mesh creation failure for polygon group with code: %u", static_cast<uint32>(condition));
            return nullptr;
        }

        physx::PxDefaultMemoryInputData inputStream(outStream.getData(), outStream.getSize());
        mesh = physics->createTriangleMesh(inputStream);
        DVASSERT(mesh != nullptr);
        cache->AddEntry(polygons, mesh);
    }
    PxTriangleMesh* triangleMesh = mesh->is<PxTriangleMesh>();
    DVASSERT(triangleMesh != nullptr);

    PxMeshScale pxScale(PxVec3(scale.x, scale.y, scale.z), PxQuat(PxIdentity));
    PxTriangleMeshGeometry geometry(triangleMesh, pxScale);
    PxShape* shape = physics->createShape(geometry, *GetDefaultMaterial(), true);

    return shape;
}

physx::PxShape* PhysicsModule::CreateConvexHullShape(Vector<PolygonGroup*>&& polygons, const Vector3& scale, PhysicsGeometryCache* cache) const
{
    using namespace physx;

    std::sort(polygons.begin(), polygons.end());
    polygons.erase(std::unique(polygons.begin(), polygons.end()), polygons.end());

    DVASSERT(cache != nullptr);
    PxBase* mesh = cache->GetConvexHullEntry(polygons);
    if (mesh == nullptr)
    {
        Vector<PxVec3> vertices;
        Vector<PxU32> indices;
        PhysicsModuleDetail::BuildPhysxMeshInfo(polygons, vertices, indices);

        PxConvexMeshDesc desc;
        desc.points.count = static_cast<PxU32>(vertices.size());
        desc.points.stride = sizeof(PxVec3);
        desc.points.data = vertices.data();
        desc.indices.count = static_cast<PxU32>(indices.size());
        desc.indices.stride = sizeof(PxU32);
        desc.indices.data = indices.data();
        desc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

        PxConvexMeshCookingResult::Enum condition;
        PxDefaultMemoryOutputStream outStream;
        if (cooking->cookConvexMesh(desc, outStream, &condition) == false)
        {
            Logger::Error("[Physics::CreateMeshShape] Mesh creation failure for polygon group with code: %u", static_cast<uint32>(condition));
            return nullptr;
        }

        physx::PxDefaultMemoryInputData inputStream(outStream.getData(), outStream.getSize());
        mesh = physics->createConvexMesh(inputStream);
        DVASSERT(mesh != nullptr);
        cache->AddEntry(polygons, mesh);
    }

    PxConvexMesh* convexMesh = mesh->is<PxConvexMesh>();
    DVASSERT(convexMesh != nullptr);
    PxMeshScale pxScale(PxVec3(scale.x, scale.y, scale.z), PxQuat(PxIdentity));
    PxConvexMeshGeometry geometry(convexMesh, pxScale);
    PxShape* shape = physics->createShape(geometry, *GetDefaultMaterial(), true);

    return shape;
}

physx::PxShape* PhysicsModule::CreateHeightField(Landscape* landscape, Matrix4& localPose) const
{
    using namespace physx;
    Heightmap* heightmap = landscape->GetHeightmap();

    uint32 size = heightmap->Size();
    uint32 samplesCount = size * size;
    Vector<PxHeightFieldSample> pxData(samplesCount);
    uint16* dvData = heightmap->Data();

    for (uint32 x = 0; x < size; ++x)
    {
        for (uint32 y = 0; y < size; ++y)
        {
            uint16 readHeight = dvData[x * size + y];
            PxHeightFieldSample& pxSample = pxData[x * size + y];
            pxSample.height = readHeight / 2;
            pxSample.materialIndex0 = 0;
            pxSample.materialIndex1 = 0;
        }
    }

    PxHeightFieldDesc desc;
    desc.format = PxHeightFieldFormat::eS16_TM;
    desc.nbColumns = size;
    desc.nbRows = size;
    desc.samples.data = pxData.data();
    desc.samples.stride = sizeof(PxHeightFieldSample);

    physx::PxDefaultMemoryOutputStream outStream;
    if (cooking->cookHeightField(desc, outStream) == false)
    {
        Logger::Error("[Physics::CreateHeightField] HeightField creation failure");
        return nullptr;
    }

    physx::PxDefaultMemoryInputData data(outStream.getData(), outStream.getSize());
    PxHeightField* heightfield = physics->createHeightField(data);

    float32 landscapeSize = landscape->GetLandscapeSize();
    physx::PxReal heightScale = landscape->GetLandscapeHeight() / 32767.f;
    physx::PxReal dimensionScale = landscapeSize / size;
    PxHeightFieldGeometry geometry(heightfield, PxMeshGeometryFlags(), heightScale, dimensionScale, dimensionScale);
    PxShape* shape = physics->createShape(geometry, *GetDefaultMaterial(), true);

    float32 translate = landscapeSize / 2.0f;
    localPose = Matrix4::MakeRotation(Vector3(1.0f, 0.0f, 0.0f), -PI_05) *
    Matrix4::MakeRotation(Vector3(0.0f, 0.0f, 1.0f), -PI_05) *
    Matrix4::MakeTranslation(Vector3(-translate, -translate, 0.0f));

    return shape;
}

physx::PxMaterial* PhysicsModule::GetDefaultMaterial() const
{
    if (defaultMaterial == nullptr)
    {
        defaultMaterial = physics->createMaterial(0.5f, 0.5f, 0.1f);
    }

    return defaultMaterial;
}

physx::PxAllocatorCallback* PhysicsModule::GetAllocator() const
{
    return allocator;
}

const Vector<uint32>& PhysicsModule::GetBodyComponentTypes() const
{
    return bodyComponents;
}

const Vector<uint32>& PhysicsModule::GetShapeComponentTypes() const
{
    return shapeComponents;
}

const Vector<uint32>& PhysicsModule::GetVehicleComponentTypes() const
{
    return vehicleComponents;
}

const Vector<uint32>& PhysicsModule::GetCharacterControllerComponentTypes() const
{
    return characterControllerComponents;
}

DAVA_VIRTUAL_REFLECTION_IMPL(PhysicsModule)
{
    ReflectionRegistrator<PhysicsModule>::Begin()
    .End();
}
}
