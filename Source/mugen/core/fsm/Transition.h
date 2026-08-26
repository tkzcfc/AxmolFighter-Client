#pragma once

#include "EventData.h"
#include "mugen/core/Object.h"

NS_MG_BEGIN

class State;

class Transition : public Object
{
public:
    typedef Object Super;

public:
    Transition();

    virtual ~Transition();

protected:
    virtual bool polling();

    virtual bool progressEvent(const std::string_view evetName, const EventData& eventData);

private:
    friend class State;

    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_name, Name);
    MG_SYNTHESIZE_READONLY(State*, m_ownState, OwnState);

    MG_SYNTHESIZE_PASS_BY_REF(std::string, m_toState, ToState);

    MG_DEFINE_SERIALIZABLE(m_toState)
};

NS_MG_END
