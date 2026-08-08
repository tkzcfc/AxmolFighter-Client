#include "YasioClient.h"
#include "NetLog.h"

namespace net
{

YasioClient::YasioClient(int maxChannelCount) : m_nextConnectionId(0), m_onEvent(nullptr)
{
    if (maxChannelCount <= 0)
        maxChannelCount = 1;

    m_connectQueue.reserve(maxChannelCount);

    m_service = std::make_unique<yasio::io_service>(maxChannelCount);
    m_service->set_option(yasio::YOPT_S_CONNECT_TIMEOUT, 5);
    m_service->set_option(yasio::YOPT_S_NO_DISPATCH, 1);
    m_service->set_option(yasio::YOPT_S_DNS_QUERIES_TIMEOUT, 3);
    m_service->set_option(yasio::YOPT_S_DNS_QUERIES_TRIES, 1);

    for (auto i = 0; i < maxChannelCount; ++i)
    {
        releaseChannel(i);
    }

    m_service->start([this](yasio::event_ptr&& e) { handleEvent(e.get()); });

    ax::Director::getInstance()->getScheduler()->schedule([this](float) { this->tick(); }, this, 0, false, "#");
}

YasioClient::~YasioClient()
{
    ax::Director::getInstance()->getScheduler()->unscheduleAllForTarget(this);
    m_service->stop();
    m_service.reset();
}

int YasioClient::connect(const std::string& host, int port, int kind)
{
    Conn conn;
    conn.id        = m_nextConnectionId++;
    conn.status    = ConnStatus::Queuing;
    conn.port      = port;
    conn.host      = host;
    conn.kind      = kind;
    conn.channel   = -1;
    conn.transport = nullptr;
    m_connectQueue.push_back(conn);
    return conn.id;
}

void YasioClient::disconnect(int connectionId)
{
    NET_LOGD("YasioClient: disconnect, id = {}", connectionId);
    auto it = m_connections.find(connectionId);
    if (it != m_connections.end())
    {
        m_service->close(it->second.channel);
        return;
    }

    for (auto it = m_connectQueue.begin(); it != m_connectQueue.end(); ++it)
    {
        if (it->id == connectionId)
        {
            m_connectQueue.erase(it);
            break;
        }
    }
}

int YasioClient::send(int connectionId, const char* data, size_t length)
{
    auto it = m_connections.find(connectionId);
    if (it == m_connections.end())
    {
        NET_LOGW("YasioClient: send failed, connection not found, id = {}", connectionId);
        return -1000;
    }

    auto& transport = it->second.transport;
    if (!transport)
    {
        NET_LOGW("YasioClient: send failed, transport not found, id = {}", connectionId);
        return -1001;
    }

    return m_service->write(transport, data, length);
}

int YasioClient::send(int connectionId, const yasio::obstream& obs)
{
    NET_LOGD("YasioClient: try send packet, id = {}, size = {}", connectionId, obs.length());
    auto it = m_connections.find(connectionId);
    if (it == m_connections.end())
    {
        NET_LOGW("YasioClient: send failed, connection not found, id = {}", connectionId);
        return -1000;
    }

    auto& transport = it->second.transport;
    if (!transport)
    {
        NET_LOGW("YasioClient: send failed, transport not found, id = {}", connectionId);
        return -1001;
    }

    NET_LOGD("YasioClient: do send packet, id = {}, size = {}", connectionId, obs.length());
    return m_service->write(transport, std::move(obs.buffer()));
}

void YasioClient::setOnEvent(const EventCallback& callback)
{
    m_onEvent = callback;
}

void YasioClient::handleEvent(yasio::io_event* event)
{
    int channelIndex = event->cindex();
    auto channel     = m_service->channel_at(channelIndex);
    int connectionId = channel->ud_.ival;

    switch (event->kind())
    {
    case yasio::YEK_ON_OPEN:
    {
        if (event->status() == 0)
        {
            auto it = m_connections.find(connectionId);
            if (it != m_connections.end())
            {
                it->second.transport = event->transport();
            }
            NET_LOGD("YasioClient: connect success, id = {}", connectionId);
            emitEvent(Event::ConnectSuccess, connectionId, 0, 0);
        }
        else
        {
            NET_LOGD("YasioClient: connect failed, id = {}", connectionId);
            handleEof(channel, event->status());

            char err[128];
            snprintf(err, sizeof(err), "connect failed, internal error code: %d", event->status());
            emitEvent(Event::ConnectFailed, connectionId, err, strlen(err));
        }
    }
    break;
    case yasio::YEK_ON_CLOSE:
    {
        NET_LOGD("YasioClient: disconnect, id = {}, status = {}", connectionId, event->status());
        handleEof(channel, event->status());

        char err[128];
        snprintf(err, sizeof(err), "disconnect, internal error code: %d", event->status());
        emitEvent(Event::Disconnect, connectionId, err, strlen(err));
    }
    break;
    case yasio::YEK_ON_PACKET:
    {
        auto& packet = event->packet();
        NET_LOGD("YasioClient: recv packet, id = {}, size = {}", connectionId, packet.size());
        emitEvent(Event::RecvData, connectionId, packet.data(), packet.size());
    }
    break;
    }
}

void YasioClient::handleEof(yasio::io_channel* channel, int internalErrorCode)
{
    int connectionId  = channel->ud_.ival;
    channel->ud_.ival = -1;

    auto it = m_connections.find(connectionId);
    if (it != m_connections.end())
    {
        m_connections.erase(it);
    }
    // 回收信道
    releaseChannel(channel->index());
}

void YasioClient::tick()
{
    flushConnectQueue();
    m_service->dispatch();
}

void YasioClient::flushConnectQueue()
{
    while (!m_connectQueue.empty())
    {
        auto& conn   = m_connectQueue.front();
        auto channel = takeChannel();

        // 没有空闲信道了，等待下次tick再处理
        if (channel < 0)
            break;

        conn.channel = channel;

        auto channelHandle = m_service->channel_at(channel);
        NET_LOGD("YasioClient: open connection for {}:{}, id = {}", conn.host, conn.port, conn.id);
        channelHandle->ud_.ival = conn.id;

        if (conn.kind == yasio::YCK_UDP_CLIENT)
        {
            m_service->set_option(yasio::YOPT_C_UNPACK_PARAMS, channel, 1024 * 1024 * 10, -1, 4, 0);
            m_service->set_option(yasio::YOPT_C_UNPACK_STRIP, channel, 0);
            m_service->set_option(yasio::YOPT_C_UNPACK_NO_BSWAP, channel, 0);
        }
        else
        {
            // TCP 帧格式：[u32 len][payload...]
            // len 包含自身4字节，大端序（网络字节序）
            m_service->set_option(yasio::YOPT_C_UNPACK_PARAMS, channel, 1024 * 1024 * 10, 0, 4, 0);
            m_service->set_option(yasio::YOPT_C_UNPACK_STRIP, channel, 4);
            m_service->set_option(yasio::YOPT_C_UNPACK_NO_BSWAP, channel, 0);
        }

        m_service->set_option(yasio::YOPT_C_REMOTE_ENDPOINT, channel, conn.host.data(), conn.port);
        m_service->open(channel, conn.kind);

        m_connections.insert(std::make_pair(conn.id, m_connectQueue[0]));
        m_connectQueue.erase(m_connectQueue.begin());
    }
}

int YasioClient::takeChannel()
{
    if (!m_freeChannels.empty())
    {
        auto channel = m_freeChannels.front();
        m_freeChannels.pop();
        return channel;
    }
    return -1;
}

void YasioClient::releaseChannel(int channel)
{
    m_freeChannels.push(channel);
}

}  // namespace net
