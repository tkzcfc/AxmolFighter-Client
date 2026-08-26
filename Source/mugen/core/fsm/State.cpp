#include "State.h"
#include "FSM.h"
#include "FSMFactory.h"

NS_MG_BEGIN

State::State() : m_fsm(nullptr) {}

State::~State()
{
    _clear();
}

Transition* State::addTransition(const std::string& transitionName)
{
    auto Transition = FSMFactory::getInstance()->spwanTransition(transitionName);
    if (Transition == nullptr)
    {
        return nullptr;
    }

    Transition->m_name     = std::string(transitionName);
    Transition->m_ownState = this;
    m_transitionArr.push_back(Transition);
    return Transition;
}

bool State::removeTransition(const std::string& transitionName)
{
    Transition* Transition = nullptr;
    for (auto it = m_transitionArr.begin(); it != m_transitionArr.end(); ++it)
    {
        Transition = *it;
        if (Transition->getName() == transitionName)
        {
            delete Transition;
            m_transitionArr.erase(it);
            return true;
        }
    }
    return false;
}

void State::_clear()
{
    for (auto& it : m_transitionArr)
    {
        delete it;
    }
    m_transitionArr.clear();
}

bool State::changeToState(const std::string& toStateName)
{
    return m_fsm->changeToStateByName(toStateName);
}

void State::_onEnter()
{
    this->onEnter();
}

void State::_onExit()
{
    this->onExit();
}

void State::_onStay()
{
    if (m_transitionArr.empty())
    {
        onStay();
    }
    else
    {
        for (auto it : m_transitionArr)
        {
            if (it->polling() && this->changeToState(it->getToState()))
            {
                return;
            }
        }
        onStay();
    }
}

void State::_progressEvent(const std::string& evetName, const EventData& eventData)
{
    this->onEvent(evetName, eventData);

    if (m_transitionArr.empty())
        return;

    for (auto it : m_transitionArr)
    {
        if (it->progressEvent(evetName, eventData) && this->changeToState(it->getToState()))
        {
            return;
        }
    }
}

void State::serialize(ByteBuffer& byteBuffer) const
{
    Super::serialize(byteBuffer);
    byteBuffer.writeByte(static_cast<uint8_t>(m_transitionArr.size()));
    for (const auto& it : m_transitionArr)
    {
        byteBuffer.writeString(it->getName());
        it->serialize(byteBuffer);
    }
}

bool State::deserialize(ByteBuffer& byteBuffer)
{
    if (!Super::deserialize(byteBuffer))
    {
        return false;
    }

    uint8_t TransitionCount = 0;
    if (!byteBuffer.getByte(TransitionCount))
    {
        return false;
    }

    std::string TransitionName;
    for (uint8_t i = 0; i < TransitionCount; ++i)
    {
        if (!byteBuffer.getString(TransitionName))
            return false;

        Transition* Transition = this->addTransition(TransitionName);
        if (Transition == nullptr)
        {
            return false;
        }

        if (!Transition->deserialize(byteBuffer))
        {
            return false;
        }
    }

    return true;
}

NS_MG_END
