#pragma once

#include "mugen/core/Object.h"

#include <cstdint>
#include <string>
#include <variant>

NS_MG_BEGIN

// 事件数据
using EventData = std::variant<std::monostate, int32_t, uint32_t, float, double, bool, std::string>;

NS_MG_END
