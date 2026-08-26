#pragma once

#include "mugen/core/bt/BTComposite.h"

#include <vector>

NS_MG_BEGIN

// 并行节点
// 所有子节点同时执行，直到所有子节点都执行完毕
class BTParallel : public BTComposite
{
public:
    typedef BTComposite Super;

public:
    BTParallel();

    virtual ~BTParallel();

    const char* typeName() const override { return "BTParallel"; }

protected:
    bool onEnter(BTContext& ctx) override;

    void onExit(BTContext& ctx) override;

    BTStatus onUpdate(BTContext& ctx, int32_t dtMs) override;

private:
    BTStatus updateParallel(BTContext& ctx, int32_t dtMs);

    std::vector<uint32_t> m_running;

public:
    MG_DEFINE_SERIALIZABLE(m_running)
};

NS_MG_END
