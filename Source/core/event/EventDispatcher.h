#pragma once

#include "core/event/GameEvents.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace game
{

struct EventToken
{
    uint64_t id = 0;
    bool valid() const { return id != 0; }
};

// Ordinary (non-Meyers) event bus. Owned by AppContext.
class EventDispatcher
{
public:
    using Handler = std::function<void()>;

    EventDispatcher() = default;
    ~EventDispatcher();

    EventDispatcher(const EventDispatcher&)            = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;

    EventToken subscribe(GameEvent ev, void* owner, Handler handler);
    void unsubscribe(EventToken token);
    void unsubscribeAll(void* owner);
    void unsubscribe(GameEvent ev, void* owner);

    void dispatch(GameEvent ev);

    // Facade → AppContext::eventDispatcher(); null if AppContext not ready.
    static EventDispatcher* getInstance();

private:
    struct Listener
    {
        uint64_t id = 0;
        GameEvent ev{};
        void* owner = nullptr;
        Handler handler;
    };

    uint64_t m_nextId = 1;
    std::vector<Listener> m_listeners;
};

}  // namespace game
