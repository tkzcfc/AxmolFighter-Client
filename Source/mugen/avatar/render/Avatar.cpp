#include "Avatar.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/data/AvatarAssetCache.h"

#    include <algorithm>

NS_MG_BEGIN

namespace
{

// 骨架层面的差异（需整体重建）
bool sameSkeleton(const AvatarDesc& a, const AvatarDesc& b)
{
    return a.skeleton == b.skeleton && a.scale == b.scale && a.motionFile == b.motionFile &&
           a.characterClass == b.characterClass;
}

}  // namespace

Avatar* Avatar::create(const AvatarDesc& desc, AvatarLoadMode mode)
{
    auto* ret = new (std::nothrow) Avatar();
    if (ret && ret->initWithDesc(desc, mode))
    {
        ret->autorelease();
        return ret;
    }
    AX_SAFE_DELETE(ret);
    return nullptr;
}

Avatar* Avatar::create(const FashionAppearance& appearance)
{
    AvatarDesc desc;
    if (!FashionResolver::resolve(appearance, desc))
    {
        MG_LOG_E("Avatar: resolve appearance failed role={}", appearance.roleId);
        return nullptr;
    }
    return create(desc, AvatarLoadMode::kAsyncExclusive);
}

Avatar::Avatar() : m_timeMs(0), m_loop(false), m_autoPlay(false) {}

Avatar::~Avatar() = default;

bool Avatar::initWithDesc(const AvatarDesc& desc, AvatarLoadMode mode)
{
    if (!ax::Node::init())
        return false;
    if (desc.skeleton.empty())
    {
        MG_LOG_E("Avatar: empty skeleton");
        return false;
    }

    m_mode = mode;
    if (!rebuild(desc))
        return false;

    // 始终挂上 update；是否推进由 m_autoPlay 控制（战斗 Avatar 保持 false，由 AvatarRenderSystem 驱动）
    scheduleUpdate();
    return true;
}

bool Avatar::rebuild(const AvatarDesc& desc)
{
    const MotionMap* motionMap = AvatarAssetCache::getInstance()->getMotionMap(desc.motionFile);
    if (!motionMap)
    {
        MG_LOG_E("Avatar: motion map not found '{}'", desc.motionFile);
        return false;
    }

    for (auto& accessory : m_accessories)
        accessory.reset();
    m_body.reset();
    m_revealed = false;

    // 保留当前动作与时间（换角色时按动作名重新对应）
    m_desc      = desc;
    m_motionMap = motionMap;
    m_motion    = m_motionName.empty() ? nullptr : m_motionMap->findMotion(m_motionName);
    normalizeTime();

    // 先建全部部件再开始加载：同步/命中缓存的回调会立即触发 tryReveal
    m_body = std::make_unique<SpineBody>(this, m_desc, m_mode);
    for (size_t i = 0; i < m_accessories.size(); ++i)
    {
        const auto slot = static_cast<AvatarAccessorySlot>(i);
        if (m_desc.accessory(slot) != 0)
            m_accessories[i] = std::make_unique<AvatarAccessory>(
                this, slot, m_desc.accessory(slot), AvatarAccessoryRules::rule(slot, m_desc.characterClass));
    }

    m_body->load([this]() { onBodyLoaded(); });
    for (auto& accessory : m_accessories)
    {
        if (accessory)
            accessory->load(m_mode, [this](AvatarAccessory* a) { onAccessorySettled(a); });
    }
    return true;
}

bool Avatar::setAppearance(const FashionAppearance& appearance)
{
    if (m_mode != AvatarLoadMode::kAsyncExclusive)
    {
        MG_LOG_W("Avatar::setAppearance: requires kAsyncExclusive avatar");
        return false;
    }

    AvatarDesc desc;
    if (!FashionResolver::resolve(appearance, desc))
    {
        MG_LOG_E("Avatar::setAppearance: resolve failed role={}", appearance.roleId);
        return false;
    }

    if (!sameSkeleton(desc, m_desc))
        return rebuild(desc);

    m_body->setLook(desc.atlases, desc.skin);
    m_desc = std::move(desc);
    syncAccessories();
    return true;
}

void Avatar::syncAccessories()
{
    bool changed = false;
    for (size_t i = 0; i < m_accessories.size(); ++i)
    {
        const auto slot  = static_cast<AvatarAccessorySlot>(i);
        const int32_t id = m_desc.accessory(slot);
        auto& current    = m_accessories[i];
        if (current ? current->resSpineId() == id : id == 0)
            continue;
        current.reset();
        changed = true;
        if (id != 0)
            createAccessory(slot);
    }
    if (changed)
        syncGrafts();
}

void Avatar::createAccessory(AvatarAccessorySlot slot)
{
    auto& accessory = m_accessories[static_cast<size_t>(slot)];
    accessory       = std::make_unique<AvatarAccessory>(this, slot, m_desc.accessory(slot),
                                                        AvatarAccessoryRules::rule(slot, m_desc.characterClass));
    accessory->load(m_mode, [this](AvatarAccessory* a) { onAccessorySettled(a); });
}

void Avatar::onBodyLoaded()
{
    if (!m_body->isReady())
        return;
#    if _DEBUG
    if (m_motion)
        m_body->validateMotion(*m_motion);
#    endif
    syncGrafts();
    applyTime();
    tryReveal();
}

void Avatar::onAccessorySettled(AvatarAccessory* accessory)
{
    if (accessory->rule().graftSlots)
        syncGrafts();
    tryReveal();
    refreshVisibility();
}

void Avatar::syncGrafts()
{
    if (!m_body || !m_body->isExclusive())
        return;
    for (size_t i = 0; i < m_accessories.size(); ++i)
    {
        const auto slot = static_cast<AvatarAccessorySlot>(i);
        const auto rule = AvatarAccessoryRules::rule(slot, m_desc.characterClass);
        if (!rule.graftSlots || rule.graftSlots->empty())
            continue;
        const auto& accessory = m_accessories[i];
        MgSkeletonDataPtr donor =
            accessory && accessory->node() ? accessory->node()->skeletonDataPtr() : MgSkeletonDataPtr();
        m_body->setGraft(slot, std::move(donor), rule.graftSlots);
    }
}

void Avatar::tryReveal()
{
    if (m_revealed || !m_body || !m_body->isReady())
        return;
    for (const auto& accessory : m_accessories)
    {
        if (accessory && !accessory->isSettled())
            return;
    }
    m_revealed = true;
    refreshVisibility();
}

void Avatar::refreshVisibility()
{
    if (!m_body)
        return;
    m_body->setVisible(m_revealed);
    const bool standing = m_motionName == AvatarAccessoryRules::kStandAnim;
    for (auto& accessory : m_accessories)
    {
        if (accessory)
            accessory->refresh(m_revealed, standing);
    }
}

void Avatar::setMotion(const std::string& motionName, const std::string& entryId, bool loop)
{
    m_motionName = motionName;
    m_entryId    = entryId;
    m_loop       = loop;
    m_timeMs     = 0;
    m_motion     = m_motionMap->findMotion(motionName);
    if (!m_motion)
    {
        MG_LOG_W("Avatar: motion '{}' not found in '{}'", motionName, m_desc.motionFile);
    }
    else
    {
        const int startMs = m_motion->startTimeMs(entryId);
        if (startMs < 0)
            MG_LOG_W("Avatar: entry '{}' not found in motion '{}'", entryId, motionName);
        m_timeMs = std::max(0, startMs);
#    if _DEBUG
        if (m_body->isReady())
            m_body->validateMotion(*m_motion);
#    endif
    }

    m_body->resetClip();
    applyTime();
    refreshVisibility();
}

void Avatar::normalizeTime()
{
    if (!m_motion)
    {
        m_timeMs = 0;
        return;
    }
    const int dur = m_motion->durationMs();
    m_timeMs      = std::max(0, m_timeMs);
    m_timeMs      = m_loop ? m_timeMs % dur : std::min(m_timeMs, dur);
}

void Avatar::applyTime()
{
    if (!m_motion || !m_body || !m_body->isReady())
        return;
    int localMs            = 0;
    size_t index           = 0;
    const MotionClip* clip = m_motion->clipAt(m_timeMs, &localMs, &index);
    if (clip)
        m_body->applyClip(index, *clip, localMs);
}

void Avatar::step(int dtMs)
{
    if (dtMs <= 0)
        return;

    if (m_motion)
    {
        m_timeMs += dtMs;
        normalizeTime();
        applyTime();
    }

    const float dtSec = static_cast<float>(dtMs) / 1000.0f;
    for (auto& accessory : m_accessories)
    {
        if (accessory)
            accessory->step(dtSec);
    }
}

void Avatar::seek(int timeMs)
{
    m_timeMs = timeMs;
    normalizeTime();
    applyTime();
}

void Avatar::update(float delta)
{
    // UI 展示：等全部部件就绪再推进，避免就绪后动画突然跳跃
    if (!m_autoPlay || !m_revealed)
        return;
    m_autoPlayCarry += delta * 1000.0f;
    const int dtMs = static_cast<int>(m_autoPlayCarry);
    m_autoPlayCarry -= static_cast<float>(dtMs);
    step(dtMs);
}

ax::Rect Avatar::localSkeletonBounds() const
{
    return m_body ? m_body->bounds() : ax::Rect::ZERO;
}

bool Avatar::isFinished() const
{
    if (m_loop)
        return false;
    return !m_motion || m_timeMs >= m_motion->durationMs();
}

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
