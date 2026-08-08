#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#include "net/NetErr.h"
#include "net/NetLog.h"

namespace net
{

namespace detail
{

template <typename T>
struct CallableFirstArg;

template <typename C, typename R, typename A, typename... Rest>
struct CallableFirstArg<R (C::*)(A, Rest...) const>
{
    using type = A;
};

template <typename C, typename R, typename A, typename... Rest>
struct CallableFirstArg<R (C::*)(A, Rest...)>
{
    using type = A;
};

template <typename Callable>
using CallableFirstArgT = typename CallableFirstArg<decltype(&std::decay_t<Callable>::operator())>::type;

template <typename Callback>
using CallRespT = std::remove_cv_t<std::remove_pointer_t<std::remove_cvref_t<CallableFirstArgT<Callback>>>>;

template <typename Listener>
using PushListenerT = std::remove_cvref_t<CallableFirstArgT<Listener>>;

}  // namespace detail

// 绑定到具体对象的网络代理：请求/通知/推送监听，回调随对象生命周期自动取消。
class NetAgent
{
public:
    NetAgent();

    virtual ~NetAgent();

    /// 发送请求并等待响应回调；析构或 detach 时自动取消
    int32_t request(uint16_t msgId,
                    const char* data,
                    size_t length,
                    const std::function<void(bool, uint16_t, const std::string_view&)>& callback,
                    float timeout = 10.0f);

    /// 从网络层解绑：取消未完成请求、取消收包监听，并清空本地推送监听（与析构行为一致，可提前调用）
    void detach();

    /// 发送 protobuf 请求并自动解码响应；RespT 从 callback 第一个参数类型推导
    template <typename ReqT, typename Callback>
    int32_t call(const ReqT& req, Callback&& callback, float timeout = 10.0f)
    {
        using RespT = detail::CallRespT<Callback>;
        auto cb     = std::decay_t<Callback>(std::forward<Callback>(callback));

        std::string data;
        if (!req.SerializeToString(&data))
        {
            NET_LOGE("call SerializeToString failed: [ReqMsgId={}]", static_cast<uint16_t>(ReqT::Id));
            cb(nullptr, NET_ERR_SERIALIZE_FAILED);
            return 0;
        }
        return request(static_cast<uint16_t>(ReqT::Id), data.data(), data.size(),
                       [cb = std::move(cb)](bool success, uint16_t msgId, const std::string_view& payload) mutable {
            if (!success)
            {
                cb(nullptr, payload);
                return;
            }

            if (msgId == static_cast<uint16_t>(RespT::Id))
            {
                RespT resp;
                if (!resp.ParseFromArray(payload.data(), static_cast<int>(payload.size())))
                {
                    NET_LOGE("call ParseFromArray failed: [ReqMsgId={}] -> [RespMsgId={}], payload size: {}",
                             static_cast<uint16_t>(ReqT::Id), msgId, payload.size());
                    cb(nullptr, NET_ERR_DECODE_FAILED);
                    return;
                }
                cb(&resp, {});
            }
            else
            {
                auto error = parseServerInternalError(msgId, payload);
                if (error)
                {
                    NET_LOGE("call failed: [ReqMsgId={}] -> [RespMsgId={}], server internal error: {} (code: {})",
                             static_cast<uint16_t>(ReqT::Id), msgId, std::get<1>(*error), std::get<0>(*error));
                    cb(nullptr, NET_ERR_SERVER_INTERNAL_ERROR);
                }
                else
                {
                    NET_LOGE("call failed: [ReqMsgId={}] -> [RespMsgId={}], unknown server error, payload size: {}",
                             static_cast<uint16_t>(ReqT::Id), msgId, payload.size());
                    cb(nullptr, NET_ERR_UNKNOWN_SERVER_ERROR);
                }
            }
        }, timeout);
    }

    /// 发送单向消息（无需响应）
    void push(uint16_t msgId, const char* data, size_t length);

    /// 发送单向 protobuf 消息
    template <typename ReqT>
    void notify(const ReqT& req)
    {
        std::string data;
        if (!req.SerializeToString(&data))
        {
            NET_LOGE("notify SerializeToString failed: [ReqMsgId={}]", static_cast<uint16_t>(ReqT::Id));
            return;
        }
        push(static_cast<uint16_t>(ReqT::Id), data.data(), data.size());
    }

    /// 监听服务端推送
    template <typename Listener>
    void listenPush(Listener&& listener)
    {
        using PushT                                       = detail::PushListenerT<Listener>;
        m_pushListeners[static_cast<uint16_t>(PushT::Id)] = [listener = std::decay_t<Listener>(std::forward<Listener>(
                                                                 listener))](const std::string_view& payload) mutable {
            PushT pushMsg;
            if (!pushMsg.ParseFromArray(payload.data(), static_cast<int>(payload.size())))
            {
                NET_LOGE("listenPush ParseFromArray failed: [PushMsgId={}], payload size: {}",
                         static_cast<uint16_t>(PushT::Id), payload.size());
                return;
            }
            listener(pushMsg);
        };
    }

protected:
    // 收到推送后的扩展点（已匹配 listenPush 的消息也会调用）
    virtual void onPush(uint16_t msgId, const std::string_view& payload);

    // 将推送分发给 listenPush 注册的回调，并触发 onPush
    void dispatchPush(uint16_t msgId, const std::string_view& payload);

private:
    // 独立生命周期标记：push 回调里可能销毁 this（如 switchView），
    // 用 shared_ptr 探测销毁，避免再访问 this。
    struct LifetimeToken
    {
        bool alive = true;
    };

    std::shared_ptr<LifetimeToken> m_lifetime;
    std::map<uint16_t, std::function<void(const std::string_view&)>> m_pushListeners;
};

}  // namespace net
