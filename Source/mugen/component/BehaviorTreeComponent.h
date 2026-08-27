#pragma once

#include "mugen/core/bt/BehaviorTree.h"
#include "mugen/core/ecs/Component.h"

#include <memory>
#include <vector>

NS_MG_BEGIN

class Entity;

class BehaviorTreeComponent : public Component
{
public:
    typedef Component Super;

public:
    BehaviorTreeComponent() {}

    virtual ~BehaviorTreeComponent() {}

    static BehaviorTreeComponent* of(Entity* entity);

    BehaviorTree* ensureTree();

    void restoreRuntimeData();

private:
    std::vector<uint8_t> m_pendingRuntime;

public:
    std::unique_ptr<BehaviorTree> tree;
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
