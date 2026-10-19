#pragma once
#include "mugen/sim/role/RoleComponents.h"

namespace mugen::sim
{
class World;
namespace hit
{
// 伤害许可在配置三元组中的位置。
enum class HurtPermit : uint8_t
{
    Attack,
    Hp,
    Number
};
// 伤害公式输入；完整定义见 Damage.h。
struct DamageInput;
// 伤害计算器：包装世界实例，收拢伤害归属、标准伤害、公式输入与许可查询，
// 替代原先按 (World&, ...) 传参的自由函数。仅作为临时对象使用，不拥有资源。
class DamageCalculator
{
public:
    explicit DamageCalculator(World& world);
    // 解析召唤物最终归属；不存在归属时返回空句柄。
    Entity finalOwner(Entity entity) const;
    // 按角色类型和等级读取标准伤害。
    Fixed standardHurt(Entity attacker) const;
    // 从特效和双方实体解析公式输入。
    bool damageInput(Entity effect, Entity target, DamageInput& input) const;
    // 查询配置中的伤害许可。
    bool checkHurtPermit(Entity target, Entity effect, HurtPermit permit) const;

private:
    World& m_world;
};
}  // namespace hit
}  // namespace mugen::sim
