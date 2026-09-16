#pragma once

#include "mugen/core/StdC.h"
#include "mugen/avatar/AvatarLayerDef.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

// 渲染层抽象（当前实现：SpineLayer；保留以便后续扩展 FrameAniLayer 等）
//
// 时间契约：
// - durationMs()：本层自身时长；容器（Avatar）取各层最大值作为全局时长。
// - step：增量推进（Spine 走 update(dt)）；到本层时长后冻末帧。
// - seek：绝对跳转（对齐、校正、循环 wrap）；超出本层时长时冻末帧。
// - 层内动画轨道不自循环；由 Avatar 全局 wrap + seek(绝对时间) 负责重启。
class RenderLayer : public ax::Node
{
public:
    // 切换动作
    // @param motionName 动作名（对应 MotionMap 中的 motionName）
    // @param entryId 动作条目 id（在设计中一个动作可能由多个动画组合而成）；空表示取首个
    virtual bool setMotion(const std::string& motionName, const std::string& entryId) = 0;

    // 增量推进显示时间（冻末帧）；循环 wrap 由容器 seek 重启
    virtual void step(int dtMs) = 0;

    // 跳到绝对时间点（对齐/校正）；超出本层时长时冻末帧
    virtual void seek(int timeMs) = 0;

    // 本层动画时长（毫秒）
    virtual int durationMs() const = 0;

    // 本层当前时间
    virtual int currentTimeMs() const = 0;

    // 骨架是否已就绪（异步装配的层就绪前为 false；未就绪时 step/seek 只记录时间）
    virtual bool isReady() const = 0;

    // 层来源标识
    MG_SYNTHESIZE(AvatarLayerTag, m_layerTag, LayerTag)

    RenderLayer() : m_layerTag(AvatarLayerTag::kBody) {}
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
