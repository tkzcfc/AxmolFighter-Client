#pragma once

#include "mugen/core/MacroDefinition.h"
#include "mugen/core/math/DamageBox.h"

#include <string>
#include <vector>

NS_MG_BEGIN

// 时间轴的轨道类型
enum class CombatTrackKind : int8_t
{
    // 攻击类型
    Attack = 0,
    // 受击类型
    Damage = 1,
    // 中性碰撞几何
    Hitbox = 2,
};

// 时间轴上的单个关键帧
class CombatKey : public Object
{
public:
    typedef Object Super;

public:
    CombatKey() {}
    virtual ~CombatKey() {}

public:
    // 这一帧的时间点（毫秒）
    int timeMs = 0;
    // 这个帧上是否有碰撞盒，若为 false 则 box 无效
    bool hasBox = false;
    // 本关键帧上的碰撞盒
    DamageBox box;

public:
    MG_DEFINE_SERIALIZABLE(timeMs, hasBox, box)
};

// 时间轴上的轨道
class CombatTrack : public Object
{
public:
    typedef Object Super;

public:
    CombatTrack() {}
    virtual ~CombatTrack() {}

public:
    // 轨道名称
    std::string name;
    // 轨道类型
    CombatTrackKind kind = CombatTrackKind::Damage;
    // 轨道包含的关键帧列表，按时间升序排列
    std::vector<CombatKey> keys;

public:
    MG_DEFINE_SERIALIZABLE(name, kind, keys)
};

// 时间轴上的事件
class CombatEvent : public Object
{
public:
    typedef Object Super;

public:
    CombatEvent() {}
    virtual ~CombatEvent() {}

public:
    // 事件触发的时间点（毫秒）
    int timeMs = 0;
    // 事件类型
    std::string type;
    // 事件值
    std::string value;

public:
    MG_DEFINE_SERIALIZABLE(timeMs, type, value)
};

// 时间轴
class CombatTimeline : public Object
{
public:
    typedef Object Super;

public:
    CombatTimeline() {}
    virtual ~CombatTimeline() {}

    // 采样指定时间点的碰撞盒（只追加；调用方负责传入干净 vector）
    void boxesAt(int timeMs, std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const;

    // 收集 [t0, t1) 内事件（只追加；调用方负责传入干净 vector）
    void eventsBetween(int t0, int t1, std::vector<const CombatEvent*>& out) const;

private:
    static const CombatKey* keyAtOrBefore(const CombatTrack& track, int timeMs);

public:
    // 总时长（毫秒）
    int duration = 0;
    // 轨道列表
    std::vector<CombatTrack> tracks;
    // 事件列表
    std::vector<CombatEvent> events;
    // 源文件路径
    std::string sourcePath;

public:
    MG_DEFINE_SERIALIZABLE(duration, tracks, events, sourcePath)
};

NS_MG_END
