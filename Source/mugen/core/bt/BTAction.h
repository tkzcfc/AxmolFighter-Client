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

    void enter(BTContext& ctx) override;

    void exit(BTContext& ctx) override;

    void update(BTContext& ctx, int32_t dtMs) final;

protected:
    virtual void onActionEnter(BTContext& /*ctx*/) {}

    virtual void onActionExit(BTContext& /*ctx*/) {}

    virtual void onActionUpdate(BTContext& ctx, int32_t dtMs) = 0;
};

NS_MG_END
