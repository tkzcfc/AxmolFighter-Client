#pragma once

#include "mugen/core/bt/BTComposite.h"

NS_MG_BEGIN

// 顺序节点
// 按顺序执行子节点，直到某个子节点失败或运行中
class BTSequence : public BTComposite
{
public:
    typedef BTComposite Super;

public:
    BTSequence();

    virtual ~BTSequence();

    const char* typeName() const override { return "BTSequence"; }

protected:
    bool onEnter(BTContext& ctx) override;

    void onExit(BTContext& ctx) override;

    BTStatus onUpdate(BTContext& ctx, int32_t dtMs) override;

private:
    BTStatus updateSequence(BTContext& ctx, int32_t dtMs);

    MG_SYNTHESIZE_READONLY(int32_t, m_currentIndex, CurrentIndex);

public:
    MG_DEFINE_SERIALIZABLE(m_currentIndex)
};

NS_MG_END
