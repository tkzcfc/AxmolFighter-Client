#pragma once

#include "mugen/core/StdC.h"

NS_MG_BEGIN

class Entity;
class ECSManager;

// 每 tick 传入的瘦上下文。节点不持有 Entity*，业务叶子自行取组件。
struct BTContext
{
    // 当前行为树所属实体
    Entity* entity        = nullptr;
    // 所属 ECS（特效生成、空间查询）
    ECSManager* ecs       = nullptr;
    // 本帧间隔（毫秒）
    int32_t dtMs          = 0;
    // 世界累计运行时间（毫秒）
    int64_t runningTimeMs = 0;
};

NS_MG_END
