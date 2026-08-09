#pragma once

#include "mugen/core/bt/BTAction.h"

#include <vector>

NS_MG_BEGIN

/**
 * 单段 action_attack 叶节点（对齐黑月 AttackRole）。
 * 运行时状态写 BehaviorTreeComponent / SkillCastComponent。
 */
class AttackAction : public BTAction
{
public:
    AttackAction(int32_t actionId, int32_t actionIndex, int32_t skillAttackId)
        : actionId(actionId), actionIndex(actionIndex), skillAttackId(skillAttackId)
    {
        debugName = "AttackAction";
    }

protected:
    void onActionEnter(BTContext& ctx) override;
    void onActionExit(BTContext& ctx) override;
    BTStatus onActionTick(BTContext& ctx, int32_t dtMs) override;

private:
    void applyBuffs(BTContext& ctx, bool add);
    void resetDisplacement(BTContext& ctx);
    void spawnEffect(BTContext& ctx, int32_t effectId);
    void playSounds(BTContext& ctx);

    int32_t actionId         = 0;
    int32_t actionIndex      = 0;
    int32_t skillAttackId    = 0;
    int32_t estimatedDurMs   = 5000;
    int32_t elapsedMs        = 0;
    int32_t animFinishedAtMs = -1;
    int32_t actionDelayMs    = 0;
    float frameIntervalMs    = 1000.0f / 30.0f;
    std::vector<bool> effectSpawned;
    std::vector<bool> soundsPlayed;
};

NS_MG_END
