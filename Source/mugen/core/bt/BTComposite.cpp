#include "mugen/core/bt/BTComposite.h"
#include "mugen/core/bt/BTContext.h"
#include "mugen/core/bt/BTFactory.h"
#include "mugen/core/StdC.h"

NS_MG_BEGIN

BTComposite::BTComposite() {}

BTComposite::~BTComposite()
{
    clearConditions();
    clearChildren();
}

void BTComposite::addCondition(BTCondition* cond)
{
    cond->parent = this;
    m_conditions.push_back(cond);
}

void BTComposite::addChild(BTNode* child)
{
    child->parent = this;
    m_children.push_back(child);
}

void BTComposite::clearConditions()
{
    for (BTCondition* cond : m_conditions)
        delete cond;
    m_conditions.clear();
}

void BTComposite::clearChildren()
{
    for (BTNode* child : m_children)
        delete child;
    m_children.clear();
}

bool BTComposite::check(BTContext& ctx)
{
    for (BTCondition* cond : m_conditions)
    {
        if (!cond->check(ctx))
            return false;
        cond->enter(ctx);
    }
    return true;
}

bool BTComposite::conditionsHold(BTContext& ctx)
{
    for (BTCondition* cond : m_conditions)
    {
        if (!cond->check(ctx))
            return false;
    }
    return true;
}

void BTComposite::exit(BTContext& ctx)
{
    for (auto it = m_conditions.rbegin(); it != m_conditions.rend(); ++it)
    {
        if ((*it)->status == BTStatus::Running)
            (*it)->exit(ctx);
    }
}

void BTComposite::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeUint16(static_cast<uint16_t>(m_conditions.size()));
    for (BTCondition* cond : m_conditions)
    {
        byteBuffer.writeString(std::string(cond->typeName()));
        cond->serialize(byteBuffer);
    }
    byteBuffer.writeUint16(static_cast<uint16_t>(m_children.size()));
    for (BTNode* child : m_children)
    {
        byteBuffer.writeString(std::string(child->typeName()));
        child->serialize(byteBuffer);
    }
}

bool BTComposite::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    clearConditions();
    clearChildren();

    uint16_t condCount = 0;
    if (!byteBuffer.getUint16(condCount))
        return false;
    for (uint16_t i = 0; i < condCount; ++i)
    {
        std::string name;
        if (!byteBuffer.getString(name))
            return false;
        BTCondition* cond = BTFactory::getInstance()->spawnCondition(name);
        if (!cond || !cond->deserialize(byteBuffer))
        {
            delete cond;
            return false;
        }
        addCondition(cond);
    }

    uint16_t childCount = 0;
    if (!byteBuffer.getUint16(childCount))
        return false;
    for (uint16_t i = 0; i < childCount; ++i)
    {
        std::string name;
        if (!byteBuffer.getString(name))
            return false;
        BTNode* child = BTFactory::getInstance()->spawnNode(name);
        if (!child || !child->deserialize(byteBuffer))
        {
            delete child;
            return false;
        }
        addChild(child);
    }
    return true;
}

NS_MG_END
