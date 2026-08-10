#pragma once

#include "mugen/core/bt/BTCondition.h"

NS_MG_BEGIN

class CondPatrol : public BTCondition
{
public:
    CondPatrol() { debugName = "CondPatrol"; }
    bool check(BTContext& ctx) override;
};

class CondAlert : public BTCondition
{
public:
    CondAlert() { debugName = "CondAlert"; }
    bool check(BTContext& ctx) override;
};

class CondChase : public BTCondition
{
public:
    CondChase() { debugName = "CondChase"; }
    bool check(BTContext& ctx) override;
};

class CondJostled : public BTCondition
{
public:
    CondJostled() { debugName = "CondJostled"; }
    bool check(BTContext& ctx) override;
};

NS_MG_END
