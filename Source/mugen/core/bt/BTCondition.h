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

protected:
    bool onEnter(BTContext& /*ctx*/) override { return true; }

    BTStatus onUpdate(BTContext& /*ctx*/, int32_t /*dtMs*/) override { return BTStatus::Running; }
};

NS_MG_END
