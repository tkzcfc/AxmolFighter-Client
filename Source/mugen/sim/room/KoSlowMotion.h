#pragma once
#include "mugen/core/math/Fixed.h"
#include "mugen/core/reflect/Reflect.h"

namespace mugen::sim::room
{
// KO 慢放分段时钟；每次更新只推进一个阶段，不向下一段传递余量。
struct KoSlowMotion
{
    // 是否正在播放慢放。
    bool active = false;
    // 降速段累计秒数。
    Fixed fallSeconds;
    // 保持段累计秒数。
    Fixed holdSeconds;
    // 恢复段累计秒数。
    Fixed riseSeconds;
    // 当前缩放倍率。
    Fixed scale = "1"_fx;

    // 重新开始半秒降速、一秒保持、一秒恢复的曲线。
    void start()
    {
        *this    = {};
        active = true;
    }

    // 使用未缩放的逻辑秒数推进，避免慢放反过来延长自身。
    void update(Fixed dt)
    {
        if (!active)
            return;
        if (fallSeconds < "0.5"_fx)
        {
            fallSeconds += dt;
            scale = fallSeconds > "0.5"_fx ? "0.1"_fx : "0.1"_fx + ("1"_fx - fallSeconds / "0.5"_fx) * "0.9"_fx;
        }
        else if (holdSeconds < "1"_fx)
            holdSeconds += dt;
        else if (riseSeconds < "1"_fx)
        {
            riseSeconds += dt;
            scale = riseSeconds > "1"_fx ? "1"_fx : "0.1"_fx + riseSeconds * "0.9"_fx;
        }
        if (scale == "1"_fx)
            *this = {};
    }

    // 快照保存各段独立的累计时间。
    MG_REFLECT(KoSlowMotion, active, fallSeconds, holdSeconds, riseSeconds, scale)
};
}  // namespace mugen::sim::room
