#include "MgSkeletonAnimation.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/conf/Config.h"
#    include "mugen/render/spine/MgSpineBackend.h"
#    include "mugen/render/spine/MgSpineLoadPipeline.h"
#    include "mugen/render/spine/MgSpineUtils.h"
#    include "mugen/render/spine/SpineSkeletonCache.h"

NS_MG_BEGIN

MgSkeletonAnimation* MgSkeletonAnimation::createWithData(MgSkeletonData* data)
{
    if (!data)
        return nullptr;
    auto* node = new (std::nothrow) MgSkeletonAnimation();
    if (node && node->initWithData(data))
    {
        node->autorelease();
        return node;
    }
    AX_SAFE_DELETE(node);
    return nullptr;
}

MgSkeletonAnimation* MgSkeletonAnimation::createWithOwnedData(MgSkeletonData* data)
{
    if (!data)
        return nullptr;
    auto* node = createWithData(data);
    if (!node)
    {
        delete data;
        return nullptr;
    }
    node->m_ownsData = true;
    return node;
}

MgSkeletonAnimation* MgSkeletonAnimation::create(int32_t resSpineId)
{
    auto* data = SpineSkeletonCache::getInstance()->getOrCreate(resSpineId);
    return data ? createWithData(data) : nullptr;
}

MgSkeletonAnimation* MgSkeletonAnimation::create(std::string_view skeletonFile, float scale)
{
    auto* data = SpineSkeletonCache::getInstance()->getOrCreate(skeletonFile, std::vector<std::string>{}, scale);
    return data ? createWithData(data) : nullptr;
}

MgSkeletonAnimation* MgSkeletonAnimation::create(std::string_view skeletonFile,
                                                 const std::string& atlasFile,
                                                 float scale)
{
    auto* data = SpineSkeletonCache::getInstance()->getOrCreate(skeletonFile, atlasFile, scale);
    return data ? createWithData(data) : nullptr;
}

void MgSkeletonAnimation::createAsync(int32_t resSpineId, std::function<void(MgSkeletonAnimation*)> onDone)
{
    auto* cfg = Config::getInstance()->getResSpineConfigById(resSpineId);
    if (!cfg || cfg->spine.empty())
    {
        MG_LOG_E("MgSkeletonAnimation::createAsync: ResSpine {} missing or spine empty", resSpineId);
        onDone(nullptr);
        return;
    }
    const float scale = cfg->scale > 0.0f ? cfg->scale : 1.0f;
    createAsync(cfg->spine, {atlasFromSpine(cfg->spine)}, scale, std::move(onDone));
}

void MgSkeletonAnimation::createAsync(std::string skeletonFile,
                                      std::vector<std::string> atlasFiles,
                                      float scale,
                                      std::function<void(MgSkeletonAnimation*)> onDone)
{
    MgSpineLoadPipeline::start(std::move(skeletonFile), std::move(atlasFiles), scale,
                               [onDone = std::move(onDone)](MgSkeletonData* data) mutable {
        onDone(data ? createWithOwnedData(data) : nullptr);
    });
}

MgSkeletonAnimation::~MgSkeletonAnimation()
{
    removeAllChildren();
    m_inner = nullptr;
    if (m_ownsData)
        delete m_data;
    m_data = nullptr;
}

bool MgSkeletonAnimation::initWithData(MgSkeletonData* data)
{
    if (!ax::Node::init())
        return false;

    m_data  = data;
    m_inner = MgSpineBackend::of(data->runtime()).createInner(data);
    if (!m_inner)
        return false;

    setCascadeColorEnabled(true);
    setCascadeOpacityEnabled(true);
    addChild(m_inner);
    return true;
}

bool MgSkeletonAnimation::isValid() const
{
    return m_data && m_inner && MgSpineBackend::of(m_data->runtime()).isInnerValid(m_inner);
}

void MgSkeletonAnimation::setAutoUpdate(bool enabled)
{
    if (m_autoUpdate == enabled)
        return;
    m_autoUpdate = enabled;
    if (!isRunning())
        return;
    if (enabled)
        scheduleUpdate();
    else
        unscheduleUpdate();
}

MgTrackEntry MgSkeletonAnimation::setAnimation(int trackIndex, const std::string& name, bool loop)
{
    return MgSpineBackend::of(m_data->runtime()).setAnimation(m_inner, trackIndex, name, loop);
}

MgAnimation MgSkeletonAnimation::findAnimation(const std::string& name) const
{
    return MgSpineBackend::of(m_data->runtime()).findAnimationOnNode(m_inner, name);
}

MgTrackEntry MgSkeletonAnimation::getCurrent(int trackIndex)
{
    return MgSpineBackend::of(m_data->runtime()).getCurrent(m_inner, trackIndex);
}

void MgSkeletonAnimation::keepCurrentTrackAlive(int trackIndex)
{
    MgSpineBackend::of(m_data->runtime()).keepCurrentTrackAlive(m_inner, trackIndex);
}

void MgSkeletonAnimation::seekCurrentTrack(int trackIndex, float timeSeconds)
{
    MgSpineBackend::of(m_data->runtime()).seekCurrentTrack(m_inner, trackIndex, timeSeconds);
}

void MgSkeletonAnimation::setSkin(const std::string& name)
{
    MgSpineBackend::of(m_data->runtime()).setSkin(m_inner, name);
}

void MgSkeletonAnimation::setSlotsToSetupPose()
{
    MgSpineBackend::of(m_data->runtime()).setSlotsToSetupPose(m_inner);
}

void MgSkeletonAnimation::setTimeScale(float scale)
{
    MgSpineBackend::of(m_data->runtime()).setTimeScale(m_inner, scale);
}

void MgSkeletonAnimation::clearTracks()
{
    MgSpineBackend::of(m_data->runtime()).clearTracks(m_inner);
}

void MgSkeletonAnimation::setCompleteListener(const CompleteListener& listener)
{
    MgSpineBackend::of(m_data->runtime()).setCompleteListener(m_inner, listener);
}

void MgSkeletonAnimation::setUpdateOnlyIfVisible(bool value)
{
    MgSpineBackend::of(m_data->runtime()).setUpdateOnlyIfVisible(m_inner, value);
}

void MgSkeletonAnimation::update(float dt)
{
    MgSpineBackend::of(m_data->runtime()).update(m_inner, dt);
}

void MgSkeletonAnimation::onEnter()
{
    ax::Node::onEnter();
    if (m_autoUpdate)
        scheduleUpdate();
}

void MgSkeletonAnimation::onExit()
{
    unscheduleUpdate();
    ax::Node::onExit();
}

ax::Rect MgSkeletonAnimation::getBoundingBox() const
{
    return MgSpineBackend::of(m_data->runtime()).boundingBox(m_inner);
}

NS_MG_END

#endif
