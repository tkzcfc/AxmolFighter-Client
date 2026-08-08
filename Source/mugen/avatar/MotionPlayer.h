#pragma once

#include "mugen/avatar/MotionLayer.h"
#include "mugen/core/Object.h"

#include <string>
#include <vector>

NS_MG_BEGIN

// 逻辑动作播放器：管理多层、推进时间、合并碰撞盒与事件（客户端/战斗服共用）
class MotionPlayer : public Object
{
    typedef Object Super;

public:
    MotionPlayer();

    // 添加层；播放中会立即同步当前动作
    bool addLayer(const AvatarLayerDef& def);

    // 按 tag 移除层
    void removeLayersByTag(int32_t tag);

    // 清空全部层
    void clearLayers();

    // 当前层数量
    size_t layerCount() const { return m_layers.size(); }

    // 播放动作；任一层解析失败则停止（避免每帧重试）
    bool play(const std::string& motionName, const std::string& entryId, bool loop);

    // 覆盖时长（毫秒）。用于直通无 .box 时由时间轴估算驱动 animationFinished。
    void setDurationMs(int durationMs);

    // 推进时间并收集事件
    void step(int dtMs, std::vector<const CombatEvent*>* outEvents = nullptr);

    // 绝对定位，不触发事件
    void seek(int timeMs);

    // 停止播放并清空当前动作
    void stop();

    // 合并各层当前时刻碰撞盒（本地坐标）
    void boxesAt(std::vector<const DamageBox*>& outAttack, std::vector<const DamageBox*>& outDamage) const;

    // 非循环且已播完
    bool isFinished() const;

    // 当前动作名
    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_motionName, CurrentMotionName)
    // 当前 entryId（空表示各层取首个）
    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_entryId, CurrentEntryId)
    // 当前播放时间（毫秒）
    MG_SYNTHESIZE_READONLY(int, m_timeMs, CurrentTimeMs)
    // 是否循环
    MG_SYNTHESIZE_IS_READONLY(bool, m_loop, Loop)
    // 是否正在播放
    MG_SYNTHESIZE_IS_READONLY(bool, m_playing, Playing)
    // 当前动作时长（各层 duration 最大值，与渲染一致）
    MG_SYNTHESIZE_READONLY(int, m_durationMs, DurationMs)

    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl,
                                  deserializeCustomImpl,
                                  m_motionName,
                                  m_entryId,
                                  m_timeMs,
                                  m_loop,
                                  m_playing,
                                  m_durationMs);

private:
    // 按各层 duration 最大值重算
    void recomputeDuration();

    // 收集各层 [t0, t1) 事件
    void collectEventsRange(int t0, int t1, std::vector<const CombatEvent*>* out) const;

    // 对所有层执行当前动作 setMotion
    void applyMotionToLayers();

    // 按时间戳稳定排序事件
    static void sortEventsByTime(std::vector<const CombatEvent*>& events);

    // 序列化层描述列表
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    // 反序列化并重建层
    bool deserializeCustomImpl(ByteBuffer& byteBuffer);

    // 逻辑层列表
    std::vector<MotionLayer> m_layers;
};

NS_MG_END
