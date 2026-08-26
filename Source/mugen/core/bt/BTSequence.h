#pragma once

#include "mugen/core/bt/BTComposite.h"

NS_MG_BEGIN

class BTSequence : public BTComposite
{
public:
    typedef BTComposite Super;

public:
    BTSequence();

    virtual ~BTSequence();

    const char* typeName() const override { return "BTSequence"; }

    void enter(BTContext& ctx) override;

    void exit(BTContext& ctx) override;

    void update(BTContext& ctx, int32_t dtMs) override;

private:
    void updateSequence(BTContext& ctx, int32_t dtMs);

    int8_t m_currentIndex = -1;

public:
    MG_DEFINE_SERIALIZABLE(m_currentIndex)
};

NS_MG_END
