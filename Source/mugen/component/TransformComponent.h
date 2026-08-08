#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/core/math/Vec2.h"
#include "mugen/core/math/Vec3.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

class TransformComponent : public Component
{
public:
    typedef Component Super;

public:
    TransformComponent() {}
    virtual ~TransformComponent() {}

    Vector3i position;
    Vector2f scale;

    // 角色朝向
    FacingDirection facingDirection = FacingDirection::kFacingRight;

    MG_DEFINE_SERIALIZABLE(position, scale, facingDirection);
};

NS_MG_END
