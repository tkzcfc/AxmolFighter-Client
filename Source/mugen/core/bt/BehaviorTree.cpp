#include "mugen/core/bt/BehaviorTree.h"

#include "mugen/component/BehaviorTreeComponent.h"
#include "mugen/core/bt/BTComposite.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/serialize/ByteBuffer.h"

#include <cstring>

NS_MG_BEGIN

BehaviorTree::BehaviorTree() {}

BehaviorTree::~BehaviorTree() {}

BehaviorTree* BehaviorTree::of(Entity* entity)
{
    if (!entity)
        return nullptr;
    auto* comp = MG_GET_COMPONENT(entity, BehaviorTreeComponent);
    return comp ? comp->ensureTree() : nullptr;
}

void BehaviorTree::update(BTContext& ctx, int32_t dtMs)
{
    if (!root)
        return;
    if (root->status == BTStatus::Running)
        root->update(ctx, dtMs);
    if (root->status == BTStatus::Running)
        return;
    root->enter(ctx);
}

void BehaviorTree::forceExit(BTContext& ctx)
{
    if (root)
        root->exit(ctx);
}

void BehaviorTree::setRoot(std::unique_ptr<BTNode> node)
{
    root = std::move(node);
    if (root)
        root->parent = nullptr;
}

void BehaviorTree::restoreRuntimeData()
{
    if (!root || m_pendingRuntime.empty())
    {
        m_pendingRuntime.clear();
        return;
    }
    ByteBuffer inner(m_pendingRuntime.data(), static_cast<uint32_t>(m_pendingRuntime.size()));
    root->deserialize(inner);
    root->parent = nullptr;
    m_pendingRuntime.clear();

    attackSelector = nullptr;
    auto* rootComp = dynamic_cast<BTComposite*>(root.get());
    for (BTNode* child : rootComp->getChildren())
    {
        auto* comp = dynamic_cast<BTComposite*>(child);
        if (!comp)
            continue;
        for (BTCondition* cond : comp->getConditions())
        {
            if (std::strcmp(cond->typeName(), "CondRoleAttack") != 0)
                continue;
            attackSelector = child;
            return;
        }
    }
}

void BehaviorTree::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    ByteBuffer inner;
    if (root)
        root->serialize(inner);
    inner.writeFinish();
    std::vector<uint8_t> blob;
    if (inner.data() && inner.len() > 0)
        blob.assign(inner.data(), inner.data() + inner.len());
    byteBuffer.writeValue(blob);
}

bool BehaviorTree::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    m_pendingRuntime.clear();
    return byteBuffer.getValue(m_pendingRuntime);
}

NS_MG_END
