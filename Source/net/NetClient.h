#pragma once

#include "YasioClient.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace net
{
// 网络帧格式：len(4) + cmd(1) + msg_id(2) + serial(4) + payload(n)
constexpr size_t NET_FRAME_HEADER_SIZE = 11;

// 网络帧 body 格式(yasio解码分片后将长度四个字节去掉了)：
// cmd(1) + msg_id(2) + serial(4)
constexpr size_t NET_FRAME_BODY_HEADER_SIZE = 7;

// 网络帧命令类型
// cmd = 1: 业务消息，msg_id 对应具体的业务消息类型
constexpr uint8_t CMD_BUSINESS = 1;
// cmd = 2: 网关消息，msg_id 对应网关消息类型
constexpr uint8_t CMD_GATEWAY_MESSAGE = 2;

using ConnectCallback      = std::function<void(bool, const std::string_view&)>;
using DisconnectCallback   = std::function<void()>;
using RequestCallback      = std::function<void(bool, uint16_t, const std::string_view&)>;
using RecvCallback         = std::function<void(uint8_t, uint16_t, int32_t, const std::string_view&)>;
using StatusChangeCallback = std::function<void()>;

struct ServiceStatus
{
    uint32_t serviceId;
    uint32_t instanceId;
    bool online;
};

class NetClient
{
public:
    NetClient() = delete;
    explicit NetClient(int maxChannelCount);
    ~NetClient();

    void setHost(const std::string& host, int port);

    void connect(const ConnectCallback& callback);
    void disconnect();

    // 发送请求并等待响应回调
    int32_t request(uint16_t msgId,
                    const char* data,
                    size_t length,
                    const RequestCallback& callback,
                    void* target,
                    float timeout = 10.0f);

    // 请求网关获取当前服务状态列表
    int32_t requestServerStatus(const RequestCallback& callback, void* target, float timeout = 10.0f);

    // 取消指定请求 ID 的请求
    void cancel(int32_t requestId);
    // 取消指定 target 的所有请求
    void cancelByTarget(void* target);

    // 发送单向消息（无需响应）
    void push(uint16_t msgId, const char* data, size_t length);
    // 发送回复给服务器,但是现在的设计是客户端不需要回复服务器的请求，所以这个函数暂时不会被调用
    void respond(uint16_t msgId, int32_t serial, const char* data, size_t length);

    // 当前是否已连接到网关
    bool isConnected() const;

    // 当前是否有未完成的请求
    bool isBusy() const;

    // 获取当前网关下发的服务状态列表
    const std::vector<ServiceStatus>& serviceStatuses() const { return m_serviceStatuses; }
    bool hasOnlineService(uint32_t serviceId) const;
    bool isInstanceOnline(uint32_t serviceId, uint32_t instanceId) const;

    void setOnDisconnect(const DisconnectCallback& callback) { m_disconnectCallback = callback; }

    // 设置接收消息回调，key 用于标识回调，方便移除。
    void listenRecv(const RecvCallback& callback, const std::string& key);
    void unlistenRecv(const std::string& key);

    // 设置服务状态变化回调
    void setOnServiceStatusChange(const StatusChangeCallback& callback) { m_statusChangeCallback = callback; }

private:
    void handleEvent(int eventType, int id, const std::string_view& data);

    void handleFrame(const std::string_view& data);

    // 处理网关下发的服务器状态。
    void handleServerStatus(int32_t serial, const char* payload, size_t length);

    // 处理网关错误。
    void handleGatewayError(int32_t serial, const char* payload, size_t length);

    int writeFrame(uint8_t cmd, uint16_t msgId, int32_t serial, const char* data, size_t length);

    void failPending(const std::string_view& error);

    void checkTimeouts(float dt);

    enum class ConnState : uint8_t
    {
        Disconnected,
        Connecting,
        Connected,
    };

    struct PendingRequest
    {
        RequestCallback callback;
        float timeout;
        void* target;
    };

    struct RecvListener
    {
        RecvCallback callback;
        std::string key;
        bool removed;
    };

private:
    std::unique_ptr<YasioClient> m_transport;
    std::string m_host;
    int m_port;
    int m_connectionId;
    ConnState m_state;
    bool m_destroying;

    int32_t m_nextSerial;

    std::unordered_map<int32_t, PendingRequest> m_pendingRequests;

    ConnectCallback m_connectCallback;
    DisconnectCallback m_disconnectCallback;

    std::vector<RecvListener> m_recvListeners;
    bool m_recvListenersDirty;

    std::vector<ServiceStatus> m_serviceStatuses;
    std::vector<RequestCallback> m_tempCallbacks;
    StatusChangeCallback m_statusChangeCallback;
};

}  // namespace net
