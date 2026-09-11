#include "mugen/render/spine/MgSpineBackend.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/spine/MgSpineProbe.h"

#    include "spine/spine-axmol.h"
#    include "spine/Animation.h"
#    include "spine/AnimationState.h"
#    include "spine/Atlas.h"
#    include "spine/AttachmentLoader.h"
#    include "spine/SkeletonBinary.h"
#    include "spine/SkeletonData.h"
#    include "spine/SkeletonJson.h"

#    include <limits>
#    include <new>

NS_MG_BEGIN

namespace
{

// 此处继承并实现onEnter是为了取消掉父类在onEnter中调用的scheduleUpdate()
// 让MgSkeletonAnimation可以手动控制驱动 update
class InnerAxmolAnimation : public spine::SkeletonAnimation
{
public:
    void onEnter() override { ax::Node::onEnter(); }
    void onExit() override { ax::Node::onExit(); }
};

spine::AxmolTextureLoader& sharedTextureLoader()
{
    static spine::AxmolTextureLoader loader;
    return loader;
}

MgAnimation wrapAnimation(spine::Animation* anim)
{
    if (!anim)
        return {};
    return MgAnimation(MgSpineRuntime::Axmol, anim, anim->getName().buffer(), anim->getDuration());
}

MgTrackEntry wrapTrack(spine::TrackEntry* entry)
{
    if (!entry)
        return {};
    const char* name = entry->getAnimation() ? entry->getAnimation()->getName().buffer() : "";
    return MgTrackEntry(MgSpineRuntime::Axmol, entry, entry->getTrackIndex(), name);
}

spine::Atlas* createAtlas(std::string_view atlasFile)
{
    const std::string path(atlasFile);
    auto* atlas = new (__FILE__, __LINE__) spine::Atlas(path.c_str(), &sharedTextureLoader(), true);
    if (!atlas || atlas->getPages().size() == 0)
    {
        MG_LOG_E("Spine: failed to read atlas '{}'", atlasFile);
        delete atlas;
        return nullptr;
    }
    return atlas;
}

void atlasAppend(spine::Atlas* self, std::string_view path)
{
    spine::Atlas* extra = createAtlas(path);
    if (!extra)
        return;

    auto& extraPages   = extra->getPages();
    auto& extraRegions = extra->getRegions();
    self->getPages().addAll(extraPages);
    self->getRegions().addAll(extraRegions);
    extraPages.clear();
    extraRegions.clear();
    delete extra;
}

spine::Atlas* createAtlas(const std::vector<std::string>& atlasFiles)
{
    if (atlasFiles.empty())
        return nullptr;
    spine::Atlas* merged = nullptr;
    for (const auto& path : atlasFiles)
    {
        if (!merged)
        {
            merged = createAtlas(std::string_view{path});
            continue;
        }
        atlasAppend(merged, path);
    }
    return merged;
}

spine::SkeletonAnimation* asAxmol(ax::Node* inner)
{
    return static_cast<spine::SkeletonAnimation*>(inner);
}

class AxmolBackend final : public MgSpineBackend
{
public:
    MgSkeletonData* load(const ax::Data& skelData,
                         const std::vector<std::string>& atlasFiles,
                         float scale,
                         std::string_view skeletonFile) const override
    {
        spine::Atlas* atlas = createAtlas(atlasFiles);
        if (!atlas)
            return nullptr;

        auto* attachmentLoader            = new (__FILE__, __LINE__) spine::AxmolAtlasAttachmentLoader(atlas);
        spine::SkeletonData* skeletonData = nullptr;
        const bool isJson                 = (firstPayloadByte(skelData) == static_cast<unsigned char>('{'));

        if (isJson)
        {
            std::string jsonText(reinterpret_cast<const char*>(skelData.getBytes()),
                                 static_cast<size_t>(skelData.getSize()));
            spine::SkeletonJson reader(attachmentLoader, false);
            reader.setScale(scale);
            skeletonData = reader.readSkeletonData(jsonText.c_str());
            if (!skeletonData)
            {
                MG_LOG_E("Spine: SkeletonJson failed '{}': {}", skeletonFile,
                         reader.getError().isEmpty() ? "unknown" : reader.getError().buffer());
                delete attachmentLoader;
                delete atlas;
                return nullptr;
            }
        }
        else
        {
            spine::SkeletonBinary reader(attachmentLoader, false);
            reader.setScale(scale);
            skeletonData = reader.readSkeletonData(skelData.getBytes(), static_cast<int>(skelData.getSize()));
            if (!skeletonData)
            {
                MG_LOG_E("Spine: SkeletonBinary failed '{}': {}", skeletonFile,
                         reader.getError().isEmpty() ? "unknown" : reader.getError().buffer());
                delete attachmentLoader;
                delete atlas;
                return nullptr;
            }
        }

        auto* out = new (std::nothrow) MgSkeletonData();
        if (!out)
        {
            delete skeletonData;
            delete attachmentLoader;
            delete atlas;
            return nullptr;
        }
        out->bindNative(MgSpineRuntime::Axmol, skeletonData, atlas, attachmentLoader);
        return out;
    }

    void dispose(MgSkeletonData& data) const override
    {
        delete static_cast<spine::SkeletonData*>(data.nativeSkeletonData());
        delete static_cast<spine::AttachmentLoader*>(data.nativeAttachmentLoader());
        delete static_cast<spine::Atlas*>(data.nativeAtlas());
        data.clearNative();
    }

    MgAnimation findAnimation(const MgSkeletonData& data, const char* name) const override
    {
        auto* native = static_cast<spine::SkeletonData*>(data.nativeSkeletonData());
        return wrapAnimation(native->findAnimation(name));
    }

    int animationCount(const MgSkeletonData& data) const override
    {
        auto* native = static_cast<spine::SkeletonData*>(data.nativeSkeletonData());
        return static_cast<int>(native->getAnimations().size());
    }

    MgAnimation animationAt(const MgSkeletonData& data, int index) const override
    {
        auto* native = static_cast<spine::SkeletonData*>(data.nativeSkeletonData());
        return wrapAnimation(native->getAnimations()[static_cast<size_t>(index)]);
    }

    bool hasSkin(const MgSkeletonData& data, const char* name) const override
    {
        auto* native = static_cast<spine::SkeletonData*>(data.nativeSkeletonData());
        return native->findSkin(name) != nullptr;
    }

    ax::Node* createInner(MgSkeletonData* data) const override
    {
        auto* node = new (std::nothrow) InnerAxmolAnimation();
        if (!node)
            return nullptr;
        node->initWithData(static_cast<spine::SkeletonData*>(data->nativeSkeletonData()), false);
        node->autorelease();
        return node;
    }

    bool isInnerValid(ax::Node* inner) const override
    {
        auto* animation = asAxmol(inner);
        return animation && animation->getSkeleton() && animation->getSkeleton()->getData();
    }

    ax::Rect boundingBox(ax::Node* inner) const override { return asAxmol(inner)->getBoundingBox(); }

    void update(ax::Node* inner, float dt) const override { asAxmol(inner)->update(dt); }

    MgTrackEntry setAnimation(ax::Node* inner, int trackIndex, const std::string& name, bool loop) const override
    {
        return wrapTrack(asAxmol(inner)->setAnimation(trackIndex, name, loop));
    }

    MgAnimation findAnimationOnNode(ax::Node* inner, const std::string& name) const override
    {
        return wrapAnimation(asAxmol(inner)->findAnimation(name));
    }

    MgTrackEntry getCurrent(ax::Node* inner, int trackIndex) const override
    {
        return wrapTrack(asAxmol(inner)->getCurrent(trackIndex));
    }

    void keepCurrentTrackAlive(ax::Node* inner, int trackIndex) const override
    {
        if (spine::TrackEntry* entry = asAxmol(inner)->getCurrent(trackIndex))
            entry->setTrackEnd(std::numeric_limits<float>::max());
    }

    void seekCurrentTrack(ax::Node* inner, int trackIndex, float timeSeconds) const override
    {
        if (spine::TrackEntry* entry = asAxmol(inner)->getCurrent(trackIndex))
        {
            entry->setTrackTime(timeSeconds);
            entry->setAnimationLast(timeSeconds);
        }
    }

    void setSkin(ax::Node* inner, const std::string& name) const override { asAxmol(inner)->setSkin(name); }

    void setSlotsToSetupPose(ax::Node* inner) const override { asAxmol(inner)->setSlotsToSetupPose(); }

    void setTimeScale(ax::Node* inner, float scale) const override { asAxmol(inner)->setTimeScale(scale); }

    void clearTracks(ax::Node* inner) const override { asAxmol(inner)->clearTracks(); }

    void setCompleteListener(ax::Node* inner, const MgSkeletonAnimation::CompleteListener& listener) const override
    {
        if (!listener)
        {
            asAxmol(inner)->setCompleteListener(nullptr);
            return;
        }
        asAxmol(inner)->setCompleteListener([listener](spine::TrackEntry* entry) { listener(wrapTrack(entry)); });
    }

    void setUpdateOnlyIfVisible(ax::Node* inner, bool value) const override
    {
        asAxmol(inner)->setUpdateOnlyIfVisible(value);
    }
};

AxmolBackend g_axmolBackend;

}  // namespace

const MgSpineBackend& MgSpineBackend::axmol()
{
    return g_axmolBackend;
}

NS_MG_END

#endif
