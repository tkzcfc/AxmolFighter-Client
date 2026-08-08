#pragma once

#include "mugen/core/MacroDefinition.h"

#include <string>
#include <vector>

NS_MG_BEGIN

// 帧数据
class AniFrame
{
public:
    AniFrame()
        : m_delay(0)
        , m_offsetX(0.0f)
        , m_offsetY(0.0f)
        , m_anchorX(0.5f)
        , m_anchorY(0.5f)
        , m_scale(1.0f)
        , m_rotation(0.0f)
    {}

    // 帧图片路径
    MG_SYNTHESIZE_PASS_BY_REF(std::string, m_imagePath, ImagePath);
    // 这一帧的持续时间（毫秒）
    MG_SYNTHESIZE(int, m_delay, Delay);
    // 帧偏移 X
    MG_SYNTHESIZE(float, m_offsetX, OffsetX);
    // 帧偏移 Y
    MG_SYNTHESIZE(float, m_offsetY, OffsetY);
    // 图片锚点 X
    MG_SYNTHESIZE(float, m_anchorX, AnchorX);
    // 图片锚点 Y
    MG_SYNTHESIZE(float, m_anchorY, AnchorY);
    // 缩放
    MG_SYNTHESIZE(float, m_scale, Scale);
    // 旋转
    MG_SYNTHESIZE(float, m_rotation, Rotation);
};

// 动画数据
class AniData
{
public:
    AniData() = default;

    // 加载编辑器生成的.box文件
    bool load(const std::string& path);

    // 帧列表
    MG_SYNTHESIZE_READONLY_BY_REF(std::vector<AniFrame>, m_frames, Frames);
    // 源文件路径(用于同步)
    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_sourcePath, SourcePath);

    // 计算总时长（毫秒），无帧返回 0
    int totalDurationMs() const;

    // 根据时间计算帧索引（毫秒）。越界夹到首/尾帧；循环由播放入口决定
    int frameIndexAtTime(int timeMs) const;
};

NS_MG_END
