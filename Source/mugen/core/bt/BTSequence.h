#pragma once

#include "mugen/core/bt/BTComposite.h"

NS_MG_BEGIN

/** 顺序 Sequence：子节点依次 Success；条件复检失败则整体 Failure */
class BTSequence : public BTComposite
{
public:
    typedef BTComposite Super;

    BTSequence() { debugName = "Sequence"; }
    virtual ~BTSequence() {}

    void onEnter(BTContext& ctx) override;
    void onExit(BTContext& ctx) override;
    BTStatus tick(BTContext& ctx, int32_t dtMs) override;
};

NS_MG_END
