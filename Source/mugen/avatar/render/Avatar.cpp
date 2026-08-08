#include "Avatar.h"

#ifdef RUNTIME_IN_AXMOL

#    include <algorithm>
#    include <string>

NS_MG_BEGIN

// 创建空 Avatar 容器
Avatar* Avatar::create()
{
    auto* ret = new (std::nothrow) Avatar();
    if (ret && ret->init())
    {
        ret->autorelease();
        return ret;
    }
    AX_SAFE_DELETE(ret);
    return nullptr;
}

bool Avatar::init()
{
    m_timeMs   = 0;
    m_loop     = false;
    m_autoPlay = false;
    if (!ax::Node::init())
        return false;
    // 始终挂上 update；是否推进由 m_autoPlay 控制（战斗 Avatar 保持 false，由 AvatarRenderSystem 驱动）
    scheduleUpdate();
    return true;
}

// 添加层；播放中立即同步动作并对齐时间
void Avatar::addLayer(RenderLayer* layer, int order, int32_t tag)
{
    if (!layer)
        return;

    layer->setLayerTag(tag);
    addChild(layer, order);
    m_layers.push_back(layer);

    if (!m_motionName.empty())
    {
        layer->setMotion(m_motionName, m_entryId, m_loop);
        layer->seek(getCurrentTimeMs());
    }
}

// 按 tag 移除层
void Avatar::removeLayersByTag(int32_t tag)
{
    for (auto it = m_layers.begin(); it != m_layers.end();)
    {
        RenderLayer* layer = *it;
        if (layer && layer->getLayerTag() == tag)
        {
            layer->removeFromParent();
            it = m_layers.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// 是否存在指定 tag 的层
bool Avatar::hasLayersWithTag(int32_t tag) const
{
    for (const RenderLayer* layer : m_layers)
    {
        if (layer && layer->getLayerTag() == tag)
            return true;
    }
    return false;
}

// 广播切换动作
void Avatar::setMotion(const std::string& motionName, const std::string& entryId, bool loop)
{
    m_motionName = motionName;
    m_entryId    = entryId;
    m_loop       = loop;
    m_timeMs     = 0;

    for (RenderLayer* layer : m_layers)
    {
        if (layer)
            layer->setMotion(motionName, entryId, loop);
    }

    // 各层时长不一致时告警（忽略 duration=0 空层）；全局仍取 max，短层冻末帧
    int firstNonZero = -1;
    bool mismatch    = false;
    for (const RenderLayer* layer : m_layers)
    {
        if (!layer)
            continue;
        const int d = layer->durationMs();
        if (d <= 0)
            continue;
        if (firstNonZero < 0)
            firstNonZero = d;
        else if (d != firstNonZero)
            mismatch = true;
    }
    if (mismatch)
    {
        std::string detail;
        for (const RenderLayer* layer : m_layers)
        {
            if (!layer)
                continue;
            if (!detail.empty())
                detail += ", ";
            detail += "tag=" + std::to_string(layer->getLayerTag()) + " dur=" + std::to_string(layer->durationMs());
        }
        MG_LOG_W("Avatar: layer durations mismatch motion='{}' max={} [{}] (using max; short layers freeze last frame)",
                 m_motionName, durationMs(), detail);
    }
}

// 按循环规则规范化累计时间
void Avatar::normalizeTime()
{
    const int dur = durationMs();
    if (dur <= 0)
    {
        m_timeMs = 0;
        return;
    }
    if (m_loop)
    {
        m_timeMs %= dur;
        if (m_timeMs < 0)
            m_timeMs += dur;
    }
    else if (m_timeMs > dur)
    {
        m_timeMs = dur;
    }
}

void Avatar::step(int dtMs)
{
    if (dtMs <= 0)
        return;

    m_timeMs += dtMs;
    normalizeTime();

    // 绝对时间驱动各层（与 MotionPlayer 全局时钟一致；短层 seek 后冻末帧）
    for (RenderLayer* layer : m_layers)
    {
        if (layer)
            layer->seek(m_timeMs);
    }
}

void Avatar::seek(int timeMs)
{
    if (timeMs < 0)
        timeMs = 0;
    m_timeMs = timeMs;
    normalizeTime();

    for (RenderLayer* layer : m_layers)
    {
        if (layer)
            layer->seek(m_timeMs);
    }
}

// 自动播放时按 delta 步进
void Avatar::update(float delta)
{
    if (!m_autoPlay)
        return;
    const int dtMs = static_cast<int>(delta * 1000.0f);
    if (dtMs > 0)
        step(dtMs);
}

// 返回各层时长最大值
int Avatar::durationMs() const
{
    int maxDur = 0;
    for (const RenderLayer* layer : m_layers)
    {
        if (layer)
            maxDur = std::max(maxDur, layer->durationMs());
    }
    return maxDur;
}

// 非循环且已播完
bool Avatar::isFinished() const
{
    if (m_loop)
        return false;
    const int dur = durationMs();
    if (dur <= 0)
        return true;
    return m_timeMs >= dur;
}

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
