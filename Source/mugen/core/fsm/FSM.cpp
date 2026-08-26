#include "FSM.h"
#include "FSMFactory.h"

using namespace std::literals;

NS_MG_BEGIN

FSM::FSM()
{
    m_preState           = nullptr;
    m_curState           = nullptr;
    m_anyState           = nullptr;
    m_userData           = nullptr;
    m_runStateTime       = 0;
    m_frameTime          = 0;
    m_deserializeContext = nullptr;
}

FSM::~FSM()
{
    reset();
}

State* FSM::addState(const std::string& stateName)
{
    if (nullptr != getStateByKey(stateName))
    {
        MG_ASSERT(false);
        return nullptr;
    }

    State* state = FSMFactory::getInstance()->spwanState(stateName);
    if (state == nullptr)
    {
        return nullptr;
    }

    state->m_fsm       = this;
    state->m_stateName = stateName;
    m_statesDict.insert(std::pair<std::string, State*>(stateName, state));

    return state;
}

bool FSM::changeToStateByName(const std::string& stateName)
{
    auto state = getStateByKey(stateName);

    if (state == nullptr)
    {
        MG_ASSERT(false);
        return false;
    }

    changeToState(state);
    return true;
}

void FSM::resetRunStateTime()
{
    m_runStateTime = 0;
}

void FSM::changeToState(State* state)
{
    if (m_anyState == state)
    {
        MG_ASSERT(false);
        return;
    }
    if (m_curState == state)
    {
        return;
    }

    if (m_curState)
    {
        MG_LOG_D("{}: exit state {}", __FUNCTION__, m_curState->getStateName());
        m_curState->_onExit();
    }

    if (m_deserializeContext == nullptr)
    {
        m_runStateTime = 0;
        m_preState     = m_curState;
    }

    m_curState = state;

    if (m_deserializeContext == nullptr)
    {
        MG_LOG_D("{}: change state to {}", __FUNCTION__, state->getStateName());
        m_curState->_onEnter();
    }
}

void FSM::setEntryStateByName(const std::string& stateName)
{
    MG_ASSERT(m_curState == nullptr);

    auto state = getStateByKey(stateName);
    if (state == nullptr)
    {
        MG_ASSERT(false);
        return;
    }

    for (auto it : m_statesDict)
    {
        if (it.second == state)
        {
            changeToState(state);
            return;
        }
    }
    MG_ASSERT(false);
}

void FSM::progressEvent(const std::string& evetName, const EventData& eventData)
{
    if (m_curState == nullptr)
    {
        MG_ASSERT(false);
        return;
    }
    m_curState->_progressEvent(evetName, eventData);

    // anyState 也接收事件，以便其转换器（如 GroundedToJump）可以跨状态响应
    if (m_anyState)
    {
        m_anyState->_progressEvent(evetName, eventData);
    }
}

void FSM::update(int32_t ms)
{
    m_frameTime = ms;
    m_runStateTime += ms;
    if (m_curState)
    {
        m_curState->_onStay();
    }

    if (m_anyState)
    {
        m_anyState->_onStay();
    }
}

void FSM::reset()
{
    if (m_anyState)
    {
        m_anyState->onExit();
    }
    if (m_curState)
    {
        m_curState->onExit();
    }
    for (auto it = m_statesDict.begin(); it != m_statesDict.end(); ++it)
    {
        delete it->second;
    }
    m_statesDict.clear();

    m_deserializeContext = nullptr;
    m_preState           = nullptr;
    m_curState           = nullptr;
    m_anyState           = nullptr;
    m_runStateTime       = 0;
    m_frameTime          = 0;
}

State* FSM::getStateByKey(const std::string& stateName) const
{
    auto it = m_statesDict.find(stateName);
    if (it == m_statesDict.end())
    {
        return nullptr;
    }
    return it->second;
}

void FSM::setAnyState(const std::string& stateName)
{
    if (m_anyState)
    {
        MG_ASSERT(false);
        return;
    }
    m_anyState = getStateByKey(stateName);
    if (!m_anyState)
    {
        MG_ASSERT(false);
        return;
    }

    if (m_deserializeContext == nullptr)
    {
        m_anyState->_onEnter();
    }
}

std::string_view FSM::getCurStateName() const
{
    if (m_curState)
    {
        return m_curState->getStateName();
    }
    return ""sv;
}

std::string_view FSM::getPreStateName() const
{
    if (m_preState)
    {
        return m_preState->getStateName();
    }
    return ""sv;
}

std::string_view FSM::getAnyStateName() const
{
    if (m_anyState)
    {
        return m_anyState->getStateName();
    }
    return ""sv;
}

void FSM::serialize(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeValue(m_runStateTime);
    byteBuffer.writeValue(m_frameTime);
    byteBuffer.writeUint16(static_cast<uint16_t>(m_statesDict.size()));
    for (auto& it : m_statesDict)
    {
        byteBuffer.writeString(it.first);
        it.second->serialize(byteBuffer);
    }
    byteBuffer.writeString(this->getAnyStateName());
    byteBuffer.writeString(this->getCurStateName());
    byteBuffer.writeString(this->getPreStateName());
}

bool FSM::deserialize(ByteBuffer& byteBuffer)
{
    reset();

    m_deserializeContext = std::make_unique<DeserializeContext>();

    if (!byteBuffer.getValue(m_runStateTime))
        return false;

    if (!byteBuffer.getValue(m_frameTime))
        return false;

    uint16_t stateCount = 0;
    if (!byteBuffer.getUint16(stateCount))
        return false;

    for (uint16_t i = 0; i < stateCount; ++i)
    {
        std::string stateName;
        if (!byteBuffer.getString(stateName))
            return false;
        State* state = addState(stateName);
        if (state == nullptr)
            return false;
        if (!state->deserialize(byteBuffer))
            return false;
    }

    // 还原任意状态
    if (!byteBuffer.getString(m_deserializeContext->anyStateName))
        return false;

    // 还原入口状态
    if (!byteBuffer.getString(m_deserializeContext->curStateName))
        return false;

    // 还原上一个状态
    if (!byteBuffer.getString(m_deserializeContext->preStateName))
        return false;

    return true;
}

void FSM::restoreRuntimeData()
{
    if (m_deserializeContext)
    {
        if (m_deserializeContext->preStateName != "")
        {
            m_preState = getStateByKey(m_deserializeContext->preStateName);
        }
        setAnyState(m_deserializeContext->anyStateName);
        setEntryStateByName(m_deserializeContext->curStateName);

        m_deserializeContext = nullptr;
    }
}

NS_MG_END
