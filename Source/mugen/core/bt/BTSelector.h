#pragma once

#include "mugen/core/bt/BTComposite.h"

NS_MG_BEGIN

/** 优先级 Selector：粘滞当前子节点；条件复检失败则换枝 */
class BTSelector : public BTComposite
{
public:
    typedef BTComposite Super;

    BTSelector() { debugName = "Selector"; }
    virtual ~BTSelector() {}

    void onEnter(BTContext& ctx) override;
    void onExit(BTContext& ctx) override;
    BTStatus tick(BTContext& ctx, int32_t dtMs) override;
};

NS_MG_END
