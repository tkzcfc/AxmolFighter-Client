#pragma once
#include <vector>
#include "mugen/sim/role/RoleComponents.h"
#include "mugen/sim/hit/HitSeams.h"

namespace mugen::conf
{
struct EffectConfig;
struct SkillHitConfig;
}

namespace mugen::sim
{
class World;
namespace hit
{
// 命中求解器：包装一次命中的完整上下文，执行碰撞分派与命中流程，
// 替代原先散落在 Hit.cpp 匿名命名空间中的自由函数。仅作为临时对象使用，不拥有资源。
class HitResolver
{
public:
    HitResolver(World& world, Entity effect, Entity target, FixedVec2 contact);
    // 按目标类型与攻击者阵营分派，并对角色目标执行完整命中流程。
    void resolve();

private:
    // 判断实体是否仍可参与逻辑命中。
    bool _enabled(Entity entity) const;
    // 判断当前技能是否为闪避槽。
    bool _dodging(Entity entity) const;
    // 构造各命中阶段共享的完整来源数据。
    HitTrigger _context(Entity effect, Entity target) const;
    // 属性闪避与主动闪避分别触发事件和文本。
    void _dodge(Entity target, const HitTrigger& data, bool passive);
    // 生成或复用同配置同目标的命中特效，位置取碰撞接触点。
    void _hitEffects(Entity sourceEntity, Entity target, FixedVec2 contact);
    // 受击预处理保持事件触发与状态过滤的先后顺序。
    bool _beforeHit(Entity target, const HitTrigger& data, const conf::EffectConfig& effectRow,
                    const conf::SkillHitConfig& hitRow, FixedVec2 contact);
    // 非霸体受击设置冻结、朝向与追击数据；状态叶子由独立模块执行。
    void _hitReaction(Entity target, const HitTrigger& data, const conf::SkillHitConfig& hitRow);
    // 非爆发状态按命中或受击回复能量。
    void _recoverEp(Entity entity, Fixed value);
    // 命中结束先处理目标表现和能量，再处理攻击者输出和事件。
    void _endHit(Entity target, HitTrigger data, const conf::EffectConfig& row);
    // 按原始列表首项哨兵转交 Buff 施加；不提前消耗概率随机值。
    void _applyBuffs(Entity pool, const std::vector<Fixed>& ids, HitTrigger data);
    // 整数配置列表转换为不丢精度的施加请求。
    static std::vector<Fixed> _buffIds(const std::vector<int32_t>& ids);
    // 角色目标的完整命中流程。
    void _hitTarget(const conf::EffectConfig& row);

    World& m_world;
    Entity m_effect{};
    Entity m_target{};
    FixedVec2 m_contact{};
};
}  // namespace hit
}  // namespace mugen::sim
