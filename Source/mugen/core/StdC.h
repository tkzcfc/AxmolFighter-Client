#pragma once

#include <stdint.h>
#include <string>
#include <assert.h>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <unordered_set>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include "MacroDefinition.h"

#define MG_ASSERT(...) assert(__VA_ARGS__)

NS_MG_BEGIN

typedef uint8_t byte;

NS_MG_END

#if !defined(RUNTIME_IN_AXMOL)
#    define RUNTIME_IN_AXMOL 1
#endif  // !RUNTIME_IN_AXMOL

#if defined(RUNTIME_IN_AXMOL) && !RUNTIME_IN_AXMOL
#    undef RUNTIME_IN_AXMOL
#endif

#if defined(OLUA_AUTOCONF)
#    undef RUNTIME_IN_AXMOL
#endif

#ifdef RUNTIME_IN_AXMOL
#    include "axmol.h"
#    define MG_LOG_D(format, ...) AXLOGD(format, ##__VA_ARGS__)
#    define MG_LOG_I(format, ...) AXLOGI(format, ##__VA_ARGS__)
#    define MG_LOG_W(format, ...) AXLOGW(format, ##__VA_ARGS__)
#    define MG_LOG_E(format, ...) AXLOGE(format, ##__VA_ARGS__)
#else
#    define MG_LOG_D(format, ...) \
        do                        \
        {                         \
        } while (false)
#    define MG_LOG_I(format, ...) \
        do                        \
        {                         \
        } while (false)
#    define MG_LOG_W(format, ...) \
        do                        \
        {                         \
        } while (false)
#    define MG_LOG_E(format, ...) \
        do                        \
        {                         \
        } while (false)
#endif
