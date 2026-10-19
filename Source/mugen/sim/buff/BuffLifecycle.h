#pragma once

#include "mugen/conf/tables/BuffTables.h"

namespace mugen::sim::buff
{
// 生命周期向规则分派器提交的单次调用，不持有规则对象。
enum class LifecycleAction : uint8_t
{
    None,
    Enter,
    Update,
    RemoveRepeat
};

// 单个实例的秒制周期状态，可随实体组件一起序列化。
struct Lifecycle
{
    // 当前层数及已执行次数。
    int32_t m_repeat = 1, m_times = 0;
    // 当前周期累计秒数及配置周期秒数。
    Fixed m_interval, m_maxInterval;
    // 配置次数，负一表示无限更新。
    int32_t m_maxTimes = 0;
    // 内部冷却累计秒数及配置秒数。
    Fixed m_innerElapsed, m_innerCd;
    // 激活、退出与销毁标记相互独立。
    bool m_active = false, m_exit = false, m_destroy = false;
    MG_REFLECT(Lifecycle,
               m_repeat,
               m_times,
               m_interval,
               m_maxInterval,
               m_maxTimes,
               m_innerElapsed,
               m_innerCd,
               m_active,
               m_exit,
               m_destroy)

    // 从配置初始化计时状态；内部冷却初始为已满足。
    static Lifecycle fromConfig(const conf::BuffConfig& config);
    // 执行模式一立即计次，模式二仅激活，模式三等待事件。
    LifecycleAction execute(int32_t executeType);
    // 周期检查先于累加，达到阈值的下一帧才执行；每帧最多一次。
    LifecycleAction advance(Fixed dtSeconds);
    // ENTER 回调完成后判断有限次数终点，保留先执行后移层的顺序。
    bool finishEnter();
    // 重置计时部分；叠层及重新执行由调用者先行处理。
    void resetCounters(int32_t resetType);
};

inline Lifecycle Lifecycle::fromConfig(const conf::BuffConfig& config)
{
    Lifecycle state;
    state.m_maxInterval  = config.interval;
    state.m_maxTimes     = config.times;
    state.m_innerCd      = config.innerCd;
    state.m_innerElapsed = config.innerCd;
    return state;
}

inline LifecycleAction Lifecycle::execute(int32_t executeType)
{
    if (executeType == 1)
    {
        m_active = true;
        if (m_maxTimes > 0)
            ++m_times;
        return LifecycleAction::Enter;
    }
    if (executeType == 2)
        m_active = true;
    else if (executeType == 3)
        m_active = false;
    return LifecycleAction::None;
}

inline LifecycleAction Lifecycle::advance(Fixed dtSeconds)
{
    if (m_repeat == 0 || !m_active || m_exit || m_maxTimes < -1 || m_maxInterval < Fixed{} ||
        (m_maxTimes == 0 && m_maxInterval == Fixed{}))
        return LifecycleAction::None;

    if (m_maxInterval > Fixed{})
    {
        if (m_interval < m_maxInterval)
        {
            m_interval += dtSeconds;
            return LifecycleAction::None;
        }
        m_interval -= m_maxInterval;
    }

    if (m_maxTimes == -1)
    {
        if (m_innerCd > Fixed{})
        {
            m_innerElapsed += dtSeconds;
            if (m_innerElapsed < m_innerCd)
                return LifecycleAction::None;
            m_innerElapsed = Fixed{};
        }
        return LifecycleAction::Update;
    }
    if (m_maxTimes == 0)
        return LifecycleAction::RemoveRepeat;
    ++m_times;
    return LifecycleAction::Enter;
}

inline bool Lifecycle::finishEnter()
{
    if (m_maxTimes > 0 && m_times >= m_maxTimes)
    {
        m_times = 0;
        return true;
    }
    return false;
}

inline void Lifecycle::resetCounters(int32_t resetType)
{
    if (resetType == 1 || resetType == 2 || resetType == 6 || resetType == 7)
        m_times = 0;
    if (resetType == 1 || resetType == 3 || resetType == 5 || resetType == 7)
        m_interval = Fixed{};
}
}  // namespace mugen::sim::buff
