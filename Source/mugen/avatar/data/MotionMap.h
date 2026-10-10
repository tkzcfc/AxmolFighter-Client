#pragma once

#include "mugen/avatar/data/CombatTimeline.h"
#include "mugen/core/reflect/Reflect.h"

#include <functional>
#include <unordered_map>

NS_MG_BEGIN

// 动作片段使用的资源类型。
enum class MotionEntryType : int8_t
{
    kAni   = 0,
    kSpine = 1,
};

// 动作定义中的一个源资源片段。
struct MotionEntry
{
    // 片段标识、类型、资源路径和战斗框路径。
    std::string id;
    MotionEntryType type = MotionEntryType::kAni;
    std::string source;
    std::string boxPath;

    MG_REFLECT(MotionEntry, id, type, source, boxPath)
};

// 序列化的命名动作定义。
struct MotionDef
{
    // 动作名称和顺序片段。
    std::string name;
    std::vector<MotionEntry> entries;

    MG_REFLECT(MotionDef, name, entries)
};

// 已绑定时间轴后的可播放片段。
struct MotionClip
{
    // 标识、类型、资源、时间轴、起始时间和时长。
    std::string id;
    MotionEntryType type = MotionEntryType::kAni;
    std::string source;
    const CombatTimeline* timeline = nullptr;
    int32_t startMs                = 0;
    int32_t durationMs             = 0;
};

// 由多个片段组成的可播放动作。
class Motion
{
public:
    // 返回动作总时长。
    int durationMs() const { return duration; }
    // 返回指定片段的起始时间。
    int startTimeMs(const std::string& entryId) const;
    size_t clipCount() const { return clips.size(); }
    // 返回指定索引处的片段。
    const MotionClip* clipAtIndex(size_t index) const;
    // 查找指定时间的片段并输出片段内时间。
    const MotionClip* clipAt(int timeMs, int* localMs, size_t* index = nullptr) const;
    // 收集指定时间生效的战斗框。
    void boxesAt(int timeMs, std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const;
    // 收集指定时间区间的逻辑事件。
    void eventsBetween(int t0, int t1, std::vector<const CombatEvent*>& out) const;
    // 末片段对齐到骨架动画时长。有事件或更晚关键帧时拒绝，动作总时长保持不变。
    bool alignClipToSpine(size_t index, int32_t spineDurationMs);

    // 动作名称、片段和总时长。
    std::string name;
    std::vector<MotionClip> clips;
    int32_t duration = 0;
};

// 从动作名称查找已绑定动作的资源容器。
class MotionMap
{
public:
    // 按名称查找动作。
    const Motion* findMotion(const std::string& name) const;
    // 按索引查找动作。
    const Motion* motionAt(size_t index) const;
    // 使用查询回调绑定所有片段时间轴。
    bool bindTimelines(const std::function<const CombatTimeline*(const std::string&)>& lookup);

    // 原始动作定义和源资源路径。
    std::vector<MotionDef> defs;
    std::string sourcePath;

    MG_REFLECT(MotionMap, defs, sourcePath)

private:
    // 已展开的动作与名称索引。
    std::vector<Motion> m_motions;
    std::unordered_map<std::string, size_t> m_nameToIndex;
};

NS_MG_END
