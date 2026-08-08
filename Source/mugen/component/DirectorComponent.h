#pragma once

#include "mugen/core/ecs/Component.h"

#include <string>

NS_MG_BEGIN

// 导演组件
class DirectorComponent : public Component
{
public:
    typedef Component Super;

public:
    DirectorComponent() {}

    virtual ~DirectorComponent() {}

    EntityId mapEntityId = INVALID_ENTITY_ID;

    MG_DEFINE_SERIALIZABLE(mapEntityId);

    //////////////////////////////////// 以下变量不参与序列化，仅在运行时使用 ////////////////////////////////////

    // 当前本地玩家实体 ID
    EntityId localPlayerEntityId = INVALID_ENTITY_ID;
    // 当前摄像机跟随的实体 ID
    EntityId cameraFollowTarget = INVALID_ENTITY_ID;
    // 是否显示调试碰撞框
    bool debugDrawCollisionBox = true;
    // 是否显示地面调试框
    bool debugDrawGroundBox = true;
};

NS_MG_END
