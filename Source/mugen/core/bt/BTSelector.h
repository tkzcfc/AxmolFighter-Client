#pragma once

#include "mugen/core/bt/BTComposite.h"

NS_MG_BEGIN

// 选择器节点
// 先判断所有条件是否成立
// 如果条件不成立,则打断当前正在运行的子节点,并将 Selector 状态设置为 Success (即选择器认为任务已成功完成)
// 如果成立,则继续执行当前节点直到他完成,再从当前索引开始遍历子节点(调用enter尝试进入节点)
// 只有 A：A 成功结束，自己也成功结束，不会再进 A。
// 有 A、B、C：A 成功后试 B、C（不试 A）；B 能跑就换成 B。B 再成功则试 C、A（不试 B）。
// 如果有两个子节点一直enter成功则会一直轮流执行
class BTSelector : public BTComposite
{
public:
    typedef BTComposite Super;

public:
    BTSelector();

    virtual ~BTSelector();

    const char* typeName() const override { return "BTSelector"; }

protected:
    bool onEnter(BTContext& ctx) override;

    void onExit(BTContext& ctx) override;

    BTStatus onUpdate(BTContext& ctx, int32_t dtMs) override;

private:
    BTStatus updateSelector(BTContext& ctx, int32_t dtMs);

    MG_SYNTHESIZE_READONLY(int32_t, m_currentIndex, CurrentIndex);

public:
    MG_DEFINE_SERIALIZABLE(m_currentIndex)
};

NS_MG_END
