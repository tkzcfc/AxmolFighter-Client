#include "ProgressSmoother.h"

#include <algorithm>
#include <cmath>

namespace gameui
{

void ProgressSmoother::update(float dt, float target, bool finished)
{
    m_finished = m_finished || finished;
    target     = std::clamp(target, 0.0f, 1.0f);

    if (m_finished)
    {
        m_display = std::min(1.0f, m_display + dt * kFinishSpeed);
        return;
    }

    if (target > m_display)
    {
        // 追赶：指数缓动，并保证一个最低速度
        const float remain = target - m_display;
        const float step   = std::max(remain * (1.0f - std::exp(-kCatchUpRate * dt)), kMinCatchUpSpeed * dt);
        m_display          = std::min(target, m_display + step);
    }
    else
    {
        // 爬行：缓慢逼近一个比真实进度略高、但不超过 kCreepCeiling 的上限
        const float ceiling = std::min(kCreepCeiling, target + (kCreepCeiling - target) * kCreepShare);
        if (m_display < ceiling)
        {
            const float remain = ceiling - m_display;
            const float step   = remain * (1.0f - std::exp(-kCreepRate * dt));
            m_display          = std::min(ceiling, m_display + step);
        }
    }
}

}  // namespace gameui
