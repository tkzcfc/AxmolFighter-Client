#include "mugen/component/BehaviorTreeComponent.h"

#include "mugen/core/ecs/Entity.h"
#include "mugen/core/serialize/ByteBuffer.h"

NS_MG_BEGIN

BehaviorTreeComponent* BehaviorTreeComponent::of(Entity* entity)
{
    if (!entity)
        return nullptr;
    return MG_GET_COMPONENT(entity, BehaviorTreeComponent);
}

BehaviorTree* BehaviorTreeComponent::ensureTree()
{
    if (!tree)
        tree = std::make_unique<BehaviorTree>();
    return tree.get();
}

void BehaviorTreeComponent::restoreRuntimeData()
{
    if (m_pendingRuntime.empty())
        return;
    auto* t = ensureTree();
    ByteBuffer inner(m_pendingRuntime.data(), static_cast<uint32_t>(m_pendingRuntime.size()));
    t->deserialize(inner);
    m_pendingRuntime.clear();
}

void BehaviorTreeComponent::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    ByteBuffer inner;
    if (tree)
        tree->serialize(inner);
    inner.writeFinish();
    std::vector<uint8_t> blob;
    if (inner.data() && inner.len() > 0)
        blob.assign(inner.data(), inner.data() + inner.len());
    byteBuffer.writeValue(blob);
}

bool BehaviorTreeComponent::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    m_pendingRuntime.clear();
    return byteBuffer.getValue(m_pendingRuntime);
}

NS_MG_END
