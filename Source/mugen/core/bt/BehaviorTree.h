#pragma once

#include "mugen/core/Object.h"
#include "mugen/core/bt/BTNode.h"

NS_MG_BEGIN

struct BTContext;

class BehaviorTree : public Object
{
public:
    typedef Object Super;

public:
    BehaviorTree();

    virtual ~BehaviorTree();

    bool enter(BTContext& ctx);

    bool update(BTContext& ctx, int32_t dtMs);

    void exit(BTContext& ctx);

    void setRoot(BTNode* node);

private:
    void destroyRoot();

public:
    MG_SYNTHESIZE_READONLY(BTNode*, m_root, Root)

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END
