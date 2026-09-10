#include "mugen/render/spine/MgSpineBackend.h"

#ifdef RUNTIME_IN_AXMOL
#if MG_SPINE_USE_3_4

#include "mugen/render/spine/MgSpineProbe.h"

#include "spine_3_4/Cocos2dAttachmentLoader.h"
#include "spine_3_4/extension.h"
#include "spine_3_4/spine-cocos2dx.h"

#include <limits>
#include <new>

NS_MG_BEGIN

namespace
{

// 此处继承并实现onEnter是为了取消掉父类在onEnter中调用的scheduleUpdate()
// 让MgSkeletonAnimation可以手动控制驱动 update
class InnerSpine34Animation : public spine34::SkeletonAnimation
{
public:
    void onEnter() override { ax::Node::onEnter(); }
    void onExit() override { ax::Node::onExit(); }
};

MgAnimation wrapAnimation(spAnimation* anim)
{
    if (!anim)
        return {};
    return MgAnimation(MgSpineRuntime::Spine34, anim, anim->name, anim->duration);
}

MgTrackEntry wrapTrack(spTrackEntry* entry, int trackIndex)
{
    if (!entry)
        return {};
    const char* name = (entry->animation && entry->animation->name) ? entry->animation->name : "";
    return MgTrackEntry(MgSpineRuntime::Spine34, entry, trackIndex, name);
}

spAtlas* createAtlas(std::string_view atlasFile)
{
    const std::string path(atlasFile);
    spAtlas* atlas = spAtlas_createFromFile(path.c_str(), 0);
    if (!atlas || !atlas->pages)
    {
        MG_LOG_E("Spine: failed to read atlas '{}'", atlasFile);
        if (atlas)
            spAtlas_dispose(atlas);
        return nullptr;
    }
    return atlas;
}

void spAtlas_append(spAtlas* self, const char* path)
{
    spAtlas* atlas = spAtlas_createFromFile(path, self->rendererObject);
    if (!atlas || !atlas->pages)
    {
        if (atlas)
            spAtlas_dispose(atlas);
        return;
    }

    for (spAtlasPage* page = atlas->pages; page; page = page->next)
        CONST_CAST(spAtlas*, page->atlas) = self;

    if (!self->pages)
    {
        self->pages    = atlas->pages;
        self->regions  = atlas->regions;
        atlas->pages   = nullptr;
        atlas->regions = nullptr;
        FREE(atlas);
        return;
    }

    spAtlasPage* page = self->pages;
    while (page)
    {
        spAtlasPage* next = page->next;
        if (!next)
        {
            page->next = atlas->pages;
            break;
        }
        page = next;
    }

    if (!self->regions)
        self->regions = atlas->regions;
    else
    {
        spAtlasRegion* region = self->regions;
        while (region)
        {
            spAtlasRegion* next = region->next;
            if (!next)
            {
                region->next = atlas->regions;
                break;
            }
            region = next;
        }
    }

    atlas->pages   = nullptr;
    atlas->regions = nullptr;
    FREE(atlas);
}

void atlasAppend(spAtlas* self, std::string_view path)
{
    const std::string file(path);
    spAtlas_append(self, file.c_str());
}

spAtlas* createAtlas(const std::vector<std::string>& atlasFiles)
{
    if (atlasFiles.empty())
        return nullptr;
    spAtlas* merged = nullptr;
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

spine34::SkeletonAnimation* asSpine34(ax::Node* inner)
{
    return static_cast<spine34::SkeletonAnimation*>(inner);
}

class Spine34Backend final : public MgSpineBackend
{
public:
    MgSkeletonData* load(const ax::Data& skelData,
                         const std::vector<std::string>& atlasFiles,
                         float scale,
                         std::string_view skeletonFile) const override
    {
        spAtlas* atlas = createAtlas(atlasFiles);
        if (!atlas)
            return nullptr;

        spAttachmentLoader* attachmentLoader = SUPER(Cocos2dAttachmentLoader_create(atlas));
        spSkeletonData* skeletonData         = nullptr;
        const bool isJson                    = (firstPayloadByte(skelData) == static_cast<unsigned char>('{'));

        if (isJson)
        {
            std::string jsonText(reinterpret_cast<const char*>(skelData.getBytes()),
                                 static_cast<size_t>(skelData.getSize()));
            spSkeletonJson* reader = spSkeletonJson_createWithLoader(attachmentLoader);
            reader->scale          = scale;
            skeletonData           = spSkeletonJson_readSkeletonData(reader, jsonText.c_str());
            if (!skeletonData)
            {
                MG_LOG_E("Spine: SkeletonJson failed '{}': {}", skeletonFile,
                         reader->error ? reader->error : "unknown");
                spSkeletonJson_dispose(reader);
                spAttachmentLoader_dispose(attachmentLoader);
                spAtlas_dispose(atlas);
                return nullptr;
            }
            spSkeletonJson_dispose(reader);
        }
        else
        {
            spSkeletonBinary* reader = spSkeletonBinary_createWithLoader(attachmentLoader);
            reader->scale            = scale;
            skeletonData =
                spSkeletonBinary_readSkeletonData(reader, skelData.getBytes(), static_cast<int>(skelData.getSize()));
            if (!skeletonData)
            {
                MG_LOG_E("Spine: SkeletonBinary failed '{}': {}", skeletonFile,
                         reader->error ? reader->error : "unknown");
                spSkeletonBinary_dispose(reader);
                spAttachmentLoader_dispose(attachmentLoader);
                spAtlas_dispose(atlas);
                return nullptr;
            }
            spSkeletonBinary_dispose(reader);
        }

        auto* out = new (std::nothrow) MgSkeletonData();
        if (!out)
        {
            spSkeletonData_dispose(skeletonData);
            spAttachmentLoader_dispose(attachmentLoader);
            spAtlas_dispose(atlas);
            return nullptr;
        }
        out->bindNative(MgSpineRuntime::Spine34, skeletonData, atlas, attachmentLoader);
        return out;
    }

    void dispose(MgSkeletonData& data) const override
    {
        if (data.nativeSkeletonData())
            spSkeletonData_dispose(static_cast<spSkeletonData*>(data.nativeSkeletonData()));
        if (data.nativeAttachmentLoader())
            spAttachmentLoader_dispose(static_cast<spAttachmentLoader*>(data.nativeAttachmentLoader()));
        if (data.nativeAtlas())
            spAtlas_dispose(static_cast<spAtlas*>(data.nativeAtlas()));
        data.clearNative();
    }

    MgAnimation findAnimation(const MgSkeletonData& data, const char* name) const override
    {
        auto* native = static_cast<spSkeletonData*>(data.nativeSkeletonData());
        return wrapAnimation(spSkeletonData_findAnimation(native, name));
    }

    int animationCount(const MgSkeletonData& data) const override
    {
        auto* native = static_cast<spSkeletonData*>(data.nativeSkeletonData());
        return native->animationsCount;
    }

    MgAnimation animationAt(const MgSkeletonData& data, int index) const override
    {
        auto* native = static_cast<spSkeletonData*>(data.nativeSkeletonData());
        return wrapAnimation(native->animations[index]);
    }

    bool hasSkin(const MgSkeletonData& data, const char* name) const override
    {
        auto* native = static_cast<spSkeletonData*>(data.nativeSkeletonData());
        return spSkeletonData_findSkin(native, name) != nullptr;
    }

    ax::Node* createInner(MgSkeletonData* data) const override
    {
        auto* node = new (std::nothrow) InnerSpine34Animation();
        if (!node)
            return nullptr;
        node->initWithData(static_cast<spSkeletonData*>(data->nativeSkeletonData()), false);
        node->autorelease();
        return node;
    }

    bool isInnerValid(ax::Node* inner) const override
    {
        auto* animation = asSpine34(inner);
        return animation && animation->getSkeleton() && animation->getSkeleton()->data;
    }

    ax::Rect boundingBox(ax::Node* inner) const override { return asSpine34(inner)->getBoundingBox(); }

    void update(ax::Node* inner, float dt) const override { asSpine34(inner)->update(dt); }

    MgTrackEntry setAnimation(ax::Node* inner, int trackIndex, const std::string& name, bool loop) const override
    {
        return wrapTrack(asSpine34(inner)->setAnimation(trackIndex, name, loop), trackIndex);
    }

    MgAnimation findAnimationOnNode(ax::Node* inner, const std::string& name) const override
    {
        return wrapAnimation(asSpine34(inner)->findAnimation(name));
    }

    MgTrackEntry getCurrent(ax::Node* inner, int trackIndex) const override
    {
        return wrapTrack(asSpine34(inner)->getCurrent(trackIndex), trackIndex);
    }

    void keepCurrentTrackAlive(ax::Node* inner, int trackIndex) const override
    {
        if (spTrackEntry* entry = asSpine34(inner)->getCurrent(trackIndex))
            entry->endTime = std::numeric_limits<float>::max();
    }

    void seekCurrentTrack(ax::Node* inner, int trackIndex, float timeSeconds) const override
    {
        if (spTrackEntry* entry = asSpine34(inner)->getCurrent(trackIndex))
        {
            entry->time     = timeSeconds;
            entry->lastTime = timeSeconds;
        }
    }

    void setSkin(ax::Node* inner, const std::string& name) const override { asSpine34(inner)->setSkin(name); }

    void setSlotsToSetupPose(ax::Node* inner) const override { asSpine34(inner)->setSlotsToSetupPose(); }

    void setTimeScale(ax::Node* inner, float scale) const override { asSpine34(inner)->setTimeScale(scale); }

    void clearTracks(ax::Node* inner) const override { asSpine34(inner)->clearTracks(); }

    void setCompleteListener(ax::Node* inner, const MgSkeletonAnimation::CompleteListener& listener) const override
    {
        if (!listener)
        {
            asSpine34(inner)->setCompleteListener(nullptr);
            return;
        }
        auto* animation = asSpine34(inner);
        animation->setCompleteListener([animation, listener](int trackIndex, int) {
            listener(wrapTrack(animation->getCurrent(trackIndex), trackIndex));
        });
    }

    void setUpdateOnlyIfVisible(ax::Node* inner, bool) const override { (void)inner; }
};

Spine34Backend g_spine34Backend;

}  // namespace

const MgSpineBackend& MgSpineBackend::spine34()
{
    return g_spine34Backend;
}

NS_MG_END

#endif
#endif
