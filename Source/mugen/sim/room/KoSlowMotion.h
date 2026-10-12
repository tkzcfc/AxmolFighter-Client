#pragma once
#include "mugen/core/reflect/Reflect.h"

namespace mugen::sim::room
{
// KO 慢放分段时钟；每次更新只推进一个阶段，不向下一段传递余量。
struct KoSlowMotion
{
    // 是否正在播放慢放。
    bool m_active = false;
    // 降速段累计秒数。
    Fixed m_fallSeconds;
    // 保持段累计秒数。
    Fixed m_holdSeconds;
    // 恢复段累计秒数。
    Fixed m_riseSeconds;
    // 当前缩放倍率。
    Fixed m_scale = "1"_fx;

    // 重新开始半秒降速、一秒保持、一秒恢复的曲线。
    void start() { *this = {}; m_active = true; }

    // 使用未缩放的逻辑秒数推进，避免慢放反过来延长自身。
    void update(Fixed dt)
    {
        if (!m_active)
            return;
        if (m_fallSeconds < "0.5"_fx)
        {
            m_fallSeconds += dt;
            m_scale = m_fallSeconds > "0.5"_fx ? "0.1"_fx :
                "0.1"_fx + ("1"_fx - m_fallSeconds / "0.5"_fx) * "0.9"_fx;
        }
        else if (m_holdSeconds < "1"_fx)
            m_holdSeconds += dt;
        else if (m_riseSeconds < "1"_fx)
        {
            m_riseSeconds += dt;
            m_scale = m_riseSeconds > "1"_fx ? "1"_fx : "0.1"_fx + m_riseSeconds * "0.9"_fx;
        }
        if (m_scale == "1"_fx)
            *this = {};
    }

    // 快照保存各段独立的累计时间。
    MG_REFLECT(KoSlowMotion, m_active, m_fallSeconds, m_holdSeconds, m_riseSeconds, m_scale)
};
}
