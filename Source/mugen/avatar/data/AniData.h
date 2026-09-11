#pragma once

#include "mugen/core/MacroDefinition.h"
#include "mugen/core/Object.h"

#include <string>
#include <vector>

NS_MG_BEGIN

// 帧数据
class AniFrame : public Object
{
public:
    typedef Object Super;

public:
    AniFrame() {}
    virtual ~AniFrame() {}

public:
    // 帧图片路径
    std::string imagePath;
    // 这一帧的持续时间（毫秒）
    int delay = 0;
    // 帧偏移 X
    float offsetX = 0.0f;
    // 帧偏移 Y
    float offsetY = 0.0f;
    // 图片锚点 X
    float anchorX = 0.5f;
    // 图片锚点 Y
    float anchorY = 0.5f;
    // 缩放
    float scale = 1.0f;
    // 旋转
    float rotation = 0.0f;

public:
    MG_DEFINE_SERIALIZABLE(imagePath, delay, offsetX, offsetY, anchorX, anchorY, scale, rotation)
};

// 动画数据
class AniData : public Object
{
public:
    typedef Object Super;

public:
    AniData() {}
    virtual ~AniData() {}

    // 计算总时长（毫秒），无帧返回 0
    int totalDurationMs() const;

    // 根据时间计算帧索引（毫秒）。越界夹到首/尾帧；循环由播放入口决定
    int frameIndexAtTime(int timeMs) const;

public:
    // 帧列表
    std::vector<AniFrame> frames;
    // 源文件路径
    std::string sourcePath;

public:
    MG_DEFINE_SERIALIZABLE(frames, sourcePath)
};

NS_MG_END
