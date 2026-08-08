#pragma once

#include "mugen/core/MacroDefinition.h"
#include "mugen/core/math/DamageBox.h"

#include <string>
#include <vector>

NS_MG_BEGIN

// 时间轴的轨道类型
enum class CombatTrackKind
{
    // 攻击类型
    Attack,
    // 受击类型
    Damage,
    // 中性碰撞几何
    Hitbox,
};

// 时间轴上的单个关键关键帧
class CombatKey
{
public:
    CombatKey() : m_timeMs(0), m_hasBox(false) {}

    // 这一帧的时间点（毫秒）
    MG_SYNTHESIZE(int, m_timeMs, TimeMs);
    // 这个帧上是否有碰撞盒，若为 false 则 m_box 无效
    MG_SYNTHESIZE_IS(bool, m_hasBox, HasBox);
    // 本关键帧上的碰撞盒
    MG_SYNTHESIZE_PASS_BY_REF(DamageBox, m_box, Box);
};

// 时间轴上的轨道
class CombatTrack
{
public:
    CombatTrack() : m_kind(CombatTrackKind::Damage) {}

    // 轨道名称
    MG_SYNTHESIZE_PASS_BY_REF(std::string, m_name, Name);
    // 轨道类型
    MG_SYNTHESIZE(CombatTrackKind, m_kind, Kind);
    // 轨道包含的关键帧列表，按时间升序排列
    MG_SYNTHESIZE_PASS_BY_REF(std::vector<CombatKey>, m_keys, Keys);
};

// 时间轴上的事件
class CombatEvent
{
public:
    CombatEvent() : m_timeMs(0) {}

    // 事件触发的时间点（毫秒）
    MG_SYNTHESIZE(int, m_timeMs, TimeMs);
    // 事件类型
    MG_SYNTHESIZE_PASS_BY_REF(std::string, m_type, Type);
    // 事件值
    MG_SYNTHESIZE_PASS_BY_REF(std::string, m_value, Value);
};

// 时间轴
class CombatTimeline
{
public:
    CombatTimeline() : m_duration(0) {}

    // 加载编辑器生成的.box文件
    bool load(const std::string& path);

    // 总时长（毫秒）
    MG_SYNTHESIZE_READONLY(int, m_duration, Duration);
    // 轨道列表
    MG_SYNTHESIZE_READONLY_BY_REF(std::vector<CombatTrack>, m_tracks, Tracks);
    // 事件列表
    MG_SYNTHESIZE_READONLY_BY_REF(std::vector<CombatEvent>, m_events, Events);
    // 源文件路径(用于同步)
    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_sourcePath, SourcePath);

    // 采样指定时间点的碰撞盒（只追加；调用方负责传入干净 vector）
    void boxesAt(int timeMs, std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const;

    // 收集 [t0, t1) 内事件（只追加；调用方负责传入干净 vector）
    void eventsBetween(int t0, int t1, std::vector<const CombatEvent*>& out) const;

private:
    static const CombatKey* keyAtOrBefore(const CombatTrack& track, int timeMs);
};

NS_MG_END
