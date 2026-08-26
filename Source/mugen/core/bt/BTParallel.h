#pragma once

#include "mugen/core/bt/BTComposite.h"

#include <vector>

NS_MG_BEGIN

class BTParallel : public BTComposite
{
public:
    typedef BTComposite Super;

public:
    BTParallel();

    virtual ~BTParallel();

    const char* typeName() const override { return "BTParallel"; }

    void enter(BTContext& ctx) override;

    void exit(BTContext& ctx) override;

    void update(BTContext& ctx, int32_t dtMs) override;

private:
    void updateParallel(BTContext& ctx, int32_t dtMs);

    std::vector<uint32_t> m_running;

public:
    MG_DEFINE_SERIALIZABLE(m_running)
};

NS_MG_END
