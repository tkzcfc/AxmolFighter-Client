#include "core/event/EventDispatcher.h"

#include "AppContext.h"

#include <algorithm>

namespace game
{

EventDispatcher::~EventDispatcher()
{
    m_listeners.clear();
}

EventToken EventDispatcher::subscribe(GameEvent ev, void* owner, Handler handler)
{
    if (!handler)
        return {};
    Listener L;
    L.id      = m_nextId++;
    L.ev      = ev;
    L.owner   = owner;
    L.handler = std::move(handler);
    m_listeners.push_back(std::move(L));
    return EventToken{m_listeners.back().id};
}

void EventDispatcher::unsubscribe(EventToken token)
{
    if (!token.valid())
        return;
    m_listeners.erase(std::remove_if(m_listeners.begin(), m_listeners.end(),
                                     [token](const Listener& L) { return L.id == token.id; }),
                      m_listeners.end());
}

void EventDispatcher::unsubscribeAll(void* owner)
{
    if (!owner)
        return;
    m_listeners.erase(std::remove_if(m_listeners.begin(), m_listeners.end(),
                                     [owner](const Listener& L) { return L.owner == owner; }),
                      m_listeners.end());
}

void EventDispatcher::unsubscribe(GameEvent ev, void* owner)
{
    m_listeners.erase(std::remove_if(m_listeners.begin(), m_listeners.end(),
                                     [ev, owner](const Listener& L) {
                                         return L.ev == ev && (owner == nullptr || L.owner == owner);
                                     }),
                      m_listeners.end());
}

void EventDispatcher::dispatch(GameEvent ev)
{
    // Copy before invoke so subscribe/unsubscribe during dispatch is safe.
    std::vector<Handler> handlers;
    handlers.reserve(m_listeners.size());
    for (const auto& L : m_listeners)
    {
        if (L.ev == ev && L.handler)
            handlers.push_back(L.handler);
    }
    for (auto& h : handlers)
        h();
}

EventDispatcher* EventDispatcher::getInstance()
{
    if (!AppContext::get().eventDispatcher())
        return nullptr;
    return AppContext::get().eventDispatcher();
}

}  // namespace game
