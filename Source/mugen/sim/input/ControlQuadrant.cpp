#include "ControlQuadrant.h"

namespace mugen::sim
{
namespace
{
// 同一轴两侧的按住状态与按下序号。
struct AxisKeys
{
    // 负向键是否按住。
    bool negativeHeld = false;
    // 正向键是否按住。
    bool positiveHeld = false;
    // 负向键按下的序号。
    uint32_t negativeOrder = 0;
    // 正向键按下的序号。
    uint32_t positiveOrder = 0;
};

// 同轴对向键同时按住时，后按下的一侧决定符号。
int32_t axisSign(AxisKeys axis)
{
    if (axis.negativeHeld && axis.positiveHeld)
        return axis.positiveOrder >= axis.negativeOrder ? 1 : -1;
    if (axis.positiveHeld)
        return 1;
    if (axis.negativeHeld)
        return -1;
    return 0;
}

// 斜向象限与移动叶子的向量符号一致。
ControlQuadrant diagonalQuadrant(int32_t xSign, int32_t zSign)
{
    if (xSign > 0)
        return zSign > 0 ? ControlQuadrant::UpRight : ControlQuadrant::DownRight;
    return zSign > 0 ? ControlQuadrant::UpLeft : ControlQuadrant::DownLeft;
}
}  // namespace

// 按各轴后按下的方向键得出象限；垂直两轴同时按住时角度为 45 度。
ControlAim ControlAim::fromDirectionKeys(bool leftHeld,
                                         bool rightHeld,
                                         bool downHeld,
                                         bool upHeld,
                                         uint32_t leftOrder,
                                         uint32_t rightOrder,
                                         uint32_t downOrder,
                                         uint32_t upOrder)
{
    const int32_t xSign = axisSign({leftHeld, rightHeld, leftOrder, rightOrder});
    const int32_t zSign = axisSign({downHeld, upHeld, downOrder, upOrder});
    if (xSign == 0 && zSign == 0)
        return {};
    if (xSign != 0 && zSign != 0)
        return {diagonalQuadrant(xSign, zSign), 45000};
    if (xSign != 0)
        return {xSign > 0 ? ControlQuadrant::UpRight : ControlQuadrant::DownLeft, 0};
    return {zSign > 0 ? ControlQuadrant::UpLeft : ControlQuadrant::DownRight, 90000};
}
}  // namespace mugen::sim
