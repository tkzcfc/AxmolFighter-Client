#include "Avatar.h"

#ifdef RUNTIME_IN_AXMOL

#    include "SpineLayer.h"

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
void Avatar::addLayer(RenderLayer* layer, int order, AvatarLayerTag tag)
{
    if (!layer)
        return;

    layer->setLayerTag(tag);
    addChild(layer, order);
    m_layers.push_back(layer);

    if (!m_motionName.empty())
    {
        layer->setMotion(m_motionName, m_entryId);
        layer->seek(getCurrentTimeMs());
    }
}

// 按 tag 移除层
void Avatar::removeLayersByTag(AvatarLayerTag tag)
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
bool Avatar::hasLayersWithTag(AvatarLayerTag tag) const
{
    return findLayer(tag) != nullptr;
}

RenderLayer* Avatar::findLayer(AvatarLayerTag tag) const
{
    for (RenderLayer* layer : m_layers)
    {
        if (layer && layer->getLayerTag() == tag)
            return layer;
    }
    return nullptr;
}

void Avatar::initFashionAppearance(const FashionAppearance& appearance)
{
    m_appearance    = appearance;
    m_fashionDriven = true;
}

bool Avatar::setFashion(FashionPosition position, int32_t id)
{
    if (!m_fashionDriven)
    {
        MG_LOG_W("Avatar::setFashion: not a fashion-driven avatar");
        return false;
    }

    if (id != 0)
        m_appearance.equipFashion[position] = id;
    else
        m_appearance.equipFashion.erase(position);

    const auto desc = FashionResolver::resolve(m_appearance);
    if (!desc.valid)
        return false;

    auto* body = dynamic_cast<SpineLayer*>(findLayer(AvatarLayerTag::kBody));
    if (!body)
        return false;
    body->applyFashionDesc(desc);
    return true;
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
            layer->setMotion(motionName, entryId);
    }

    for (const RenderLayer* layer : m_layers)
    {
        if (layer)
            m_timeMs = std::max(m_timeMs, layer->currentTimeMs());
    }

#    if _DEBUG
    // 各层时长不一致时告警
    int firstNonZero = -1;
    bool mismatch    = false;
    for (const RenderLayer* layer : m_layers)
    {
        if (!layer)
            continue;
        const int d = layer->durationMs();
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
            detail += "tag=" + std::to_string(static_cast<int32_t>(layer->getLayerTag())) +
                      " dur=" + std::to_string(layer->durationMs());
        }
        MG_LOG_W("Avatar: layer durations mismatch motion='{}' max={} [{}] (using max; short layers freeze last frame)",
                 m_motionName, durationMs(), detail);
    }
#    endif
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

    const int dur = durationMs();
    m_timeMs += dtMs;
    const bool wrapped = m_loop && dur > 0 && m_timeMs >= dur;
    normalizeTime();

    for (RenderLayer* layer : m_layers)
    {
        if (!layer)
            continue;
        if (wrapped)
            layer->seek(m_timeMs);
        else
            layer->step(dtMs);
    }

    // 层在异步装配就绪后会跳转到记录的时间；这里对齐到各层最大当前时间（吸收跳转）
    for (RenderLayer* layer : m_layers)
    {
        if (layer)
            m_timeMs = std::max(m_timeMs, layer->currentTimeMs());
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
    // UI 展示：等所有层就绪再推进，避免就绪后动画突然跳跃
    for (RenderLayer* layer : m_layers)
    {
        if (layer && !layer->isReady())
            return;
    }
    const int dtMs = static_cast<int>(delta * 1000.0f);
    if (dtMs > 0)
        step(dtMs);
}

ax::Rect Avatar::localSkeletonBounds() const
{
    ax::Rect merged;
    bool any = false;
    for (RenderLayer* layer : m_layers)
    {
        auto* spine = dynamic_cast<SpineLayer*>(layer);
        if (!spine)
            continue;
        const ax::Rect box = spine->skeletonBoundingBox();
        if (box.size.width < 1.0f && box.size.height < 1.0f)
            continue;
        if (!any)
        {
            merged = box;
            any    = true;
        }
        else
            merged.merge(box);
    }
    return merged;
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
