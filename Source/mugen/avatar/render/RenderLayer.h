#pragma once

#include "mugen/core/StdC.h"
#include "mugen/avatar/AvatarLayerDef.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

// 渲染层抽象（当前实现：SpineLayer；保留以便后续扩展 FrameAniLayer 等）
//
// 时间契约：
// - durationMs()：本层自身时长；容器（Avatar）取各层最大值作为全局时长。
// - seek/step：当 timeMs >= durationMs() 且非层内循环时，保持末帧显示（短层冻结）。
// - setMotion(..., loop)：loop 表示容器是否循环；实现层不应在时长短于容器时自行 loop，
//   由 Avatar 全局 wrap + seek(绝对时间) 负责重启。未来 FrameAniLayer 同样在 seek/step
//   里对时间 min(t, durationMs) 后显示最后一帧。
class RenderLayer : public ax::Node
{
public:
    // 切换动作（loop 为容器意图，层内动画轨道默认非自循环）
    virtual bool setMotion(const std::string& motionName, const std::string& entryId, bool loop) = 0;

    // 推进显示时间；实现应与 seek(current+dt) 语义一致（含末帧冻结）
    virtual void step(int dtMs) = 0;

    // 跳到绝对时间点（对齐/校正）；超出本层时长时冻末帧
    virtual void seek(int timeMs) = 0;

    // 本层动画时长（毫秒）
    virtual int durationMs() const = 0;

    // 本层当前时间
    virtual int currentTimeMs() const = 0;

    // 层来源标识
    MG_SYNTHESIZE(int32_t, m_layerTag, LayerTag)

    RenderLayer() : m_layerTag(kAvatarLayerTagCharacter) {}
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
