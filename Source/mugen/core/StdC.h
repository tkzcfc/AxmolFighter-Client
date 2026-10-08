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

#define MG_ASSERT(...) assert(__VA_ARGS__)

NS_MG_BEGIN

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
// Call sites use the same "{}" placeholders as the engine logger.
// The MSVC standard library only formats lvalues, so each argument is stored first.
template <typename... Args>
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
