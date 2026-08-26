#include "FSMFactory.h"

NS_MG_BEGIN

FSMFactory* FSMFactory::s_instance = nullptr;

FSMFactory::FSMFactory() {}

FSMFactory::~FSMFactory() {}

FSMFactory* FSMFactory::getInstance()
{
    if (s_instance == nullptr)
    {
        s_instance = new FSMFactory();
    }
    return s_instance;
}

void FSMFactory::destroyInstance()
{
    if (s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

void FSMFactory::registerState(const std::string& name, const StateCreateFuncType& createFunc)
{
    m_statesCreaterMap[name] = createFunc;
}

bool FSMFactory::hasState(const std::string& name) const
{
    return m_statesCreaterMap.find(name) != m_statesCreaterMap.end();
}

void FSMFactory::registerTransition(const std::string& name, const TransitionCreateFuncType& createFunc)
{
    m_transitionsCreaterMap[name] = createFunc;
}

State* FSMFactory::spwanState(const std::string& stateName)
{
    State* state = nullptr;
    auto iter    = m_statesCreaterMap.find(stateName);
    if (iter != m_statesCreaterMap.end())
    {
        state = iter->second();
    }
    return state;
}

Transition* FSMFactory::spwanTransition(const std::string& transitionName)
{
    Transition* transition = nullptr;
    auto iter              = m_transitionsCreaterMap.find(transitionName);
    if (iter != m_transitionsCreaterMap.end())
    {
        transition = iter->second();
    }

    if (transition == nullptr)
    {
        MG_LOG_E("can't find Transition with name: {}", transitionName);
        MG_ASSERT(false);
    }

    return transition;
}

NS_MG_END
