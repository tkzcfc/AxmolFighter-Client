#pragma once

#include "mugen/avatar/data/CombatTimeline.h"
#include "mugen/core/MacroDefinition.h"
#include "mugen/core/Object.h"

NS_MG_BEGIN

// 动画类型
enum class MotionEntryType : int8_t
{
    kAni   = 0,
    kSpine = 1,
};

// 单个动作条目（序列化）
class MotionEntry : public Object
{
public:
    typedef Object Super;

public:
    MotionEntry() {}
    virtual ~MotionEntry() {}

public:
    // 条目 id
    std::string id;
    // ani / spine
    MotionEntryType type = MotionEntryType::kAni;
    // type=ani 时为动画文件名；type=spine 时为 Spine 动画名
    std::string source;
    // 碰撞盒路径
    std::string boxPath;

public:
    MG_DEFINE_SERIALIZABLE(id, type, source, boxPath)
};

// 序列化后的 Motion 定义
class MotionDef : public Object
{
public:
    typedef Object Super;

public:
    MotionDef() {}
    virtual ~MotionDef() {}

public:
    // Motion 名称
    std::string name;
    // 一个 Motion 可能由多个动画组成
    std::vector<MotionEntry> entries;

public:
    MG_DEFINE_SERIALIZABLE(name, entries)
};

struct MotionClip
{
    std::string id;
    MotionEntryType type = MotionEntryType::kAni;
    std::string source;
    const CombatTimeline* timeline = nullptr;
    int startMs                    = 0;
    int durationMs                 = 0;
};

// 运行时 Motion：不参与序列化，由 MotionMap 在反序列化后绑定 CombatTimeline*
class Motion
{
public:
    int durationMs() const { return duration; }
    int startTimeMs(const std::string& entryId) const;
    size_t clipCount() const { return clips.size(); }
    const MotionClip* clipAtIndex(size_t index) const;
    const MotionClip* clipAt(int timeMs, int* localMs, size_t* index = nullptr) const;

    void boxesAt(int timeMs, std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const;
    void eventsBetween(int t0, int t1, std::vector<const CombatEvent*>& out) const;

public:
    std::string name;
    std::vector<MotionClip> clips;
    int duration = 0;
};

// 动画映射表
class MotionMap : public Object
{
public:
    typedef Object Super;

public:
    MotionMap() {}
    virtual ~MotionMap() {}

    const Motion* findMotion(const std::string& name) const;
    const Motion* motionAt(size_t index) const;

    // 按 defs 构建运行时 Motion，timeline 由 lookup(boxPath) 解析
    bool bindTimelines(const std::function<const CombatTimeline*(const std::string&)>& lookup);

private:
    std::vector<Motion> m_motions;
    std::unordered_map<std::string, size_t> m_nameToIndex;

public:
    std::vector<MotionDef> defs;
    std::string sourcePath;

public:
    MG_DEFINE_SERIALIZABLE(defs, sourcePath)
};

NS_MG_END
