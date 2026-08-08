#pragma once

#include "yasio/yasio.hpp"
#include "yasio/obstream.hpp"

#include <functional>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace net
{

class YasioClient final
{
public:
    enum class Event : int
    {
        ConnectSuccess,
        ConnectFailed,
        Disconnect,
        RecvData
    };

    using EventCallback = std::function<void(int, int, const std::string_view&)>;

    YasioClient() = delete;

    explicit YasioClient(int maxChannelCount);

    ~YasioClient();

    // 小于0表示出错，其他返回值表示新的连接id
    int connect(const std::string& host, int port, int kind);

    // 断开连接
    void disconnect(int connectionId);

    // 发送数据，返回值小于0表示出错
    int send(int connectionId, const char* data, size_t length);

    // 发送数据，返回值小于0表示出错
    int send(int connectionId, const yasio::obstream& obs);

    // 设置事件回调
    void setOnEvent(const EventCallback& callback);

private:
    // 连接状态
    enum class ConnStatus : uint8_t
    {
        // 连接排队中,等到有空闲信道时再发起连接
        Queuing,
        // 正在连接中
        Connecting,
        // 已连接
        Connected,
        // 正在断开连接中
        Disconnecting,
        // 已断开连接
        Disconnected
    };

    // 连接信息
    struct Conn
    {
        // 连接的主机地址
        std::string host;
        // 连接的端口号
        int port;
        // 连接的唯一标识
        int id;
        // 连接的类型，参考 yasio::YCK_TCP_CLIENT、yasio::YCK_UDP_CLIENT 等
        int kind;
        // 连接的信道号
        int channel;
        // 连接成功后yasio返回的连接的传输句柄
        yasio::transport_handle_t transport;
        // 连接的状态
        ConnStatus status;
    };

    void handleEvent(yasio::io_event* event);

    void handleEof(yasio::io_channel* channel, int internalErrorCode);

    void tick();

    // 处理连接队列，尝试发起连接
    void flushConnectQueue();

    // 获取一个空闲的信道号，如果没有空闲信道则返回-1
    int takeChannel();

    // 释放一个信道号，将其放回空闲信道队列
    void releaseChannel(int channel);

    inline void emitEvent(Event evt, int connectionId, char* data, size_t length)
    {
        if (m_onEvent)
        {
            m_onEvent(static_cast<int>(evt), connectionId, std::string_view(data, length));
        }
    }

private:
    // yasio服务对象
    std::unique_ptr<yasio::io_service> m_service;

    // 下一个连接id，用于生成唯一的连接标识
    int m_nextConnectionId;
    // 等待连接队列
    std::vector<Conn> m_connectQueue;
    // 已连接列表
    std::unordered_map<int, Conn> m_connections;
    // 空闲信道列表
    std::queue<int> m_freeChannels;

    EventCallback m_onEvent;
};

}  // namespace net
