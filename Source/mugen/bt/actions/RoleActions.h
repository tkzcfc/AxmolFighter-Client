#pragma once

#include "mugen/core/bt/BTAction.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

class LocomoAction : public BTAction
{
public:
    explicit LocomoAction(BehaviorKind kind) : kind(kind)
    {
        debugName = "LocomoAction";
    }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionTick(BTContext& ctx, int32_t dtMs) override;

    BehaviorKind kind;
};

class HitReactAction : public BTAction
{
public:
    HitReactAction() { debugName = "HitReactAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionTick(BTContext& ctx, int32_t dtMs) override;
};

class DeathAction : public BTAction
{
public:
    DeathAction() { debugName = "DeathAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionTick(BTContext& ctx, int32_t dtMs) override;
};

/** 无技能槽时的 Attack 占位叶：保持 Running，避免 Attack 条件通过却无子节点 */
class HoldAttackAction : public BTAction
{
public:
    HoldAttackAction() { debugName = "HoldAttackAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionTick(BTContext& ctx, int32_t dtMs) override;
};

NS_MG_END
