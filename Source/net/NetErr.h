#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <tuple>

namespace net
{

using namespace std::string_view_literals;

/// 网关向客户端返回错误:命令无效
inline constexpr std::string_view NET_GATEWAY_ERR_INVALID_CMD = "invalid command";
/// 网关向客户端返回错误:找不到转发的服务 (也就是对应的服务器不在线,服务器处于维护中)
inline constexpr std::string_view NET_GATEWAY_ERR_SERVICE_UNAVAILABLE = "service unavailable";
/// 网关向客户端返回错误:服务未绑定
inline constexpr std::string_view NET_GATEWAY_ERR_SERVICE_NOT_BOUND = "service not bound";
/// 网关向客户端返回错误:未知路由
inline constexpr std::string_view NET_GATEWAY_ERR_UNKNOWN_ROUTE = "unknown route";
/// 网关向客户端返回错误:未认证
inline constexpr std::string_view NET_GATEWAY_ERR_UNAUTHENTICATED = "gateway unauthenticated";
/// 网关向客户端返回错误:其他错误
inline constexpr std::string_view NET_GATEWAY_ERR_OTHER = "gateway error";
/// 客户端无法解析网关返回的错误信息
inline constexpr std::string_view NET_GATEWAY_ERR_DECODE_FAILED = "gateway error decode failed";

/// 网络请求超时
inline constexpr std::string_view NET_ERR_TIMEOUT = "timeout";
/// 网络连接断开
inline constexpr std::string_view NET_ERR_CONNECTION_LOST = "connection lost";
/// 主动断开连接
inline constexpr std::string_view NET_ERR_DISCONNECTED = "disconnected";
/// 数据编码失败
inline constexpr std::string_view NET_ERR_SERIALIZE_FAILED = "serialize failed";
/// 数据解码失败
inline constexpr std::string_view NET_ERR_DECODE_FAILED = "decode failed";
/// 服务器内部错误
inline constexpr std::string_view NET_ERR_SERVER_INTERNAL_ERROR = "server internal error";
/// 未知服务器错误
inline constexpr std::string_view NET_ERR_UNKNOWN_SERVER_ERROR = "unknown server error";

uint16_t getServerInternalErrorCode();

std::optional<std::tuple<int32_t, std::string>> parseServerInternalError(uint16_t msgid, std::string_view payload);

}  // namespace net
