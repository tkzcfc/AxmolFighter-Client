#include "RoleAction.h"

#include <array>

namespace mugen::sim::behavior
{
// 查表取得普通动作的动画名称。
const char* ActionNames::animationName(ActionType type)
{
    constexpr std::array names{"",        "death",   "getup",   "toground", "flydown",  "flyup", "hitswitch",
                               "hit",     "flydown", "running", "running",  "running",  "walk",  "running",
                               "stand",   "spurt",   "death",   "active",   "transfer", "walk",  "running",
                               "running", "running", "running", "special",  "hit", "stand", "stand", ""};
    return names.at(static_cast<size_t>(type));
}
}  // namespace mugen::sim::behavior
