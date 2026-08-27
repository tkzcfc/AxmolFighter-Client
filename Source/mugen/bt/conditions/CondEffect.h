#pragma once

#include "mugen/core/bt/BTAction.h"
#include "mugen/core/bt/BTCondition.h"

NS_MG_BEGIN

// 特效仍在寿命内（lifetimeMs<=0 视为不超时）
class CondEffectAlive : public BTCondition
{
public:
    CondEffectAlive();
    virtual ~CondEffectAlive();
    const char* typeName() const override { return "CondEffectAlive"; }
    bool check(BTContext& ctx) override;
};

// 表 action_ids 非空时走攻击叶
class CondEffectAttack : public BTCondition
{
public:
    CondEffectAttack();
    virtual ~CondEffectAttack();
    const char* typeName() const override { return "CondEffectAttack"; }
    bool check(BTContext& ctx) override;
};

// 占位叶：寿命未到则一直 Running
class EffectHoldAction : public BTAction
{
public:
    EffectHoldAction();
    virtual ~EffectHoldAction();
    const char* typeName() const override { return "EffectHoldAction"; }

protected:
    void onActionEnter(BTContext& /*ctx*/) override {}
    BTStatus onActionUpdate(BTContext& /*ctx*/, int32_t /*dtMs*/) override { return BTStatus::Running; }
};

NS_MG_END
