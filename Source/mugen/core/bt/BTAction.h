#pragma once

#include "mugen/core/bt/BTNode.h"

NS_MG_BEGIN

class BTAction : public BTNode
{
public:
    typedef BTNode Super;

    BTAction() {}
    virtual ~BTAction() {}

    void onEnter(BTContext& ctx) override;
    void onExit(BTContext& ctx) override;
    BTStatus tick(BTContext& ctx, int32_t dtMs) final;

protected:
    virtual void onActionEnter(BTContext& /*ctx*/) {}
    virtual void onActionExit(BTContext& /*ctx*/) {}
    virtual BTStatus onActionTick(BTContext& ctx, int32_t dtMs) = 0;

    bool entered = false;
};

NS_MG_END
