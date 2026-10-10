#pragma once

#include "mugen/core/math/Fixed.h"
#include "mugen/core/reflect/Reflect.h"

#include <string>
#include <vector>

NS_MG_BEGIN

// 帧动画中的单个图像帧。
struct AniFrame
{
    // 图像资源路径。
    std::string imagePath;
    // 该帧持续毫秒数。
    int32_t delay = 0;
    // 图像横向偏移。
    Fixed offsetX;
    // 图像纵向偏移。
    Fixed offsetY;
    // 横向锚点比例。
    Fixed anchorX = Fixed::ratio(1, 2);
    // 纵向锚点比例。
    Fixed anchorY = Fixed::ratio(1, 2);
    // 图像缩放比例。
    Fixed scale = Fixed::fromInt(1);
    // 图像旋转角度。
    Fixed rotation;

    MG_REFLECT(AniFrame, imagePath, delay, offsetX, offsetY, anchorX, anchorY, scale, rotation)
};

// 帧动画资源及其时间查询方法。
class AniData
{
public:
    // 计算全部帧的总时长。
    int totalDurationMs() const;
    // 返回指定时间所在的帧索引。
    int frameIndexAtTime(int timeMs) const;

    // 按播放顺序排列的图像帧。
    std::vector<AniFrame> frames;
    // 源资源路径。
    std::string sourcePath;

    MG_REFLECT(AniData, frames, sourcePath)
};

NS_MG_END
