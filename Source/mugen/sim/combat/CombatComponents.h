#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include <vector>

namespace mugen::sim
{
// 角色当前资源及其上限。
struct Health
{
    // 当前生命值。
    Fixed m_hp;
    // 当前魔法值。
    Fixed m_mp = Fixed::fromInt(100);
    // 当前能量值。
    Fixed m_ep;
    // 生命上限。
    Fixed m_maxHp;
    // 魔法上限。
    Fixed m_maxMp = Fixed::fromInt(100);
    // 能量上限。
    Fixed m_maxEp = Fixed::fromInt(1000);
    // 护盾剩余值与消耗倍率。
    Fixed m_shield;
    Fixed m_shieldConsumeScale = Fixed::fromInt(1);
    // 锁血下限。
    Fixed m_hpLock;
    // 是否允许损失生命或接受打击。
    bool m_beHitEnabled = true;
    // 是否已判定死亡。
    bool m_dead = false;
    // 上一帧采样的资源（供事件条件读取）。
    Fixed m_prevHp;
    Fixed m_prevEp;
    // 判断生命耗尽且具备有效上限。
    bool isDead() const { return m_hp <= Fixed{} && m_maxHp > Fixed{}; }
    MG_REFLECT(Health, m_hp, m_mp, m_ep, m_maxHp, m_maxMp, m_maxEp, m_shield, m_shieldConsumeScale, m_hpLock,
               m_beHitEnabled, m_dead, m_prevHp, m_prevEp)
};
// 顿帧 / 定身计时（暂停期间推进类系统跳过）。
struct Pause
{
    // 顿帧剩余毫秒。
    Fixed m_freezeMs;
    // 定身剩余毫秒。
    Fixed m_staticMs;
    // 本帧是否处于暂停（顿帧或定身任一大于 0）。
    bool m_paused = false;
    // 定身复位时间（毫秒）。
    Fixed m_staticResetAtMs;
    MG_REFLECT(Pause, m_freezeMs, m_staticMs, m_paused, m_staticResetAtMs)
};
// 上一帧的接触目标集合（碰撞推导 Began/Continue/End 使用）。
struct ContactMemory
{
    // 上一帧仍在接触的实体。
    std::vector<Entity> m_contacts;
    MG_REFLECT(ContactMemory, m_contacts)
};
}  // namespace mugen::sim
