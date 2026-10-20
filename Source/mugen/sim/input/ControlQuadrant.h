#pragma once

#include <cstdint>

namespace mugen::sim
{
// 方向象限编码，与移动叶子的向量符号一致。
// 四个象限是 90 度扇区，每块占住一条坐标轴；纯方向靠 angle 与斜向区分
// （0 水平、45000 斜向、90000 垂直）。
//
//               上  ← 归 UpLeft(4)
//                |
//         4      |      1
//                |
// 左 ← 归 3 -----+----- 右  ← 归 UpRight(1)
//                |
//         3      |      2
//                |
//               下  ← 归 DownRight(2)
//
// fromDirectionKeys 实际会出现的组合（DownRight 不会配 angle=0）：
// 按键  | quadrant  | angle           | 走向
// 右    | UpRight   | 0（水平）       | 右
// 左    | DownLeft  | 0（水平）       | 左
// 上    | UpLeft    | 90000（垂直）   | 上
// 下    | DownRight | 90000（垂直）   | 下
// 右+上 | UpRight   | 45000（斜向）   | 右上 45 度
// 右+下 | DownRight | 45000（斜向）   | 右下 45 度
// 左+下 | DownLeft  | 45000（斜向）   | 左下 45 度
// 左+上 | UpLeft    | 45000（斜向）   | 左上 45 度
enum class ControlQuadrant : int32_t
{
    // 无方向。
    None = 0,
    // 右上。
    UpRight = 1,
    // 右下。
    DownRight = 2,
    // 左下。
    DownLeft = 3,
    // 左上。
    UpLeft = 4,
};

// 将上下左右箭头转为该方向所属象限：→ UpRight、↓ DownRight、← DownLeft、↑ UpLeft。
// 空字符或其他值表示无方向，供连招序列截断。
constexpr ControlQuadrant quadrantFromArrow(char32_t arrow)
{
    switch (arrow)
    {
    case U'→':
        return ControlQuadrant::UpRight;
    case U'↓':
        return ControlQuadrant::DownRight;
    case U'←':
        return ControlQuadrant::DownLeft;
    case U'↑':
        return ControlQuadrant::UpLeft;
    default:
        return ControlQuadrant::None;
    }
}

// 象限在水平轴上的移动符号：右为正，左为负。
constexpr int32_t vectorX(ControlQuadrant quadrant)
{
    if (quadrant == ControlQuadrant::None)
        return 0;
    return quadrant == ControlQuadrant::UpRight || quadrant == ControlQuadrant::DownRight ? 1 : -1;
}

// 象限在纵深轴上的移动符号：上为正，下为负。
constexpr int32_t vectorZ(ControlQuadrant quadrant)
{
    if (quadrant == ControlQuadrant::None)
        return 0;
    return quadrant == ControlQuadrant::UpRight || quadrant == ControlQuadrant::UpLeft ? 1 : -1;
}

// 一帧方向意图：象限仍是四向扇区，斜向额外带 45 度角。
class ControlAim
{
public:
    // 当前方向象限。
    ControlQuadrant quadrant = ControlQuadrant::None;
    // 斜向角度，单位为 1/1000 度；无方向为 0。
    int32_t angle = 0;

    // 按各轴后按下的方向键得出象限；垂直两轴同时按住时角度为 45 度。
    static ControlAim fromDirectionKeys(bool leftHeld,
                                        bool rightHeld,
                                        bool downHeld,
                                        bool upHeld,
                                        uint32_t leftOrder,
                                        uint32_t rightOrder,
                                        uint32_t downOrder,
                                        uint32_t upOrder);
};
}  // namespace mugen::sim
