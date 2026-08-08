#pragma once

#include "mugen/core/StdC.h"
#include "RenderLayer.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class Avatar final : public ax::Node
{
public:
    // 创建空容器
    static Avatar* create();

    // 添加渲染层；播放中会同步当前动作
    void addLayer(RenderLayer* layer, int order, int32_t tag);

    // 按 tag 移除层
    void removeLayersByTag(int32_t tag);

    // 是否存在指定 tag 的层
    bool hasLayersWithTag(int32_t tag) const;

    // 广播切换动作
    void setMotion(const std::string& motionName, const std::string& entryId, bool loop);

    // 步进时间
    void step(int dtMs);

    // 跳跃到绝对时间点
    void seek(int timeMs);

    // 自动播放时转 step
    void update(float delta) override;

    // 各层时长最大值
    int durationMs() const;

    // 非循环且已播完
    bool isFinished() const;

    // 自身累计时间
    MG_SYNTHESIZE_READONLY(int, m_timeMs, CurrentTimeMs)
    // 当前动作名
    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_motionName, CurrentMotionName)
    // 当前 entryId
    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_entryId, CurrentEntryId)
    // 是否循环
    MG_SYNTHESIZE_IS_READONLY(bool, m_loop, Loop)
    // 是否自动播放（UI 预览；战斗中保持 false，由 AvatarRenderSystem 驱动）
    MG_SYNTHESIZE_WRITEONLY(bool, m_autoPlay, AutoPlay)

private:
    bool init() override;

    // 按循环/非循环规范化 m_timeMs
    void normalizeTime();

    // 渲染层列表（子节点）
    std::vector<RenderLayer*> m_layers;
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
