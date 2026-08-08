#include "net/NetAgent.h"

#include "AppContext.h"
#include "net/NetClient.h"

namespace net
{

namespace
{

std::string formatNetCallbackKey(void* target)
{
    return fmt::format("NetAgent_{}", reinterpret_cast<uintptr_t>(target));
}

}  // namespace

NetAgent::NetAgent() : m_lifetime(std::make_shared<LifetimeToken>())
{
    if (auto* netClient = AppContext::get().netClient())
    {
        netClient->listenRecv([this](uint8_t cmd, uint16_t msgId, int32_t serial, const std::string_view& payload) {
            if (cmd == CMD_BUSINESS && serial == 0)
            {
                dispatchPush(msgId, payload);
            }
        }, formatNetCallbackKey(this));
    }
}

NetAgent::~NetAgent()
{
    if (m_lifetime)
        m_lifetime->alive = false;
    detach();
}

int32_t NetAgent::request(uint16_t msgId,
                          const char* data,
                          size_t length,
                          const std::function<void(bool, uint16_t, const std::string_view&)>& callback,
                          float timeout)
{
    if (auto* client = AppContext::get().netClient())
    {
        return client->request(msgId, data, length, callback, this, timeout);
    }
    return 0;
}

void NetAgent::detach()
{
    if (m_lifetime)
        m_lifetime->alive = false;

    if (auto* client = AppContext::get().netClient())
    {
        client->cancelByTarget(this);
        client->unlistenRecv(formatNetCallbackKey(this));
    }
    m_pushListeners.clear();
}

void NetAgent::push(uint16_t msgId, const char* data, size_t length)
{
    if (auto* client = AppContext::get().netClient())
    {
        client->push(msgId, data, length);
    }
}

void NetAgent::dispatchPush(uint16_t msgId, const std::string_view& payload)
{
    // 先持有 lifetime 与 listener 拷贝：回调内可能 switchView 销毁 this，
    // 并 clear m_pushListeners（不能边跑边销毁正在执行的 std::function）。
    auto life = m_lifetime;
    std::function<void(const std::string_view&)> listener;
    if (auto it = m_pushListeners.find(msgId); it != m_pushListeners.end())
        listener = it->second;

    if (listener)
        listener(payload);

    // 这儿可能已经被销毁了，不能再访问成员变量了。
    if (!life || !life->alive)
        return;

    onPush(msgId, payload);
}

void NetAgent::onPush(uint16_t msgId, const std::string_view& payload) {}

}  // namespace net
