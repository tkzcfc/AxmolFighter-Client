#include "mugen/core/bt/BTFactory.h"
#include "mugen/core/StdC.h"

NS_MG_BEGIN

BTFactory* BTFactory::s_instance = nullptr;

BTFactory::BTFactory() {}

BTFactory::~BTFactory() {}

BTFactory* BTFactory::getInstance()
{
    if (s_instance == nullptr)
        s_instance = new BTFactory();
    return s_instance;
}

void BTFactory::destroyInstance()
{
    if (s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

void BTFactory::registerNode(const std::string& name, const BTNodeCreateFunc& createFunc)
{
    m_nodeCreators[name] = createFunc;
}

void BTFactory::registerCondition(const std::string& name, const BTConditionCreateFunc& createFunc)
{
    m_conditionCreators[name] = createFunc;
}

bool BTFactory::hasNode(const std::string& name) const
{
    return m_nodeCreators.find(name) != m_nodeCreators.end();
}

bool BTFactory::hasCondition(const std::string& name) const
{
    return m_conditionCreators.find(name) != m_conditionCreators.end();
}

BTNode* BTFactory::spawnNode(const std::string& name)
{
    auto iter = m_nodeCreators.find(name);
    if (iter == m_nodeCreators.end())
    {
        MG_LOG_E("BTFactory: unknown node '{}'", name);
        MG_ASSERT(false);
        return nullptr;
    }
    return iter->second();
}

BTCondition* BTFactory::spawnCondition(const std::string& name)
{
    auto iter = m_conditionCreators.find(name);
    if (iter == m_conditionCreators.end())
    {
        MG_LOG_E("BTFactory: unknown condition '{}'", name);
        MG_ASSERT(false);
        return nullptr;
    }
    return iter->second();
}

NS_MG_END
