#pragma once

#include "mugen/core/Object.h"
#include "mugen/core/bt/BTNode.h"

#include <memory>
#include <vector>

NS_MG_BEGIN

struct BTContext;
class Entity;

// 行为树壳：拥有 root、驱动 update / forceExit，并序列化运行时 blob。
class BehaviorTree : public Object
{
public:
    typedef Object Super;

public:
    BehaviorTree();

    virtual ~BehaviorTree();

    const char* typeName() const { return "BehaviorTree"; }

    static BehaviorTree* of(Entity* entity);

    void update(BTContext& ctx, int32_t dtMs);

    void forceExit(BTContext& ctx);

    void restoreRuntimeData();

    void setRoot(std::unique_ptr<BTNode> node);

private:
    std::vector<uint8_t> m_pendingRuntime;

public:
    std::unique_ptr<BTNode> root;
    BTNode* attackSelector = nullptr;
    bool cityMode          = false;
    int32_t treeKind       = 0;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl, cityMode, treeKind)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END
