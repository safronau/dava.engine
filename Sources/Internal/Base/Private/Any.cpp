#include "Base/Any.h"

namespace DAVA
{
bool Any::LoadData(void* data, const Type* type_)
{
    type = type_;

    if (type->IsPointer())
    {
        void** src = reinterpret_cast<void**>(data);
        anyStorage.SetAuto(*src);
        return true;
    }
    else if (type->IsTrivial())
    {
        anyStorage.SetData(data, type->GetSize());
        return true;
    }

    return false;
}

bool Any::StoreData(void* data, size_t size) const
{
    if (nullptr != type && size >= type->GetSize())
    {
        if (type->IsPointer())
        {
            void** dst = reinterpret_cast<void**>(data);
            *dst = anyStorage.GetAuto<void*>();
            return true;
        }
        else if (type->IsTrivial())
        {
            std::memcpy(data, anyStorage.GetData(), size);
            return true;
        }
    }

    return false;
}

bool Any::operator==(const Any& any) const
{
    if (type == nullptr && any.type == nullptr)
    {
        return true;
    }
    else if (type == nullptr || any.type == nullptr)
    {
        return false;
    }

    if (type->IsPointer() && any.type->IsPointer())
    {
        return anyStorage.GetSimple<void*>() == any.anyStorage.GetSimple<void*>();
    }
    else if (type->IsTrivial() && type == any.type)
    {
        return (0 == std::memcmp(anyStorage.GetData(), any.anyStorage.GetData(), type->GetSize()));
    }

    return (*compareFn)(*this, any);
}
} // namespace DAVA
