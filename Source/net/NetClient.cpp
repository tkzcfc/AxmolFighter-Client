#include "NetClient.h"
#include "NetErr.h"
#include "NetLog.h"
#include "gateway_client.pb.h"
#include <chrono>
#include <algorithm>
#include <cstring>
#include "yasio/ibstream.hpp"

namespace net
{

/// 客户端发了网关不接受的 cmd，比如直接发内部控制命令。
const int32_t GATEWAY_ERR_INVALID_CMD = 1;
/// msg_id 能找到目标服务类型，但当前没有可用的服务实例。
const int32_t GATEWAY_ERR_SERVICE_UNAVAILABLE = 2;
/// 这个消息需要先绑定服务实例，但当前 session 还没绑定。
const int32_t GATEWAY_ERR_SERVICE_NOT_BOUND = 3;
/// msg_id 没命中 gateway.toml 里的任何路由范围。
const int32_t GATEWAY_ERR_UNKNOWN_ROUTE = 4;
/// 客户端还没完成网关认证。
const int32_t GATEWAY_ERR_UNAUTHENTICATED = 5;

namespace
{
// 默认请求回调
void defaultCallback(bool, uint16_t, const std::string_view&) {}

#ifdef AX_PLATFORM_PC
uint64_t now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
static uint64_t lastTime = now();
#endif
}  // namespace

NetClient::NetClient(int maxChannelCount)
    : m_port(0)
    , m_connectionId(-1)
    , m_state(ConnState::Disconnected)
    , m_destroying(false)
    , m_nextSerial(0)
    , m_recvListenersDirty(false)
    , m_connectCallback(nullptr)
    , m_disconnectCallback(nullptr)
    , m_statusChangeCallback(nullptr)
{
    m_transport = std::make_unique<YasioClient>(maxChannelCount);
    m_transport->setOnEvent(
        [this](int eventType, int id, const std::string_view& data) { this->handleEvent(eventType, id, data); });

    // 超时检测定时器（每 0.5 秒）
    ax::Director::getInstance()->getScheduler()->schedule([this](float dt) { this->checkTimeouts(dt); }, this, 0.5f,
                                                          false, "NetClient_timeout");
}

NetClient::~NetClient()
{
    m_destroying = true;
    ax::Director::getInstance()->getScheduler()->unschedule("NetClient_timeout", this);
    disconnect();
    m_transport = nullptr;
}

void NetClient::setHost(const std::string& host, int port)
{
    m_host = host;
    m_port = port;
}

void NetClient::connect(const ConnectCallback& callback)
{
    if (m_state != ConnState::Disconnected)
    {
        NET_LOGI("already connected or connecting, ignoring connect() call");

        if (callback)
        {
            callback(false, "already connected or connecting"sv);
        }
        return;
    }

    m_connectCallback = callback;
    m_state           = ConnState::Connecting;
    m_nextSerial      = 0;

    NET_LOGI("connecting to {}:{}...", m_host, m_port);
    m_transport->connect(m_host, m_port, yasio::YCK_TCP_CLIENT);
}

void NetClient::disconnect()
{
    NET_LOGI("disconnecting...");
    if (m_connectionId != -1)
    {
        m_transport->disconnect(m_connectionId);
        m_connectionId = -1;
    }
    m_state = ConnState::Disconnected;
    m_serviceStatuses.clear();

    // 通知所有未完成的请求
    failPending(NET_ERR_DISCONNECTED);
}

void NetClient::failPending(const std::string_view& error)
{
    if (m_pendingRequests.empty())
        return;

    m_tempCallbacks.clear();
    m_tempCallbacks.reserve(m_pendingRequests.size());
    for (auto& kv : m_pendingRequests)
        m_tempCallbacks.emplace_back(std::move(kv.second.callback));
    m_pendingRequests.clear();

    for (auto& cb : m_tempCallbacks)
        cb(false, 0, error);
    m_tempCallbacks.clear();
}

int32_t NetClient::request(uint16_t msgId,
                           const char* data,
                           size_t length,
                           const RequestCallback& callback,
                           void* target,
                           float timeout)
{
    --m_nextSerial;
    int32_t serial    = m_nextSerial;
    int32_t requestId = -serial;  // 正值，作为请求 ID

    PendingRequest req;
    req.callback = callback ? callback : defaultCallback;
    req.timeout  = timeout;
    req.target   = target;
    m_pendingRequests.emplace(requestId, std::move(req));

    if (writeFrame(CMD_BUSINESS, msgId, serial, data, length) < 0)
    {
        NET_LOGE("failed to send request, msgId: {}, serial: {}", msgId, serial);
        // 发送失败，将超时设为0，下个检测周期触发失败回调
        m_pendingRequests[requestId].timeout = 0;
    }

    return requestId;
}

int32_t NetClient::requestServerStatus(const RequestCallback& callback, void* target, float timeout)
{
    PB::GatewayClient::ServerStatusReq req;
    std::string data;
    req.SerializeToString(&data);

    --m_nextSerial;
    int32_t serial    = m_nextSerial;
    int32_t requestId = -serial;

    PendingRequest pending;
    pending.callback = callback ? callback : defaultCallback;
    pending.timeout  = timeout;
    pending.target   = target;
    m_pendingRequests.emplace(requestId, std::move(pending));

    if (writeFrame(CMD_GATEWAY_MESSAGE, static_cast<uint16_t>(PB::GatewayClient::ServerStatusReq::Id), serial,
                   data.data(), data.size()) < 0)
    {
        NET_LOGE("failed to send server status request, serial: {}", serial);
        m_pendingRequests[requestId].timeout = 0;
    }

    return requestId;
}

void NetClient::cancel(int32_t requestId)
{
    m_pendingRequests.erase(requestId);
}

void NetClient::cancelByTarget(void* target)
{
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end();)
    {
        if (it->second.target == target)
        {
            it = m_pendingRequests.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void NetClient::push(uint16_t msgId, const char* data, size_t length)
{
    writeFrame(CMD_BUSINESS, msgId, 0, data, length);
}

void NetClient::respond(uint16_t msgId, int32_t serial, const char* data, size_t length)
{
    writeFrame(CMD_BUSINESS, msgId, serial, data, length);
}

bool NetClient::isConnected() const
{
    return m_state == ConnState::Connected;
}

bool NetClient::isBusy() const
{
    return !m_pendingRequests.empty();
}

bool NetClient::hasOnlineService(uint32_t serviceId) const
{
    return std::any_of(m_serviceStatuses.begin(), m_serviceStatuses.end(), [serviceId](const ServiceStatus& status) {
        return status.serviceId == serviceId && status.online;
    });
}

bool NetClient::isInstanceOnline(uint32_t serviceId, uint32_t instanceId) const
{
    return std::any_of(m_serviceStatuses.begin(), m_serviceStatuses.end(),
                       [serviceId, instanceId](const ServiceStatus& status) {
        return status.serviceId == serviceId && status.instanceId == instanceId && status.online;
    });
}

void NetClient::listenRecv(const RecvCallback& callback, const std::string& key)
{
    for (auto& entry : m_recvListeners)
    {
        if (entry.key == key && !entry.removed)
        {
            entry.callback = callback;
            return;
        }
    }
    m_recvListeners.push_back({callback, key, false});
}

void NetClient::unlistenRecv(const std::string& key)
{
    for (auto& entry : m_recvListeners)
    {
        if (entry.key == key)
        {
            entry.removed        = true;
            m_recvListenersDirty = true;
        }
    }
}

// yasio 事件回调
void NetClient::handleEvent(int eventType, int id, const std::string_view& data)
{
    switch (static_cast<YasioClient::Event>(eventType))
    {
    case YasioClient::Event::ConnectSuccess:
        NET_LOGI("connected to {}:{}, connection id: {}", m_host, m_port, id);
        m_connectionId = id;
        m_state        = ConnState::Connected;
        if (m_connectCallback)
        {
            m_connectCallback(true, ""sv);
            m_connectCallback = nullptr;
        }
        break;

    case YasioClient::Event::ConnectFailed:
        NET_LOGI("failed to connect to {}:{}, error: {}", m_host, m_port, data);
        m_state = ConnState::Disconnected;
        if (m_connectCallback)
        {
            m_connectCallback(false, data);
            m_connectCallback = nullptr;
        }
        break;

    case YasioClient::Event::Disconnect:
        NET_LOGI("disconnected, connection id: {}", id);
        if (m_state == ConnState::Connected)
        {
            m_connectionId = -1;
            m_state        = ConnState::Disconnected;

            failPending(NET_ERR_CONNECTION_LOST);

            if (m_disconnectCallback && !m_destroying)
                m_disconnectCallback();
        }
        else
        {
            m_state = ConnState::Disconnected;
        }
        break;

    case YasioClient::Event::RecvData:
        handleFrame(data);
        break;
    }
}

void NetClient::handleFrame(const std::string_view& data)
{
    if (data.size() < NET_FRAME_BODY_HEADER_SIZE)
    {
        NET_LOGE("invalid frame size: {}, disconnecting", data.size());
        disconnect();
        return;
    }

    // yasio::ibstream_view 默认大端序读取
    yasio::ibstream_view ibs(data.data(), static_cast<int>(data.size()));
    uint8_t cmd         = ibs.read<uint8_t>();
    uint16_t msgId      = ibs.read<uint16_t>();
    int32_t serial      = ibs.read<int32_t>();
    const char* payload = data.data() + NET_FRAME_BODY_HEADER_SIZE;
    size_t payloadLen   = data.size() - NET_FRAME_BODY_HEADER_SIZE;

    if (cmd == CMD_BUSINESS)
    {
        // 收到业务逻辑消息请求的回复了
        if (serial > 0)
        {
            auto it = m_pendingRequests.find(serial);
            if (it != m_pendingRequests.end())
            {
                auto cb = std::move(it->second.callback);
                m_pendingRequests.erase(it);
                cb(true, msgId, std::string_view(payload, payloadLen));
            }
        }
    }
    else if (cmd == CMD_GATEWAY_MESSAGE)
    {
        if (msgId == static_cast<uint16_t>(PB::GatewayClient::ServerStatusPush::Id))
        {
            handleServerStatus(serial, payload, payloadLen);
        }
        else if (msgId == static_cast<uint16_t>(PB::GatewayClient::GatewayErrorResp::Id))
        {
            handleGatewayError(serial, payload, payloadLen);
        }
    }
    else
    {
        // 错误的 cmd，理论上不应该收到，忽略并记录日志
        NET_LOGE("received frame with invalid cmd: {}", cmd);
        return;
    }

    // 清理掉被标记为 removed 的回调
    if (m_recvListenersDirty)
    {
        m_recvListenersDirty = false;
        m_recvListeners.erase(std::remove_if(m_recvListeners.begin(), m_recvListeners.end(),
                                             [](const RecvListener& e) { return e.removed; }),
                              m_recvListeners.end());
    }
    // 派发给所有回调
    if (!m_recvListeners.empty())
    {
        for (auto& entry : m_recvListeners)
        {
            if (!entry.removed)
            {
                entry.callback(cmd, msgId, serial, std::string_view(payload, payloadLen));
            }
        }
    }
}

// 处理网关下发的服务器状态信息
void NetClient::handleServerStatus(int32_t serial, const char* payload, size_t length)
{
    PB::GatewayClient::ServerStatusPush push;
    if (!push.ParseFromArray(payload, static_cast<int>(length)))
    {
        NET_LOGE("failed to parse ServerStatusPush");
        if (serial > 0)
        {
            auto it = m_pendingRequests.find(serial);
            if (it != m_pendingRequests.end())
            {
                auto cb = std::move(it->second.callback);
                m_pendingRequests.erase(it);
                cb(false, 0, NET_GATEWAY_ERR_DECODE_FAILED);
            }
        }
        return;
    }

    // serial > 0:回应 ServerStatusReq 的全量快照,清空重建;
    // serial == 0:网关主动推送的单服务增量(绑定/解绑/上下线),按 (service_id, instance_id) 合并,
    // 否则会误清其他服务的在线状态(如绑定 town 后清掉 game 导致误报"服务器维护中")。
    const bool fullSnapshot = serial > 0;
    if (fullSnapshot)
    {
        m_serviceStatuses.clear();
        m_serviceStatuses.reserve(push.services_size());
    }

    NET_LOGI("--------------------------------------------------");
    NET_LOGI("received server status push, {} services, {}.", push.services_size(),
             fullSnapshot ? "full snapshot" : "incremental");
    for (const auto& service : push.services())
    {
        NET_LOGI("service_id: {}, instance_id: {}, online: {}", service.service_id(), service.instance_id(),
                 service.online());

        auto it =
            std::find_if(m_serviceStatuses.begin(), m_serviceStatuses.end(), [&service](const ServiceStatus& status) {
            return status.serviceId == service.service_id() && status.instanceId == service.instance_id();
        });
        if (it != m_serviceStatuses.end())
        {
            it->online = service.online();
        }
        else
        {
            m_serviceStatuses.push_back(ServiceStatus{
                service.service_id(),
                service.instance_id(),
                service.online(),
            });
        }
    }
    NET_LOGI("--------------------------------------------------");

    if (serial > 0)
    {
        auto it = m_pendingRequests.find(serial);
        if (it != m_pendingRequests.end())
        {
            auto cb = std::move(it->second.callback);
            m_pendingRequests.erase(it);
            cb(true, static_cast<uint16_t>(PB::GatewayClient::ServerStatusPush::Id), std::string_view(payload, length));
        }
    }

    if (m_statusChangeCallback)
    {
        m_statusChangeCallback();
    }
}

// 处理网关错误
void NetClient::handleGatewayError(int32_t serial, const char* payload, size_t length)
{
    std::string_view error;
    PB::GatewayClient::GatewayErrorResp resp;
    if (resp.ParseFromArray(payload, static_cast<int>(length)))
    {
        // 根据网关返回的错误码转换成对应的错误信息
        switch (resp.code())
        {
        case GATEWAY_ERR_INVALID_CMD:
            error = NET_GATEWAY_ERR_INVALID_CMD;
            break;
        case GATEWAY_ERR_SERVICE_UNAVAILABLE:
            error = NET_GATEWAY_ERR_SERVICE_UNAVAILABLE;
            break;
        case GATEWAY_ERR_SERVICE_NOT_BOUND:
            error = NET_GATEWAY_ERR_SERVICE_NOT_BOUND;
            break;
        case GATEWAY_ERR_UNKNOWN_ROUTE:
            error = NET_GATEWAY_ERR_UNKNOWN_ROUTE;
            break;
        case GATEWAY_ERR_UNAUTHENTICATED:
            error = NET_GATEWAY_ERR_UNAUTHENTICATED;
            break;
        default:
            error = NET_GATEWAY_ERR_OTHER;
            break;
        };
        NET_LOGE("Gateway error: {}, code: {}, msg: {}", error, resp.code(), resp.message());
    }
    else
    {
        error = NET_GATEWAY_ERR_DECODE_FAILED;
        NET_LOGE("failed to parse GatewayErrorResp");
    }

    if (serial > 0)
    {
        auto it = m_pendingRequests.find(serial);
        if (it != m_pendingRequests.end())
        {
            NET_LOGD("request serial: {}, error: {}", serial, error);
            auto cb = std::move(it->second.callback);
            m_pendingRequests.erase(it);
            cb(false, 0, error);
        }
    }
}

int NetClient::writeFrame(uint8_t cmd, uint16_t msgId, int32_t serial, const char* data, size_t length)
{
    if (m_connectionId == -1)
        return -1;

    uint32_t frameLen = static_cast<uint32_t>(NET_FRAME_HEADER_SIZE + length);

    yasio::obstream obs;
    obs.write<uint32_t>(frameLen);
    obs.write<uint8_t>(cmd);
    obs.write<uint16_t>(msgId);
    obs.write<int32_t>(serial);
    if (length > 0)
        obs.write_bytes(data, static_cast<int>(length));

    return m_transport->send(m_connectionId, obs);
}

void NetClient::checkTimeouts(float dt)
{
#ifdef AX_PLATFORM_PC
    auto n   = now();
    dt       = (n - lastTime) / 1000.0f;
    lastTime = n;
#endif

    m_tempCallbacks.clear();
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end();)
    {
        it->second.timeout -= dt;
        if (it->second.timeout <= 0)
        {
            // 超时了
            NET_LOGD("request serial: {} timed out", it->first);
            m_tempCallbacks.emplace_back(std::move(it->second.callback));
            it = m_pendingRequests.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto& cb : m_tempCallbacks)
        cb(false, 0, NET_ERR_TIMEOUT);
    m_tempCallbacks.clear();
}

}  // namespace net
