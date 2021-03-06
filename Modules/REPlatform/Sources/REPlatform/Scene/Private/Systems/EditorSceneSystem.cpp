#include "REPlatform/Scene/Systems/EditorSceneSystem.h"
#include "REPlatform/Scene/SceneEditor2.h"

#include <Scene3D/Scene.h>

namespace DAVA
{
void EditorSceneSystem::EnableSystem()
{
    systemIsEnabled = true;
}

void EditorSceneSystem::DisableSystem()
{
    systemIsEnabled = false;
}

bool EditorSceneSystem::IsSystemEnabled() const
{
    return systemIsEnabled;
}

bool EditorSceneSystem::AcquireInputLock(DAVA::Scene* scene)
{
    return static_cast<SceneEditor2*>(scene)->AcquireInputLock(this);
}

void EditorSceneSystem::ReleaseInputLock(DAVA::Scene* scene)
{
    static_cast<SceneEditor2*>(scene)->ReleaseInputLock(this);
}
} // namespace DAVA
