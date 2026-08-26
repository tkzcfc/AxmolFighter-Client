#include "mugen/core/bt/BehaviorTree.h"

#include "mugen/core/bt/BTContext.h"
#include "mugen/core/bt/BTFactory.h"
#include "mugen/core/serialize/ByteBuffer.h"

NS_MG_BEGIN

BehaviorTree::BehaviorTree() : m_root(nullptr) {}

BehaviorTree::~BehaviorTree()
{
    destroyRoot();
}

bool BehaviorTree::enter(BTContext& ctx)
{
    if (!m_root)
        return false;
    return m_root->enter(ctx);
}

bool BehaviorTree::update(BTContext& ctx, int32_t dtMs)
{
    if (!m_root)
        return false;
    m_root->update(ctx, dtMs);
    return m_root->isRunning();
}

void BehaviorTree::exit(BTContext& ctx)
{
    if (m_root)
        m_root->exit(ctx);
}

void BehaviorTree::setRoot(BTNode* node)
{
    if (m_root != node)
    {
        destroyRoot();
        m_root = node;
        if (m_root)
            m_root->m_parent = nullptr;
    }
}

void BehaviorTree::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    if (!m_root)
    {
        byteBuffer.writeString(std::string());
        return;
    }
    byteBuffer.writeString(std::string(m_root->typeName()));
    m_root->serialize(byteBuffer);
}

bool BehaviorTree::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    destroyRoot();

    std::string name;
    if (!byteBuffer.getString(name))
        return false;
    if (name.empty())
        return true;
    m_root = BTFactory::getInstance()->spawnNode(name);
    if (!m_root || !m_root->deserialize(byteBuffer))
    {
        destroyRoot();
        return false;
    }
    return true;
}

void BehaviorTree::destroyRoot()
{
    if (m_root)
    {
        delete m_root;
        m_root = nullptr;
    }
}

NS_MG_END
