#include "SpineLayer.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/data/AvatarAssetCache.h"
#    include "mugen/render/spine/MgSpineLoadPipeline.h"
#    include "mugen/render/spine/MgSpineUtils.h"
#    include "mugen/render/spine/SpineSkeletonCache.h"

#    include <algorithm>
#    include <cmath>

NS_MG_BEGIN

namespace
{
// 附件（武器/翅膀/光环）在层内的 z-order：光环/翅膀在身体骨架后，武器在前
constexpr int kHaloZOrder   = -20;
constexpr int kWingZOrder   = -10;
constexpr int kWeaponZOrder = 10;
// 附件循环播放的待机动画
constexpr const char* kAttachmentAnim = "stand";
}  // namespace

SpineLayer* SpineLayer::create(const FashionSpineDesc& desc, bool asyncLoad)
{
    auto* ret = new (std::nothrow) SpineLayer();
    if (ret && ret->initWithDesc(desc, asyncLoad))
    {
        ret->autorelease();
        return ret;
    }
    AX_SAFE_DELETE(ret);
    return nullptr;
}

SpineLayer::~SpineLayer()
{
    if (m_asyncAlive)
        m_asyncAlive->store(false);
}

bool SpineLayer::initWithDesc(const FashionSpineDesc& desc, bool asyncLoad)
{
    if (!ax::Node::init())
    {
        MG_LOG_E("SpineLayer::init: invalid args");
        return false;
    }

    m_asyncAlive = std::make_shared<std::atomic<bool>>(true);

    m_motionMap = AvatarAssetCache::getInstance()->getMotionMap(desc.motionFile);
    if (!m_motionMap)
    {
        MG_LOG_E("SpineLayer::init: failed to load motion map '{}'", desc.motionFile);
        return false;
    }

    m_requestedAtlases = desc.atlases;
    m_requestedSkin    = desc.skin;

    if (!initSkeleton(desc, asyncLoad))
        return false;

    // 武器/翅膀/光环附件（独立 spine 子节点，与身体骨架异步加载互不阻塞）
    syncAttachments(desc);

    if (m_skeleton && !desc.skin.empty())
        setSkin(desc.skin);

    return true;
}

bool SpineLayer::initSkeleton(const FashionSpineDesc& desc, bool asyncLoad)
{
    const float scale = desc.scale > 0.0f ? desc.scale : 1.0f;
    if (asyncLoad)
    {
        // 异步装配：实例私有数据（可换装）；先返回空层，骨架就绪后自动应用皮肤/暂存换装/暂存动作
        m_skeletonLoading = true;
        auto alive        = m_asyncAlive;
        MgSpineLoadPipeline::start(desc.skeleton, desc.atlases, scale, [this, alive](MgSkeletonData* data) {
            if (!alive->load())
            {
                delete data;
                return;
            }
            onSkeletonReady(data);
        });
        return true;
    }
    // 同步：共享缓存（返回即就绪，不可换装）
    return setupSkeleton(SpineSkeletonCache::getInstance()->getOrCreate(desc.skeleton, desc.atlases, scale), false);
}

bool SpineLayer::setupSkeleton(MgSkeletonData* data, bool owned)
{
    if (!data)
        return false;
    m_skeleton = owned ? MgSkeletonAnimation::createWithOwnedData(data) : MgSkeletonAnimation::createWithData(data);
    if (!m_skeleton)
    {
        MG_LOG_E("SpineLayer: failed to create skeleton node");
        return false;
    }

    m_skeleton->setAutoUpdate(false);
    m_skeleton->setUpdateOnlyIfVisible(false);
    m_skeleton->setTimeScale(1.0f);
    addChild(m_skeleton);
    return true;
}

void SpineLayer::onSkeletonReady(MgSkeletonData* data)
{
    m_skeletonLoading = false;
    if (!setupSkeleton(data, true))
    {
        MG_LOG_E("SpineLayer: async skeleton setup failed");
        return;
    }
    // 身体骨架异步装配期间附件先隐藏，就绪后再显示
    setAttachmentsVisible(true);

    if (!m_requestedSkin.empty())
        setSkin(m_requestedSkin);

    // 装配期间请求的换装（最新一条）
    if (m_swapDirty)
    {
        m_swapDirty = false;
        applyRequestedAtlases();
    }

    // 装配期间暂存的动作
    if (m_hasPendingMotion)
    {
        const std::string motionName = std::move(m_pendingMotionName);
        const std::string entryId    = std::move(m_pendingMotionEntry);
        const int elapsed            = m_timeMs;  // 装配期间累计的流逝时间
        const int seekTarget         = m_pendingSeekMs;
        m_hasPendingMotion           = false;
        m_pendingSeekMs              = -1;
        setMotion(motionName, entryId);
        // 跳转到装配期间记录的时间（战斗：最能还原；UI 因 update 门闸不会走到这）
        if (seekTarget >= 0)
            seek(seekTarget);
        else if (elapsed > 0)
            seek(m_timeMs + elapsed);
    }
}

bool SpineLayer::replaceAtlases(const std::vector<std::string>& atlasFiles, const std::string& skin)
{
    // 相同请求直接跳过（含初始图集）
    if (atlasFiles == m_requestedAtlases && skin == m_requestedSkin)
        return true;

    m_requestedAtlases = atlasFiles;
    m_requestedSkin    = skin;

    if (m_skeletonLoading)
    {
        // 骨架异步装配中：就绪后应用最新请求
        m_swapDirty = true;
        return true;
    }
    if (!m_skeleton || !m_skeleton->ownsData())
    {
        MG_LOG_W("SpineLayer: replaceAtlases requires instance-owned skeleton data");
        return false;
    }
    applyRequestedAtlases();
    return true;
}

void SpineLayer::applyRequestedAtlases()
{
    const uint32_t seq     = ++m_swapSeq;
    auto alive             = m_asyncAlive;
    const auto atlasFiles  = m_requestedAtlases;
    const std::string skin = m_requestedSkin;

    prewarmTexturesOnMain(collectTexturePaths(atlasFiles), [this, alive, seq, atlasFiles, skin]() {
        if (!alive->load() || seq != m_swapSeq || !m_skeleton)
            return;
        m_skeleton->skeletonData()->replaceAtlas(atlasFiles);
        if (!skin.empty())
            setSkin(skin);
    });
}

void SpineLayer::applyFashionDesc(const FashionSpineDesc& desc)
{
    // 身体图集/皮肤
    replaceAtlases(desc.atlases, desc.skin);
    // 武器/翅膀/光环附件
    syncAttachments(desc);
}

void SpineLayer::syncAttachments(const FashionSpineDesc& desc)
{
    const struct
    {
        FashionPosition pos;
        int32_t descId;
        int32_t* curId;
        MgSkeletonAnimation** node;
        int zOrder;
    } rules[] = {
        {FashionPosition::kWeapon, desc.weaponSpineId, &m_weaponSpineId, &m_weapon, kWeaponZOrder},
        {FashionPosition::kWing, desc.wingSpineId, &m_wingSpineId, &m_wing, kWingZOrder},
        {FashionPosition::kHalo, desc.ringSpineId, &m_ringSpineId, &m_halo, kHaloZOrder},
    };
    for (const auto& rule : rules)
    {
        if (rule.descId == *rule.curId)
            continue;
        if (*rule.node)
        {
            (*rule.node)->removeFromParent();
            *rule.node = nullptr;
        }
        *rule.curId = 0;
        if (rule.descId != 0)
        {
            *rule.node = createAttachment(rule.descId, rule.zOrder);
            if (*rule.node)
                *rule.curId = rule.descId;
        }
    }
}

MgSkeletonAnimation* SpineLayer::createAttachment(int32_t resSpineId, int zOrder)
{
    auto* node = MgSkeletonAnimation::create(resSpineId);
    if (!node)
    {
        MG_LOG_W("SpineLayer: attachment spine load failed {}", resSpineId);
        return nullptr;
    }
    // 附件为独立循环动画，自驱动（不随层的 step/seek）
    auto* data = node->skeletonData();
    if (data->findAnimation(kAttachmentAnim))
        node->setAnimation(0, kAttachmentAnim, true);
    else if (data->animationCount() > 0)
        node->setAnimation(0, data->animationAt(0).name(), true);
    // 身体骨架异步装配时，就绪后再显示
    node->setVisible(m_skeleton != nullptr);
    addChild(node, zOrder);
    return node;
}

void SpineLayer::setAttachmentsVisible(bool visible)
{
    if (m_weapon)
        m_weapon->setVisible(visible);
    if (m_wing)
        m_wing->setVisible(visible);
    if (m_halo)
        m_halo->setVisible(visible);
}

ax::Rect SpineLayer::skeletonBoundingBox() const
{
    if (!m_skeleton)
        return ax::Rect::ZERO;
    return m_skeleton->getBoundingBox();
}

bool SpineLayer::setSkin(const std::string& skinName)
{
    if (!m_skeleton || skinName.empty())
        return false;
    if (!m_skeleton->skeletonData()->hasSkin(skinName.c_str()))
    {
        MG_LOG_W("SpineLayer: skin not found '{}'", skinName);
        return false;
    }
    m_skeleton->setSkin(skinName);
    m_skeleton->setSlotsToSetupPose();
    return true;
}

bool SpineLayer::setMotion(const std::string& motionName, const std::string& entryId)
{
    m_timeMs    = 0;
    m_clipIndex = static_cast<size_t>(-1);
    m_motion    = nullptr;

    if (!m_motionMap)
        return false;

    // 骨架异步装配中：暂存，就绪后应用
    if (!m_skeleton)
    {
        m_pendingMotionName  = motionName;
        m_pendingMotionEntry = entryId;
        m_hasPendingMotion   = true;
        return true;
    }
    m_hasPendingMotion = false;

    const Motion* motion = m_motionMap->findMotion(motionName);
    if (!motion || motion->clips.empty())
        return false;

    const int startMs = motion->startTimeMs(entryId);
    if (startMs < 0)
        return false;

    for (size_t i = 0; i < motion->clipCount(); ++i)
    {
        const MotionClip* clip = motion->clipAtIndex(i);
        if (clip->type != MotionEntryType::kSpine || clip->source.empty())
            return false;

        MgAnimation anim = m_skeleton->findAnimation(clip->source);
        if (!anim)
        {
            MG_LOG_W("SpineLayer: animation not found '{}'", clip->source);
            return false;
        }

        MG_ASSERT(clip->durationMs == static_cast<int>(std::lround(anim.duration() * 1000.0f)));
    }

    m_motion = motion;
    m_timeMs = startMs;
    applyTrackTime(m_timeMs);
    return true;
}

int SpineLayer::durationMs() const
{
    return m_motion ? m_motion->durationMs() : 0;
}

void SpineLayer::applyTrackTime(int timeMs)
{
    if (!m_skeleton || !m_motion)
        return;

    int localMs            = 0;
    size_t index           = 0;
    const MotionClip* clip = m_motion->clipAt(timeMs, &localMs, &index);
    if (!clip)
        return;

    if (index != m_clipIndex)
    {
        const std::string& spineAnim = clip->source;
        if (!m_skeleton->setAnimation(0, spineAnim, false))
        {
            MG_LOG_W("SpineLayer: setAnimation failed '{}'", spineAnim);
            return;
        }
        // setAnimation(..., false) 会新建一条 不循环 的 TrackEntry。
        // Spine 默认把 trackEnd / endTime 设成动画时长，时间一到就 complete，然后丢掉这条 track。
        // 丢掉之后 getCurrent(0) 为空，姿势会掉回 setup pose
        // 所以这里要 keepCurrentTrackAlive(0) 保持这条 track 不被丢掉
        // 他的实现是把 trackEnd / endTime 设成 FLT_MAX，时间永远到不了
        // 这样 seek 到末尾、update(0)、或 update(dt) 稍稍越过动画时长，track 都还在，最后一帧能freeze住，下一次
        // seekCurrentTrack 也还有当前 entry
        m_skeleton->keepCurrentTrackAlive(0);
        m_clipIndex = index;
    }

    float tSec = static_cast<float>(std::max(0, localMs)) / 1000.0f;
    if (clip->durationMs > 0)
        tSec = std::min(tSec, static_cast<float>(clip->durationMs) / 1000.0f);

    if (m_skeleton->getCurrent(0))
    {
        // 跳转到指定时间点
        m_skeleton->seekCurrentTrack(0, tSec);
    }
    m_skeleton->update(0.0f);
}

void SpineLayer::step(int dtMs)
{
    if (dtMs <= 0)
        return;

    // 骨架异步装配中：时间照常记录，就绪后跳转到对应时间（战斗可接受，最能还原）
    if (!m_skeleton)
    {
        if (m_skeletonLoading)
        {
            m_timeMs += dtMs;
            if (m_pendingSeekMs >= 0)
                m_pendingSeekMs += dtMs;
        }
        return;
    }
    if (!m_motion)
        return;

    const int dur = durationMs();
    if (dur > 0 && m_timeMs >= dur)
        return;

    const size_t oldIndex = m_clipIndex;
    int applyMs           = dtMs;
    m_timeMs += dtMs;
    if (dur > 0 && m_timeMs > dur)
    {
        applyMs -= (m_timeMs - dur);
        m_timeMs = dur;
    }

    size_t newIndex = oldIndex;
    int localMs     = 0;
    m_motion->clipAt(m_timeMs, &localMs, &newIndex);
    if (newIndex != oldIndex)
        applyTrackTime(m_timeMs);
    else if (applyMs > 0)
        m_skeleton->update(static_cast<float>(applyMs) / 1000.0f);
}

void SpineLayer::seek(int timeMs)
{
    if (timeMs < 0)
        timeMs = 0;

    // 骨架异步装配中：记录绝对目标时间，就绪后跳转（战斗 sync 切动作的主路径）
    if (!m_skeleton)
    {
        if (m_skeletonLoading)
            m_pendingSeekMs = timeMs;
        return;
    }

    m_timeMs = timeMs;
    applyTrackTime(timeMs);
}

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
