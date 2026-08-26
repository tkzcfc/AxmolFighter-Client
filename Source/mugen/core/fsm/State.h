#pragma once

#include "EventData.h"
#include "Transition.h"

NS_MG_BEGIN

class FSM;
class State : public Object
{
public:
    typedef Object Super;

public:
    State();

    virtual ~State();

    virtual void onEnter() {}

    virtual void onExit() {}

    virtual void onEvent(const std::string& evetName, const EventData& eventData) {}

    virtual void onStay() {}

    MG_SYNTHESIZE_READONLY(FSM*, m_fsm, FSM);
    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_stateName, StateName);

public:
    Transition* addTransition(const std::string& transitionName);

    bool removeTransition(const std::string& transitionName);

    // 状态切换
    bool changeToState(const std::string& toStateName);

protected:
    void _onEnter();

    void _onExit();

    void _onStay();

    void _progressEvent(const std::string& evetName, const EventData& eventData);

    void _clear();

public:
    virtual void serialize(ByteBuffer& byteBuffer) const override;

    virtual bool deserialize(ByteBuffer& byteBuffer) override;

private:
    friend class FSM;
    std::vector<Transition*> m_transitionArr;
};

NS_MG_END
