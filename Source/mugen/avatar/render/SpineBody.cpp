#include "SpineBody.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/AvatarAccessoryRules.h"
#    include "mugen/render/spine/MgSpineLoadPipeline.h"
#    include "mugen/render/spine/MgSpineUtils.h"
#    include "mugen/render/spine/SpineSkeletonCache.h"

#    include <algorithm>
#    include <cmath>

NS_MG_BEGIN

SpineBody::SpineBody(ax::Node* parent, const AvatarDesc& desc, AvatarLoadMode mode)
    : m_parent(parent)
    , m_mode(mode)
    , m_skeleton(desc.skeleton)
    , m_scale(desc.scale > 0.0f ? desc.scale : 1.0f)
    , m_token(std::make_shared<char>(0))
{
    m_requested.atlases = resolveAtlasFiles(desc.skeleton, desc.atlases);
    m_requested.skin    = desc.skin;
}

SpineBody::~SpineBody()
{
    m_token.reset();
    if (m_node)
        m_node->removeFromParent();
}

void SpineBody::load(LoadedCallback callback)
{
    m_onLoaded = std::move(callback);

    if (m_mode == AvatarLoadMode::kSyncShared)
    {
        auto data = SpineSkeletonCache::getInstance()->getOrCreate(m_skeleton, m_requested.atlases, m_scale);
        onLoaded(MgSkeletonAnimation::createWithData(std::move(data)));
        return;
    }

    std::weak_ptr<char> token = m_token;
    MgSkeletonAnimation::createExclusiveAsync(m_skeleton, m_requested.atlases, m_scale,
                                              [this, token](MgSkeletonAnimation* node) {
        if (!token.expired())
            onLoaded(node);
    });
}

void SpineBody::onLoaded(MgSkeletonAnimation* node)
{
    if (!node)
    {
        MG_LOG_E("SpineBody: failed to load skeleton '{}'", m_skeleton);
        m_onLoaded();
        return;
    }

    m_node = node;
    m_node->setAutoUpdate(false);
    m_node->setUpdateOnlyIfVisible(false);
    m_node->setTimeScale(1.0f);
    m_node->setVisible(false);
    m_parent->addChild(m_node, AvatarAccessoryRules::kBodyZOrder);

    // 骨架按加载时的 atlas 建立；皮肤尚未设置
    m_applied.atlases = m_requested.atlases;
    m_applied.skin.clear();
    syncLook();
    syncGrafts();

    m_onLoaded();
}

void SpineBody::setLook(const std::vector<std::string>& atlases, const std::string& skin)
{
    // 判断一下换装是否可行，避免在非独占模式下误操作
    MG_ASSERT(isExclusive() && "SpineBody::setLook requires exclusive skeleton data");
    if (!isExclusive())
    {
        MG_LOG_W("SpineBody: setLook requires exclusive skeleton data");
        return;
    }

    auto resolved = resolveAtlasFiles(m_skeleton, atlases);
    if (resolved == m_requested.atlases && skin == m_requested.skin)
        return;
    m_requested.atlases = std::move(resolved);
    m_requested.skin    = skin;
    syncLook();
}

void SpineBody::syncLook()
{
    if (!m_node)
        return;

    // 任何新请求都使在途的 atlas 加载失效
    const uint32_t seq = ++m_lookSeq;
    if (m_requested.atlases == m_applied.atlases)
    {
        applySkin(m_requested.skin);
        return;
    }

    std::weak_ptr<char> token = m_token;
    const Look look           = m_requested;
    MgSpineLoadPipeline::loadAtlas(m_node->skeletonData()->runtime(), look.atlases,
                                   [this, token, seq, look](MgAtlasHandlePtr atlas) {
        if (token.expired() || seq != m_lookSeq)
            return;
        if (!atlas || !m_node->replaceAtlas(std::move(atlas)))
        {
            MG_LOG_W("SpineBody: atlas swap failed '{}'", m_skeleton);
            return;
        }
        m_applied.atlases = look.atlases;
        applySkin(look.skin);
    });
}

void SpineBody::applySkin(const std::string& skin)
{
    if (!skin.empty() && skin != m_applied.skin)
    {
        if (m_node->skeletonData()->hasSkin(skin.c_str()))
            m_node->setSkin(skin);
        else
            MG_LOG_W("SpineBody: skin not found '{}' in '{}'", skin, m_skeleton);
    }
    m_applied.skin = skin;
    applyPose();
}

void SpineBody::setGraft(AvatarAccessorySlot slot, MgSkeletonDataPtr donor, const std::vector<std::string>* slotNames)
{
    MG_ASSERT(isExclusive() && "SpineBody::setGraft requires exclusive skeleton data");
    auto& graft     = m_grafts[static_cast<size_t>(slot)];
    graft.desired   = std::move(donor);
    graft.slotNames = slotNames;
    syncGrafts();
}

void SpineBody::syncGrafts()
{
    if (!m_node || !isExclusive())
        return;

    bool changed = false;
    for (auto& graft : m_grafts)
    {
        if (graft.desired == graft.applied)
            continue;
        changed = true;
        if (graft.applied)
            m_node->clearSkinSlots(*graft.slotNames);
        if (graft.desired)
            m_node->graftSkinSlots(graft.desired, AvatarAccessoryRules::kDonorSkin, *graft.slotNames);
        graft.applied = graft.desired;
    }
    if (changed)
        applyPose();
}

void SpineBody::resetClip()
{
    m_clipIndex = static_cast<size_t>(-1);
    m_hasTrack  = false;
    m_trackSec  = 0.0f;
}

void SpineBody::applyClip(size_t clipIndex, const MotionClip& clip, int localMs)
{
    if (clipIndex != m_clipIndex)
    {
        m_clipIndex = clipIndex;
        m_hasTrack  = static_cast<bool>(m_node->setAnimation(0, clip.source, false));
        if (!m_hasTrack)
        {
            MG_LOG_W("SpineBody: setAnimation failed '{}' in '{}'", clip.source, m_skeleton);
            return;
        }
        // 轨道不自行结束，时间完全由 Avatar 以绝对值驱动
        m_node->keepCurrentTrackAlive(0);
    }
    if (!m_hasTrack)
        return;

    const int clamped = clip.durationMs > 0 ? std::clamp(localMs, 0, clip.durationMs) : std::max(0, localMs);
    m_trackSec        = static_cast<float>(clamped) / 1000.0f;
    applyPose();
}

void SpineBody::applyPose()
{
    if (!m_hasTrack)
        return;
    m_node->seekCurrentTrack(0, m_trackSec);
    m_node->update(0.0f);
}

void SpineBody::setVisible(bool visible)
{
    if (m_node)
        m_node->setVisible(visible);
}

ax::Rect SpineBody::bounds() const
{
    return m_node ? m_node->getBoundingBox() : ax::Rect::ZERO;
}

#    if _DEBUG
void SpineBody::validateMotion(const Motion& motion) const
{
    for (const MotionClip& clip : motion.clips)
    {
        if (clip.type != MotionEntryType::kSpine)
        {
            MG_LOG_W("SpineBody: motion '{}' clip '{}' is not a spine clip", motion.name, clip.id);
            continue;
        }
        const MgAnimation anim = m_node->findAnimation(clip.source);
        if (!anim)
        {
            MG_LOG_W("SpineBody: animation '{}' not found in '{}'", clip.source, m_skeleton);
            continue;
        }
        MG_ASSERT(clip.durationMs == static_cast<int>(std::lround(anim.duration() * 1000.0f)));
    }
}
#    endif

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
