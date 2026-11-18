#pragma once
#include "mugen/core/reflect/Reflect.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Fixed.h"
#include <vector>

namespace mugen::sim
{
// 角色当前资源及其上限。
struct HealthComponent
{
    // 当前生命值。
    Fixed hp;
    // 当前魔法值。
    Fixed mp = Fixed::fromInt(100);
    // 当前能量值。
    Fixed ep;
    // 生命上限。
    Fixed maxHp;
    // 魔法上限。
    Fixed maxMp = Fixed::fromInt(100);
    // 能量上限。
    Fixed maxEp = Fixed::fromInt(1000);
    // 护盾剩余值与消耗倍率。
    Fixed shield;
    Fixed shieldConsumeScale = Fixed::fromInt(1);
    // 锁血下限。
    Fixed hpLock;
    // 是否允许损失生命或接受打击。
    bool beHitEnabled = true;
    // 是否已判定死亡。
    bool dead = false;
    // 上一帧采样的资源（供事件条件读取）。
    Fixed prevHp;
    Fixed prevEp;
    // 判断生命耗尽且具备有效上限。
    bool isDead() const { return hp <= Fixed{} && maxHp > Fixed{}; }
    MG_REFLECT(HealthComponent, hp, mp, ep, maxHp, maxMp, maxEp, shield, shieldConsumeScale, hpLock,
               beHitEnabled, dead, prevHp, prevEp)
};
// 顿帧 / 定身计时（暂停期间推进类系统跳过）。
struct PauseComponent
{
    // 顿帧剩余毫秒。
    Fixed freezeMs;
    // 定身剩余毫秒。
    Fixed staticMs;
    // 本帧是否处于暂停（顿帧或定身任一大于 0）。
    bool paused = false;
    // 定身复位时间（毫秒）。
    Fixed staticResetAtMs;
    MG_REFLECT(PauseComponent, freezeMs, staticMs, paused, staticResetAtMs)
};
// 上一帧的接触目标集合（碰撞推导 Began/Continue/End 使用）。
struct ContactMemoryComponent
{
    // 上一帧仍在接触的实体。
    std::vector<Entity> contacts;
    MG_REFLECT(ContactMemoryComponent, contacts)
};
}  // namespace mugen::sim
