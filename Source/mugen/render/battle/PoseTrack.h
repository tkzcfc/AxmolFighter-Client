#pragma once

// 表现层的位置插值轨迹与回滚纠正平滑。只含浮点数学、不依赖引擎，客户端表现层与无头联网测试共用；
// 它只读取模拟位置，从不写回模拟状态。

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mugen
{

// 渲染单位的位置。
struct VisualPos
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// 渲染帧内相对最近一次逻辑 tick 的插值进度。
// 联网模式下逻辑 tick 由时钟估计驱动、与渲染帧不同相，进度必须从 tick 发生的那一刻起算：
// 若用独立的墙钟锯齿，进度回绕与 tick 错开时位置会在一个逻辑帧内来回跳（重影）。
class TickAlpha
{
public:
    // 每个渲染帧调用一次：本帧推进过逻辑帧则进度从 0 起算，否则累加渲染帧时长。
    void onRenderFrame(float deltaSec, bool ticked)
    {
        if (ticked)
            m_elapsedSec = 0.0f;
        else
            m_elapsedSec += deltaSec;
    }
    // 返回 [0, 1] 的插值进度；frameSec 为逻辑帧时长。tick 迟到时停在 1，不会回绕。
    float alpha(float frameSec) const { return std::min(1.0f, m_elapsedSec / frameSec); }

private:
    // 距最近一次逻辑 tick 的累计时长（秒）。
    float m_elapsedSec = 0.0f;
};

// 单个实体的位置轨迹：previous / current 为相邻两个逻辑帧的模拟位置，渲染帧按进度在两者间插值；
// 回滚把实体位置改写时，轨迹整体平移并记下反向偏移，显示位置保持连续，偏移随后指数衰减归零，
// 把原本的硬拉回变成短时间的平滑修正。
class PoseTrack
{
public:
    // 位置跳变超过该距离视为瞬移，直接吸附（渲染单位）。
    static constexpr float kTeleportDistance = 500.0f;
    // 逻辑帧一次推进超过该帧数视为跳帧（重同步等），直接吸附；追帧的几帧仍平滑插值。
    static constexpr uint64_t kMaxInterpolatedGap = 8;
    // 纠正偏移的衰减时间常数（秒）。
    static constexpr float kCorrectionTauSec = 0.1f;
    // 纠正偏移超过该距离直接清零（渲染单位）：这么大的修正平滑只会拖出长长的重影。
    static constexpr float kMaxCorrection = 400.0f;

    // 采样最新逻辑帧的模拟位置；仅在帧号变化时滚动 previous / current。
    void sample(uint64_t frame, const VisualPos& pos)
    {
        if (!m_has)
        {
            m_previous = m_current = pos;
            m_frame                = frame;
            m_has                  = true;
            return;
        }
        if (frame == m_frame)
            return;
        const float dx    = pos.x - m_current.x;
        const float dy    = pos.y - m_current.y;
        const float dz    = pos.z - m_current.z;
        const bool backwards = frame < m_frame;
        const bool snap = backwards || frame - m_frame > kMaxInterpolatedGap ||
                          dx * dx + dy * dy + dz * dz > kTeleportDistance * kTeleportDistance;
        m_previous = snap ? pos : m_current;
        m_current  = pos;
        m_frame    = frame;
        if (snap)
            m_offset = {};
    }

    // 回滚把该实体当前帧的模拟位置改写了 shift（新 - 旧）：整条轨迹平移，偏移记反向量，显示位置不变。
    void applyCorrection(const VisualPos& shift)
    {
        if (!m_has)
            return;
        m_previous.x += shift.x;
        m_previous.y += shift.y;
        m_previous.z += shift.z;
        m_current.x += shift.x;
        m_current.y += shift.y;
        m_current.z += shift.z;
        m_offset.x -= shift.x;
        m_offset.y -= shift.y;
        m_offset.z -= shift.z;
        if (offsetLength() > kMaxCorrection)
            m_offset = {};
    }

    // 按渲染帧时长衰减纠正偏移。
    void advance(float deltaSec)
    {
        const float keep = std::exp(-deltaSec / kCorrectionTauSec);
        m_offset.x *= keep;
        m_offset.y *= keep;
        m_offset.z *= keep;
        if (offsetLength() < 0.05f)
            m_offset = {};
    }

    // 当前显示位置：previous 到 current 按 alpha 插值，再叠加纠正偏移。
    VisualPos display(float alpha) const
    {
        return {m_previous.x + (m_current.x - m_previous.x) * alpha + m_offset.x,
                m_previous.y + (m_current.y - m_previous.y) * alpha + m_offset.y,
                m_previous.z + (m_current.z - m_previous.z) * alpha + m_offset.z};
    }

    // 是否已有采样。
    bool hasSample() const { return m_has; }
    // 当前未衰减完的纠正偏移长度。
    float offsetLength() const
    {
        return std::sqrt(m_offset.x * m_offset.x + m_offset.y * m_offset.y + m_offset.z * m_offset.z);
    }

private:
    // 上一逻辑帧与最新逻辑帧的位置（已含纠正平移）。
    VisualPos m_previous;
    VisualPos m_current;
    // 尚未衰减完的纠正偏移。
    VisualPos m_offset;
    // 最新采样对应的逻辑帧号。
    uint64_t m_frame = 0;
    // 是否已建立采样。
    bool m_has = false;
};

}  // namespace mugen
