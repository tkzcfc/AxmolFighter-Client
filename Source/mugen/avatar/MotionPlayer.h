#pragma once

#include "mugen/avatar/data/MotionMap.h"
#include "mugen/core/Object.h"

#include <string>
#include <vector>

NS_MG_BEGIN

// 逻辑动作播放器：绑定一个 .motion，推进时间、采样碰撞盒、收集 .box 事件（客户端/战斗服共用）
class MotionPlayer : public Object
{
    typedef Object Super;

public:
    MotionPlayer();

    // 绑定 .motion（AvatarAssetCache 中的路径）；失败时保持未绑定。会停止当前播放
    bool bind(const std::string& motionFile);

    // 解除绑定并停止
    void unbind();

    // 是否已绑定 .motion
    bool isBound() const { return m_motionMap != nullptr; }

    // 播放动作；动作不存在则停止（避免每帧重试）
    bool play(const std::string& motionName, const std::string& entryId, bool loop);

    // 按下标取 motion 名（与 .motion 中 animations 顺序一致）；越界返回空
    std::string motionNameAt(size_t index) const;

    // 推进时间并收集事件
    void step(int dtMs, std::vector<const CombatEvent*>* outEvents = nullptr);

    // 绝对定位，不触发事件
    void seek(int timeMs);

    // 停止播放并清空当前动作
    void stop();

    // 当前时刻碰撞盒（本地坐标）
    void boxesAt(std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const;

    // 非循环且已播完
    bool isFinished() const;

    // 当前动作时长
    int getDurationMs() const { return m_motion ? m_motion->durationMs() : 0; }

    // 绑定的 .motion 路径
    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_motionFile, MotionFile)
    // 当前动作名
    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_motionName, CurrentMotionName)
    // 当前 entryId（空表示从第一段起播）
    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_entryId, CurrentEntryId)
    // 当前播放时间（毫秒）
    MG_SYNTHESIZE_READONLY(int, m_timeMs, CurrentTimeMs)
    // 是否循环
    MG_SYNTHESIZE_IS_READONLY(bool, m_loop, Loop)
    // 是否正在播放
    MG_SYNTHESIZE_IS_READONLY(bool, m_playing, Playing)

    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl,
                                  deserializeCustomImpl,
                                  m_motionFile,
                                  m_motionName,
                                  m_entryId,
                                  m_timeMs,
                                  m_loop,
                                  m_playing);

private:
    // 收集 [t0, t1) 事件
    void collectEventsRange(int t0, int t1, std::vector<const CombatEvent*>* out) const;

    // 按时间戳稳定排序事件
    static void sortEventsByTime(std::vector<const CombatEvent*>& events);

    void serializeCustomImpl(ByteBuffer& byteBuffer) const {}

    // 按 m_motionFile / m_motionName 恢复运行时指针
    bool deserializeCustomImpl(ByteBuffer& byteBuffer);

    const MotionMap* m_motionMap = nullptr;
    const Motion* m_motion       = nullptr;
};

NS_MG_END
