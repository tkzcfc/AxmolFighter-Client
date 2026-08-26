#include "Transition.h"
#include "State.h"

#include <charconv>

NS_MG_BEGIN

Transition::Transition()
{
    m_toState  = "";
    m_ownState = nullptr;
}

Transition::~Transition() {}

bool Transition::polling()
{
    return false;
}

bool Transition::progressEvent(const std::string_view evetName, const EventData& eventData)
{
    return false;
}

NS_MG_END
