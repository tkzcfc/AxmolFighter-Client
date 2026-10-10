#pragma once

#include "mugen/core/math/DamageBox.h"
#include "mugen/core/reflect/Reflect.h"

#include <string>
#include <vector>

NS_MG_BEGIN

// 战斗时间轴轨道的框体类别。
enum class CombatTrackKind : int8_t
{
    Attack = 0,
    Damage = 1,
    Hitbox = 2,
};

// 时间轴上一次框体状态变更。
struct CombatKey
{
    // 关键帧时间、框体存在标记与框体数据。
    int32_t timeMs = 0;
    bool hasBox = false;
    DamageBox box;

    MG_REFLECT(CombatKey, timeMs, hasBox, box)
};

// 同类战斗框的命名时间轨道。
struct CombatTrack
{
    // 轨道名称、类别和有序关键帧。
    std::string name;
    CombatTrackKind kind = CombatTrackKind::Damage;
    std::vector<CombatKey> keys;

    MG_REFLECT(CombatTrack, name, kind, keys)
};

// 时间轴中触发的一条逻辑事件。
struct CombatEvent
{
    // 触发时间、事件类型和载荷文本。
    int32_t timeMs = 0;
    std::string type;
    std::string value;

    MG_REFLECT(CombatEvent, timeMs, type, value)
};

// 查询战斗框和逻辑事件的时间轴资源。
class CombatTimeline
{
public:
    // 收集指定时刻生效的攻击框和受击框。
    void boxesAt(int timeMs, std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const;
    // 收集开区间内发生的事件。
    void eventsBetween(int t0, int t1, std::vector<const CombatEvent*>& out) const;

    // 总时长、框轨道、事件和源路径。
    int32_t duration = 0;
    std::vector<CombatTrack> tracks;
    std::vector<CombatEvent> events;
    std::string sourcePath;

    MG_REFLECT(CombatTimeline, duration, tracks, events, sourcePath)

private:
    // 查找指定时间之前最后一个关键帧。
    static const CombatKey* keyAtOrBefore(const CombatTrack& track, int timeMs);
};

NS_MG_END
