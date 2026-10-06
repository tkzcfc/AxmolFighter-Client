#pragma once

#include <cstdint>

namespace gameui
{

// 本地玩家出场角色 id：根据 AppContext::gameSession() 里已选角色的职业映射到 RoleConfig id；
// 没有选中角色，或者映射出来的角色没有配置，就回退到固定的兜底角色（英雄 role 101）。
// LocalBattleMode、TownView、GameView 共用这一份逻辑，确保城镇/副本里本地玩家的角色 id
// 解析结果一致（预加载和真正 spawn 时都调用它，缓存 key 才不会对不上）。
int32_t resolveLocalPlayerRoleId();

}  // namespace gameui
