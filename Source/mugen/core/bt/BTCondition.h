#pragma once

#include "mugen/core/bt/BTNode.h"

NS_MG_BEGIN

class BTCondition : public BTNode
{
public:
    typedef BTNode Super;

public:
    BTCondition();

    virtual ~BTCondition();

    virtual bool check(BTContext& ctx) = 0;

    void enter(BTContext& ctx) override;

    void exit(BTContext& ctx) override;

protected:
    virtual void onEnter(BTContext& /*ctx*/) {}

    virtual void onExit(BTContext& /*ctx*/) {}
};

NS_MG_END
