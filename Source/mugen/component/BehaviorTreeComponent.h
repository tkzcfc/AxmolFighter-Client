#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/core/bt/BTNode.h"
#include "mugen/conf/GameDef.h"

#include <memory>
#include <vector>

NS_MG_BEGIN

class BehaviorTreeComponent : public Component
{
public:
    typedef Component Super;

    BehaviorTreeComponent() {}
    virtual ~BehaviorTreeComponent() {}

    /** 树结构（不序列化；spawn/反序列化后由 RoleTreeBuilder 重建） */
    std::unique_ptr<BTNode> root;

    /** Attack 空 Selector 指针（Phase 1.2 SkillTreeBuilder 灌技能子树用；不序列化） */
    BTNode* attackSelector = nullptr;

    /** 下一可用 memory 槽（建树时递增分配） */
    int32_t nextMemorySlot = 0;

    // —— 可序列化运行时状态 ——
    int32_t activeBranchKind = static_cast<int32_t>(BehaviorKind::kIdle);
    std::vector<int8_t> selectorMemory;
    int32_t currentActionId   = 0;
    int32_t actionElapsedMs   = 0;
    uint32_t effectSpawnMask  = 0;
    bool animationEnd         = false;

    int32_t allocMemorySlot() { return nextMemorySlot++; }

    MG_DEFINE_SERIALIZABLE(activeBranchKind,
                           selectorMemory,
                           currentActionId,
                           actionElapsedMs,
                           effectSpawnMask,
                           animationEnd);
};

NS_MG_END
