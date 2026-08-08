#include "Component.h"

NS_MG_BEGIN

Component::Component()
{
    MG_ECS_OBJECT_GC_LOG("[%p] new Component\n", this);
}
Component::~Component()
{
    MG_ECS_OBJECT_GC_LOG("[%p] delete Component\n", this);
}

ComponentTypeId Component::getTypeId() const
{
    return m_typeId;
}

NS_MG_END
