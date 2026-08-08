#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

// 标识组件,存放身份标识（ID、名称、阵营等）
class IdentityComponent : public Component
{
public:
    typedef Component Super;

public:
    IdentityComponent() {}
    virtual ~IdentityComponent() {}

    EntityCategory category = kMonster;  // 实体类别（玩家/怪物/技能效果）
    JobType job = JobType::kUnknown;
    int64_t playerId = 0;
    std::string name;

    // 运行时怪物阵营属性（不进序列化）
    int32_t monsterCamps = 0;

    MG_DEFINE_SERIALIZABLE(category, job, playerId, name);
};

NS_MG_END
