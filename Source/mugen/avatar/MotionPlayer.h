#pragma once

#include "mugen/avatar/data/MotionMap.h"
#include "mugen/core/MacroDefinition.h"
#include "mugen/core/reflect/Reflect.h"

#include <string>
#include <vector>

NS_MG_BEGIN

// 推进动作时间轴并收集碰撞框和事件的逻辑播放器。
class MotionPlayer
{
public:
    // 构造未绑定动作资源的播放器。
    MotionPlayer();
    // 绑定动作图资源。
    bool bind(const std::string& motionFile);
    // 解除资源和当前播放状态。
    void unbind();
    bool isBound() const { return m_motionMap != nullptr; }
    // 从指定片段开始播放动作。
    bool play(const std::string& motionName, const std::string& entryId, bool loop);
    // 返回给定索引处的动作名称。
    std::string motionNameAt(size_t index) const;
    // 推进时间并收集经过的战斗事件。
    void step(int dtMs, std::vector<const CombatEvent*>* outEvents = nullptr);
    // 定位到当前动作内的绝对时间。
    void seek(int timeMs);
    // 停止当前动作播放。
    void stop();
    // 收集当前时间的攻击框与受击框。
    void boxesAt(std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const;
    // 判断非循环动作是否播放完毕。
    bool isFinished() const;
    int getDurationMs() const { return m_motion ? m_motion->durationMs() : 0; }
    const std::string& getMotionFile() const { return m_motionFile; }
    const std::string& getCurrentMotionName() const { return m_motionName; }
    const std::string& getCurrentEntryId() const { return m_entryId; }
    int getCurrentTimeMs() const { return m_timeMs; }
    bool isLoop() const { return m_loop; }
    bool isPlaying() const { return m_playing; }

private:
    // 收集给定时间区间内的事件。
    void collectEventsRange(int t0, int t1, std::vector<const CombatEvent*>* out) const;
    // 按事件时间稳定排序。
    static void sortEventsByTime(std::vector<const CombatEvent*>& events);

    // 当前绑定的动作图路径。
    std::string m_motionFile;
    // 当前动作名称。
    std::string m_motionName;
    // 当前起始片段 id。
    std::string m_entryId;
    // 当前动作内时间毫秒数。
    int32_t m_timeMs = 0;
    // 当前动作是否循环。
    bool m_loop = false;
    // 当前是否处于播放状态。
    bool m_playing = false;
    // 绑定的动作图缓存。
    const MotionMap* m_motionMap = nullptr;
    // 当前播放的动作定义。
    const Motion* m_motion = nullptr;

public:
    MG_REFLECT(MotionPlayer, m_motionFile, m_motionName, m_entryId, m_timeMs, m_loop, m_playing)
};

NS_MG_END
