// [8.1] CombatSystem
// 读：`ContactEvent`、`EffectInfo`、`HitTracker`、`HitDef`、`Stats`、`StatusFlags`、`RoleState`、`SkillCast`、`Health`、`Identity`、`Transform`
// 写：`HitTracker`、`Health`（经 health::rules）、`Pause`、`HitState`(受击请求)、`SkillCast`(强制打断)、`Transform`(受击朝向)
// 消费：`ContactEvent`(特效→角色)
// 产生：`HitEvent`、`HpEvent`、`DeathRequest`、`HitStopEvent`、`SpawnRequest`(受击特效)、`DespawnRequest`、`CastEvent`(Cancelled)；RNG
#include "mugen/sim/Systems.h"
#include "mugen/sim/World.h"
#include "mugen/sim/combat/CombatRules.h"
#include <algorithm>
#include <vector>

namespace mugen::sim
{
namespace
{
// 实体创建序（each 的遍历序），供 R5 排序。
size_t creationRank(const std::vector<Entity>& created, Entity entity)
{
    const auto it = std::find(created.begin(), created.end(), entity);
    return it == created.end() ? created.size() : static_cast<size_t>(it - created.begin());
}
}  // namespace

// 系统 CombatSystem：8.1 行，见 DESIGN.md §6.6。只收集并按 R5 排序接触，逐个走完命中微管线。
void runCombat(World& world)
{
    std::vector<Entity> created;
    world.ecs().each<Identity>([&](Entity entity, const Identity&) { created.push_back(entity); });
    auto contacts = world.ecs().singleton<EventQueue<ContactEvent>>().m_items;
    std::stable_sort(contacts.begin(), contacts.end(), [&](const ContactEvent& a, const ContactEvent& b) {
        const size_t attacker = creationRank(created, a.m_a);
        const size_t other    = creationRank(created, b.m_a);
        if (attacker != other)
            return attacker < other;
        return creationRank(created, a.m_b) < creationRank(created, b.m_b);
    });

    for (const ContactEvent& contact : contacts)
    {
        if (contact.m_phase == ContactPhase::End)
            continue;
        combat::rules::HitContext ctx;
        if (!combat::rules::gate(world, contact, ctx))
            continue;
        combat::rules::recordHit(world, ctx);
        // S2: trigger EvadeSuccess on invulnerable or an active dodge skill (steps 3-4).
        if (combat::rules::resolveDodge(world, ctx))
            continue;
        // S2: trigger BeforeToBeHit (step 6). hit_type == -1 skips damage but still spawns hit effects.
        // S2: trigger BeforeHit (step 7).
        // S2: trigger op probability draws (R15, after dodge, before crit).
        combat::rules::computeDamage(world, ctx);
        combat::rules::applyDamage(world, ctx);
        const bool superArmor = world.ecs().has<StatusFlags>(ctx.m_target) &&
                                world.ecs().get<StatusFlags>(ctx.m_target).active(mugen::ExtraState::SuperArmor);
        if (!superArmor)
        {
            combat::rules::applyHitStop(world, ctx);
            combat::rules::requestHitReaction(world, ctx);
        }
        // S2: trigger rage and partner rage (step 11).
        // S2: trigger AfterToBeHit / AfterHit (step 12).
        // S2: trigger debuff admit when the target is not dodging (step 13).
        combat::rules::emitHit(world, ctx);
        combat::rules::spawnHitEffects(world, ctx);
        // S2: trigger CueEvent from the target sound_type (step 15).
    }
}
}  // namespace mugen::sim
