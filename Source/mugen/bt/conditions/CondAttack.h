#pragma once

#include "mugen/core/bt/BTCondition.h"

NS_MG_BEGIN

/** 攻击总条件：有当前技能且未受击 */
class CondRoleAttack : public BTCondition
{
public:
    CondRoleAttack() { debugName = "CondRoleAttack"; }
    bool check(BTContext& ctx) override;
    void onExit(BTContext& ctx) override;
};

class CondAttackSlot : public BTCondition
{
public:
    explicit CondAttackSlot(int32_t slot) : slot(slot) { debugName = "CondAttackSlot"; }
    bool check(BTContext& ctx) override;
    void onEnter(BTContext& ctx) override;
    void onExit(BTContext& ctx) override;
    int32_t slot = 0;
};

class CondAttackStep : public BTCondition
{
public:
    CondAttackStep(int32_t slot, int32_t stepIndex) : slot(slot), stepIndex(stepIndex)
    {
        debugName = "CondAttackStep";
    }
    bool check(BTContext& ctx) override;
    void onEnter(BTContext& ctx) override;
    void onExit(BTContext& ctx) override;
    int32_t slot      = 0;
    int32_t stepIndex = 0;
};

class CondAttackPipe : public BTCondition
{
public:
    explicit CondAttackPipe(int32_t pipeIndex) : pipeIndex(pipeIndex) { debugName = "CondAttackPipe"; }
    bool check(BTContext& ctx) override;
    void onEnter(BTContext& ctx) override;
    void onExit(BTContext& ctx) override;
    int32_t pipeIndex = 0;
};

class CondAttackToward : public BTCondition
{
public:
    explicit CondAttackToward(int32_t towardIndex) : towardIndex(towardIndex)
    {
        debugName = "CondAttackToward";
    }
    bool check(BTContext& ctx) override;
    int32_t towardIndex = 1;
};

NS_MG_END
