#pragma once

#include <stdint.h>
#include <string>
#include <assert.h>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <array>
#include <algorithm>
#include <functional>
#include "MacroDefinition.h"

// 未显式指定时默认使用引擎运行时。
#if !defined(RUNTIME_IN_AXMOL)
#    define RUNTIME_IN_AXMOL 1
#endif  // !RUNTIME_IN_AXMOL

#if defined(RUNTIME_IN_AXMOL) && !RUNTIME_IN_AXMOL
#    undef RUNTIME_IN_AXMOL
#endif

#if defined(OLUA_AUTOCONF)
#    undef RUNTIME_IN_AXMOL
#endif

#ifndef RUNTIME_IN_AXMOL
#    include <format>
#    include <iostream>
#    include <string_view>
#    include <utility>
#endif

// 统一核心断言入口。
#define MG_ASSERT(...) assert(__VA_ARGS__)

NS_MG_BEGIN

// 二进制缓冲区使用的单字节类型别名。
typedef uint8_t byte;

NS_MG_END

#ifdef RUNTIME_IN_AXMOL
#    include "axmol.h"
#    define MG_LOG_D(format, ...) AXLOGD(format, ##__VA_ARGS__)
#    define MG_LOG_I(format, ...) AXLOGI(format, ##__VA_ARGS__)
#    define MG_LOG_W(format, ...) AXLOGW(format, ##__VA_ARGS__)
#    define MG_LOG_E(format, ...) AXLOGE(format, ##__VA_ARGS__)
#else
namespace mugen::detail
{
// 无引擎环境复用与引擎日志相同的 "{}" 占位符语义。
// MSVC 标准库仅格式化左值，因此参数以 const 引用传入。
template <typename... Args>
// 向标准错误输出带级别的格式化日志。
void mgLog(std::string_view level, std::string_view fmt, const Args&... args)
{
    std::cerr << '[' << level << "] " << std::vformat(fmt, std::make_format_args(args...)) << '\n';
}
}  // namespace mugen::detail

#    define MG_LOG_D(format, ...) ::mugen::detail::mgLog("D", format, ##__VA_ARGS__)
#    define MG_LOG_I(format, ...) ::mugen::detail::mgLog("I", format, ##__VA_ARGS__)
#    define MG_LOG_W(format, ...) ::mugen::detail::mgLog("W", format, ##__VA_ARGS__)
#    define MG_LOG_E(format, ...) ::mugen::detail::mgLog("E", format, ##__VA_ARGS__)
#endif
