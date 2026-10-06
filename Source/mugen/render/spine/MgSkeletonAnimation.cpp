#include "MgSkeletonAnimation.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/spine/MgSpineBackend.h"
#    include "mugen/render/spine/MgSpineLoadPipeline.h"
#    include "mugen/render/spine/SpineSkeletonCache.h"

#    include <algorithm>

NS_MG_BEGIN

MgSkeletonAnimation* MgSkeletonAnimation::createWithData(MgSkeletonDataPtr data)
{
    if (!data)
        return nullptr;
    auto* node = new (std::nothrow) MgSkeletonAnimation();
    if (node && node->initWithData(std::move(data)))
    {
        node->autorelease();
        return node;
    }
    AX_SAFE_DELETE(node);
    return nullptr;
}

MgSkeletonAnimation* MgSkeletonAnimation::createFromCache(int32_t resSpineId)
{
    return createWithData(SpineSkeletonCache::getInstance()->getOrCreate(resSpineId));
}

MgSkeletonAnimation* MgSkeletonAnimation::createFromCache(std::string_view skeletonFile,
                                                          const std::string& atlasFile,
                                                          float scale)
{
    std::vector<std::string> atlasFiles;
    if (!atlasFile.empty())
        atlasFiles.push_back(atlasFile);
    return createWithData(SpineSkeletonCache::getInstance()->getOrCreate(skeletonFile, atlasFiles, scale));
}

void MgSkeletonAnimation::createFromCacheAsync(int32_t resSpineId, CreateCallback onDone)
{
    SpineSkeletonCache::getInstance()->loadAsync(resSpineId, [onDone = std::move(onDone)](MgSkeletonDataPtr data) {
        onDone(data ? createWithData(std::move(data)) : nullptr);
    });
}

void MgSkeletonAnimation::createExclusiveAsync(std::string skeletonFile,
                                               std::vector<std::string> atlasFiles,
                                               float scale,
                                               CreateCallback onDone)
{
    MgSpineLoadPipeline::start(skeletonFile, std::move(atlasFiles), scale, true,
                               [onDone = std::move(onDone)](MgSkeletonDataPtr data) {
        onDone(data ? createWithData(std::move(data)) : nullptr);
    });
}

MgSkeletonAnimation::~MgSkeletonAnimation()
{
    // 先恢复嫁接（Axmol 后端为原 attachment 额外持有了引用）
    if (m_inner && !m_grafts.empty())
        takeGrafts();
    removeAllChildren();
    m_inner = nullptr;
    m_data.reset();
}

bool MgSkeletonAnimation::initWithData(MgSkeletonDataPtr data)
{
    if (!ax::Node::init())
        return false;

    m_data  = std::move(data);
    m_inner = MgSpineBackend::of(m_data->runtime()).createInner(m_data.get());
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
    auto grafts         = takeGrafts();
    const auto& backend = MgSpineBackend::of(m_data->runtime());
    backend.setSkin(m_inner, name);
    backend.setSlotsToSetupPose(m_inner);
    applyGrafts(std::move(grafts));
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

bool MgSkeletonAnimation::replaceAtlas(const std::vector<std::string>& atlasFiles)
{
    auto grafts   = takeGrafts();
    const bool ok = m_data->replaceAtlas(atlasFiles);
    applyGrafts(std::move(grafts));
    return ok;
}

bool MgSkeletonAnimation::replaceAtlas(MgAtlasHandlePtr atlas)
{
    auto grafts   = takeGrafts();
    const bool ok = m_data->replaceAtlas(std::move(atlas));
    applyGrafts(std::move(grafts));
    return ok;
}

bool MgSkeletonAnimation::graftSkinSlots(const MgSkeletonDataPtr& donor,
                                         const char* srcSkinName,
                                         const std::vector<std::string>& names)
{
    MG_ASSERT(isDataExclusive() && "graftSkinSlots requires exclusive skeleton data");
    if (!isDataExclusive())
    {
        MG_LOG_W("MgSkeletonAnimation: graftSkinSlots requires exclusive skeleton data");
        return false;
    }
    if (!donor || names.empty())
        return false;
    if (donor->runtime() != m_data->runtime())
    {
        MG_LOG_W("MgSkeletonAnimation: graftSkinSlots runtime mismatch");
        return false;
    }

    // 同名先撤销，保证每个 name 只属于一条嫁接记录
    clearSkinSlots(names);

    if (!MgSpineBackend::of(m_data->runtime()).replaceSkinSlots(m_inner, *donor, srcSkinName, names, m_slotOriginals))
        return false;

    m_grafts.push_back(Graft{donor, srcSkinName ? srcSkinName : "", names});
    return true;
}

void MgSkeletonAnimation::clearSkinSlots(const std::vector<std::string>& names)
{
    if (names.empty() || m_grafts.empty())
        return;

    MgSpineBackend::of(m_data->runtime()).clearSkinSlots(m_inner, names, m_slotOriginals);

    for (auto& graft : m_grafts)
    {
        auto& own = graft.names;
        own.erase(std::remove_if(own.begin(), own.end(),
                                 [&names](const std::string& n) {
            return std::find(names.begin(), names.end(), n) != names.end();
        }),
                  own.end());
    }
    m_grafts.erase(std::remove_if(m_grafts.begin(), m_grafts.end(), [](const Graft& g) { return g.names.empty(); }),
                   m_grafts.end());
}

std::vector<MgSkeletonAnimation::Graft> MgSkeletonAnimation::takeGrafts()
{
    std::vector<Graft> grafts = std::move(m_grafts);
    m_grafts.clear();
    const auto& backend = MgSpineBackend::of(m_data->runtime());
    for (const auto& graft : grafts)
        backend.clearSkinSlots(m_inner, graft.names, m_slotOriginals);
    return grafts;
}

void MgSkeletonAnimation::applyGrafts(std::vector<Graft> grafts)
{
    for (const auto& graft : grafts)
        graftSkinSlots(graft.donor, graft.skin.c_str(), graft.names);
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
