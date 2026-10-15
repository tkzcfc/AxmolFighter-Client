#pragma once
#include "mugen/sim/skill/SkillTriggers.h"

namespace mugen::sim
{
struct BuffContext;
// 命中事件上下文；同步触发允许免死事件改变当前生命。
struct HitTrigger
{
    // 显式默认构造使旧技能聚合参数不会误匹配命中重载。
    HitTrigger() = default;
    // 关联的攻击方、目标和特效。
    Entity m_attacker, m_target, m_effect;
    // 技能及槽首技能标识。
    int32_t m_skillId = -1, m_baseSkillId = -1;
    // 技能槽和技能属性来源。
    uint16_t m_slot = 0;
    Entity m_skillAttribute;
    // 神器来源。
    int32_t m_artifactId = -1, m_artifactIndex = -1;
    // 实际公式伤害及暴击标记。
    Fixed m_damage;
    bool m_critical = false;
    // 目标为友方时启用友方事件过滤。
    bool m_teammate = false;
};
// Buff 池提供的治疗修正；不在命中层保存 Buff 实例。
struct HpBuffModifiers
{
    // 寒气禁止治疗。
    bool m_chill = false;
    // 治疗变化参数的总和。
    Fixed m_healingChange;
};
// 施加 Buff 所需来源；目标池与事件目标分开，事件目标保持空。
struct HitBuffApplication
{
    // 配置允许保留原始非整数值，由 Buff 所有者校验。
    Fixed m_buffId;
    // 命中来源上下文。
    HitTrigger m_source;
    // 显式实体装配传入的绑定技能和来源，不从命中特效推断。
    std::optional<int32_t> m_boundSkill, m_origin;
};
// 能力系统所拥有的命中通知，不在此执行能力规则。
enum class HitAbilityEvent : uint8_t { BeforeHit, AfterHit, AfterToBeHit, SameCamp, Anger, Death, Crystal, Obstacle };
// 世界实例持有的同步模块接缝，不进入状态快照。
struct HitHooks
{
    // 默认未接入 Buff 的事件接缝。
    static void poolTrigger(World&, Entity, BFEvent, const HitTrigger&);
    // 默认未接入 Buff 的施加接缝。
    static void poolApply(World&, Entity, const HitBuffApplication&);
    // 查询生产实例池中的配置标识。
    static bool poolHasBuff(World&, Entity, int32_t);
    // 向状态施加器转发行为类名。
    static void poolBehaviorState(World&, Entity, BFEvent, const char*);
    // 默认无治疗修正。
    static HpBuffModifiers noModifiers(World&, Entity);
    // 默认不以 EP/TP 代替生命扣除。
    static bool noReplacement(World&, Entity, Fixed);
    // 默认没有按层护盾；返回真表示最后一层被消耗。
    static bool noShieldLayer(World&, Entity);
    // 将命中生命周期转换为能力事件。
    static void noAbility(World&, Entity, HitAbilityEvent, const HitTrigger&, int32_t);
    // 转发 Buff 绑定的能力事件。
    static void boundAbility(World&, Entity, int32_t, BFEvent, const BuffContext&);
    // 在角色重生或死亡时重置能力池。
    static void roleAbilities(World&, Entity, bool);
    // 默认使用角色等级；技能属性系统可提供等级换算结果。
    static Fixed roleDamageLevel(World&, Entity, Entity);
    // 同步 Buff 事件入口。
    void (*m_trigger)(World&, Entity, BFEvent, const HitTrigger&) = poolTrigger;
    // Buff 施加入口。
    void (*m_apply)(World&, Entity, const HitBuffApplication&) = poolApply;
    // 查询寒气和治疗变化。
    HpBuffModifiers (*m_modifiers)(World&, Entity) = noModifiers;
    // 由 Buff 层执行 EP/TP 替代；返回是否接管。
    bool (*m_replaceDamage)(World&, Entity, Fixed) = noReplacement;
    // 消耗按层护盾的一层。
    bool (*m_shieldLayer)(World&, Entity) = noShieldLayer;
    // 能力与角色特殊生命周期接缝。
    void (*m_ability)(World&, Entity, HitAbilityEvent, const HitTrigger&, int32_t) = noAbility;
    // 技能属性的等级加成接缝。
    Fixed (*m_damageLevel)(World&, Entity, Entity) = roleDamageLevel;
    // 死亡掉落的生产入口。
    static void dropGoods(World&, Entity);
    void (*m_dropGoods)(World&, Entity) = dropGoods;
    // 查询真实 Buff 池中的保护效果，未接入时为空。
    bool (*m_hasBuff)(World&, Entity, int32_t) = poolHasBuff;
    // 角色死亡卸载能力、复活重载能力的接缝。
    void (*m_roleAbilities)(World&, Entity, bool) = roleAbilities;
    // 普通角色动作的开始和结束通知，名称交由 Buff 模块匹配。
    void (*m_behaviorState)(World&, Entity, BFEvent, const char*) = poolBehaviorState;
    // Buff 绑定能力的同步接缝，能力系统接管实例化与事件处理。
    void (*m_buffAbility)(World&, Entity, int32_t, BFEvent, const BuffContext&) = boundAbility;
};
}
