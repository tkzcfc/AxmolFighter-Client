#pragma once
#include "AiRules.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/FixedVec3.h"
#include <vector>

namespace mugen::conf
{
struct AiConfig;
}

namespace mugen::sim
{
class World;
// 按首次受击顺序保存一个攻击者的仇恨。
struct AiThreat
{
    // 攻击者句柄与累计伤害。
    Entity m_source;
    Fixed m_damage;
    MG_REFLECT(AiThreat, m_source, m_damage)
};
// 每个怪物独占的决策状态；目标由 SkillPool 持有。
struct AiAgent
{
    // 进入警戒后不再清除。
    bool m_alerted = false;
    // 上次启动的规则动作。
    ai::AiAction m_lastAction = ai::AiAction::Idle;
    // 每行规则剩余冷却，按需扩展到 kMonsterRules.size()。
    std::vector<Fixed> m_ruleCooldownMs;
    // 各自动施放槽位的公共间隔。
    std::vector<Fixed> m_skillIntervalMs;
    // 换目标累计毫秒。
    Fixed m_changeTargetMs{};
    // 稳定顺序的仇恨记录。
    std::vector<AiThreat> m_threat;
    // 引导可暂停本角色的 AI。
    bool m_enabled = true;
    MG_REFLECT(AiAgent, m_alerted, m_lastAction, m_ruleCooldownMs, m_skillIntervalMs, m_changeTargetMs, m_threat, m_enabled)
};
namespace ai
{
// AI 实体的临时视图，不存入组件、不拥有资源，生命周期限于单次调用。
class AiDriver
{
public:
    // 构造指向指定世界实例中某个 AI 实体的视图。
    AiDriver(World& world, Entity entity);
    // 在角色 AI 更新阶段执行决策、嘲讽和十秒仇恨切换。
    void update();
    // 记录命中伤害，不为非 AI 角色创建状态。
    void addThreat(Entity source, Fixed damage);
    // 选择严格高于当前目标权重的有效攻击者，然后清空本周期记录。
    void changeTarget();
    // 按地图四侧边界和角色半径修正目标点。
    FixedVec3 calcPos(FixedVec3 point) const;
    // 以固定创建顺序收集存活、启用且可命中的敌对阵营组角色。
    std::vector<Entity> candidates() const;
    // 从当前组件和 AI 配置生成决策快照；不改状态，也不消耗随机数。
    AiFacts gather() const;

private:
    // 存活句柄和死亡状态共同限制跨实体访问。
    bool _alive(Entity entity) const;
    // 不可命中状态不参与普通 AI 目标选择。
    bool _hittable(Entity entity) const;
    // 获取当前角色配置，悬空引用由入口拒绝。
    const conf::AiConfig* _config() const;
    // 死亡、受击和失控时不能执行主动决策动作。
    bool _canAct() const;
    // 先设置技能控制角，成功释放后再更新角色朝向。
    void _direction(bool apply);
    // 按警戒、激怒和候选者选择索敌目标。
    void _selectTarget(const conf::AiConfig& row);
    // 先看已锁定目标再按创建顺序看候选者，用带符号的索敌区间；没有目标时返回自身哨兵。
    Entity _sightTarget(const conf::AiConfig& row) const;
    // 执行命中规则的动作，返回动作是否真的启动。
    bool _run(AiAction action, const conf::AiConfig& row);
    // X/Z 使用手动施放门槛，已有技能时续接原槽。
    bool _castSpecial(bool first);
    // 普通槽按优先级降序、原索引升序检查，成功后重置同优先级间隔。
    bool _castBest(const conf::AiConfig& row);
    // 巡逻和游走先抽 X 再抽 Z；追击不消耗随机流。
    bool _move(AiAction action, const conf::AiConfig& row);
    // 受击、仇恨记录或有效嘲讽都算激怒。
    bool _provoked() const;
    // 追击范围全零、重合或落在范围内时都不算范围外。
    bool _outsideChase(Entity target, const conf::AiConfig& row) const;

    World& m_world;
    Entity m_entity{};
};
}  // namespace ai
}  // namespace mugen::sim
