#pragma once

#include "mugen/core/bt/BTCondition.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

class CondStatus : public BTCondition
{
public:
    typedef BTCondition Super;

public:
    CondStatus();
    explicit CondStatus(BehaviorKind kind);
    virtual ~CondStatus();

    const char* typeName() const override { return "CondStatus"; }

    bool check(BTContext& ctx) override;

public:
    BehaviorKind kind = BehaviorKind::kIdle;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END
