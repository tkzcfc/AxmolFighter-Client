#pragma once

#include "mugen/core/bt/BTNode.h"

NS_MG_BEGIN

class BTAction : public BTNode
{
public:
    typedef BTNode Super;

public:
    BTAction();

    virtual ~BTAction();

protected:
    bool onEnter(BTContext& ctx) final;

    void onExit(BTContext& ctx) final;

    BTStatus onUpdate(BTContext& ctx, int32_t dtMs) final;

    virtual void onActionEnter(BTContext& /*ctx*/) {}

    virtual void onActionExit(BTContext& /*ctx*/) {}

    virtual BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) = 0;
};

NS_MG_END
