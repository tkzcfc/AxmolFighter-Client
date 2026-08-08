#pragma once

#include "Types.h"

NS_MG_BEGIN

class Component : public Object
{
public:
    typedef Object Super;

public:
    Component();

    virtual ~Component();

    ComponentTypeId getTypeId() const;

private:
    friend class ECSManager;
    friend class Entity;
    ComponentTypeId m_typeId = INVALID_COMPONENT_TYPE_ID;
};

NS_MG_END
