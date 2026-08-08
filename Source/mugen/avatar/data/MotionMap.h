#pragma once

#include "mugen/core/MacroDefinition.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

NS_MG_BEGIN

// 动画类型
enum class MotionEntryType : int8_t
{
    kAni   = 0,
    kSpine = 1,
};

// 单个动作条目
class MotionEntry
{
public:
    MotionEntry() : m_type(MotionEntryType::kAni) {}

    // 条目 id
    MG_SYNTHESIZE_PASS_BY_REF(std::string, m_id, Id);
    // ani / spine
    MG_SYNTHESIZE(MotionEntryType, m_type, Type);
    /**
     * 通用引用：type=ani 时为动画的文件名；
     * type=spine 时为 Spine 动画名。
     */
    MG_SYNTHESIZE_PASS_BY_REF(std::string, m_source, Source);
    /**
     * 碰撞盒文件名
     */
    MG_SYNTHESIZE_PASS_BY_REF(std::string, m_boxPath, BoxPath);
};

// 单个Motion定义
class MotionDefinition
{
public:
    MotionDefinition() = default;

    // Motion名称
    MG_SYNTHESIZE_PASS_BY_REF(std::string, m_name, Name);
    // 一个Motion可能由多个动画组成
    MG_SYNTHESIZE_PASS_BY_REF(std::vector<MotionEntry>, m_entries, Entries);
};

// 动画映射表
// 为了统一Spine类型和帧动画类型的动画,单独定义了一个MotionMap,用于存储所有的动画定义
// 并且将动画和碰撞盒的映射关系存储在MotionEntry中,和动画分离
// 播放时统一使用.motion文件里面定义的名称
class MotionMap
{
public:
    MotionMap() = default;

    // 加载编辑器生成的.motion文件
    bool load(const std::string& path);

    // 全部 motion 定义
    MG_SYNTHESIZE_READONLY_BY_REF(std::vector<MotionDefinition>, m_motions, Motions);
    // 源文件路径(用于同步)
    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_sourcePath, SourcePath);

    const MotionDefinition* findMotion(const std::string& name) const;
    const MotionEntry* findEntry(const std::string& motionName, const std::string& entryId) const;
    const MotionEntry* entryAt(const std::string& motionName, size_t index) const;

private:
    // motion 名 -> motions 下标
    std::unordered_map<std::string, size_t> m_nameToIndex;
};

NS_MG_END
