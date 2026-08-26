#include "mugen/core/bt/BehaviorTree.h"

#include "mugen/core/bt/BTContext.h"
#include "mugen/core/bt/BTFactory.h"
#include "mugen/core/serialize/ByteBuffer.h"

NS_MG_BEGIN

BehaviorTree::BehaviorTree() : m_root(nullptr) {}

BehaviorTree::~BehaviorTree()
{
    delete m_root;
    m_root = nullptr;
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
    if (m_root == node)
        return;
    delete m_root;
    m_root = node;
    if (m_root)
        m_root->parent = nullptr;
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
    if (m_root)
    {
        delete m_root;
        m_root = nullptr;
    }

    std::string name;
    if (!byteBuffer.getString(name))
        return false;
    if (name.empty())
        return true;
    m_root = BTFactory::getInstance()->spawnNode(name);
    if (!m_root || !m_root->deserialize(byteBuffer))
    {
        delete m_root;
        m_root = nullptr;
        return false;
    }
    m_root->parent = nullptr;
    return true;
}

NS_MG_END
