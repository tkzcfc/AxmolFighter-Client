#include "mugen/render/spine/MgSpineBackend.h"

#ifdef RUNTIME_IN_AXMOL
#    if MG_SPINE_USE_3_4

#        include "mugen/render/spine/MgSpineUtils.h"

#        include "spine_3_4/Cocos2dAttachmentLoader.h"
#        include "spine_3_4/AttachmentVertices.h"
#        include "spine_3_4/extension.h"
#        include "spine_3_4/spine-cocos2dx.h"

#        include <limits>
#        include <new>

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

spAtlas* createAtlas(std::string_view atlasFile, bool withTextures)
{
    const std::string path(atlasFile);
    spAtlas* atlas = spAtlas_createFromFile(path.c_str(), 0, withTextures ? 1 : 0);
    if (!atlas || !atlas->pages)
    {
        MG_LOG_E("Spine: failed to read atlas '{}'", atlasFile);
        if (atlas)
            spAtlas_dispose(atlas);
        return nullptr;
    }
    return atlas;
}

void spAtlas_append(spAtlas* self, const char* path, bool withTextures)
{
    spAtlas* atlas = spAtlas_createFromFile(path, self->rendererObject, withTextures ? 1 : 0);
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

void atlasAppend(spAtlas* self, std::string_view path, bool withTextures)
{
    const std::string file(path);
    spAtlas_append(self, file.c_str(), withTextures);
}

spAtlas* createAtlas(const std::vector<std::string>& atlasFiles, bool withTextures)
{
    if (atlasFiles.empty())
        return nullptr;
    spAtlas* merged = nullptr;
    for (const auto& path : atlasFiles)
    {
        if (!merged)
        {
            merged = createAtlas(std::string_view{path}, withTextures);
            continue;
        }
        atlasAppend(merged, path, withTextures);
    }
    return merged;
}

// 把 attachment 的贴图区域重指到 newAtlas 中的同名 region；找不到时保留旧 region
// 注意 rendererObject 在创建期已被 Cocos2dAttachmentLoader 包装成 AttachmentVertices（纹理+UV 烘焙），
// region 名取自 attachment 的 path，重指后需同步重建 AttachmentVertices
unsigned short quadTriangles[6] = {0, 1, 2, 2, 3, 0};

void relinkAttachment(spAttachment* attachment, spAtlas* newAtlas)
{
    if (!attachment)
        return;
    switch (attachment->type)
    {
    case SP_ATTACHMENT_REGION:
    {
        spRegionAttachment* regionAtt = SUB_CAST(spRegionAttachment, attachment);
        if (!regionAtt->path)
            return;
        spAtlasRegion* region = spAtlas_findRegion(newAtlas, regionAtt->path);
        if (!region)
        {
            MG_LOG_W("Spine: replaceAtlas missing region '{}'", regionAtt->path);
            return;
        }
        spRegionAttachment_setUVs(regionAtt, region->u, region->v, region->u2, region->v2, region->rotate);
        regionAtt->regionOffsetX        = region->offsetX;
        regionAtt->regionOffsetY        = region->offsetY;
        regionAtt->regionWidth          = region->width;
        regionAtt->regionHeight         = region->height;
        regionAtt->regionOriginalWidth  = region->originalWidth;
        regionAtt->regionOriginalHeight = region->originalHeight;
        spRegionAttachment_updateOffset(regionAtt);

        delete SUB_CAST(spine34::AttachmentVertices, regionAtt->rendererObject);
        auto* attachmentVertices = new spine34::AttachmentVertices((ax::Texture2D*)region->page->rendererObject, 4,
                                                                   quadTriangles, 6);
        for (int i = 0, ii = 0; i < 4; ++i, ii += 2)
        {
            attachmentVertices->_triangles->verts[i].texCoords.u = regionAtt->uvs[ii];
            attachmentVertices->_triangles->verts[i].texCoords.v = regionAtt->uvs[ii + 1];
        }
        regionAtt->rendererObject = attachmentVertices;
        break;
    }
    case SP_ATTACHMENT_MESH:
    case SP_ATTACHMENT_LINKED_MESH:
    {
        spMeshAttachment* mesh = SUB_CAST(spMeshAttachment, attachment);
        if (!mesh->path)
            return;
        spAtlasRegion* region = spAtlas_findRegion(newAtlas, mesh->path);
        if (!region)
        {
            MG_LOG_W("Spine: replaceAtlas missing region '{}'", mesh->path);
            return;
        }
        mesh->regionU               = region->u;
        mesh->regionV               = region->v;
        mesh->regionU2              = region->u2;
        mesh->regionV2              = region->v2;
        mesh->regionRotate          = region->rotate;
        mesh->regionOffsetX         = region->offsetX;
        mesh->regionOffsetY         = region->offsetY;
        mesh->regionWidth           = region->width;
        mesh->regionHeight          = region->height;
        mesh->regionOriginalWidth   = region->originalWidth;
        mesh->regionOriginalHeight  = region->originalHeight;
        spMeshAttachment_updateUVs(mesh);

        delete SUB_CAST(spine34::AttachmentVertices, mesh->rendererObject);
        auto* attachmentVertices = new spine34::AttachmentVertices((ax::Texture2D*)region->page->rendererObject,
                                                                   mesh->super.worldVerticesLength >> 1,
                                                                   mesh->triangles, mesh->trianglesCount);
        for (int i = 0, ii = 0, nn = mesh->super.worldVerticesLength; ii < nn; ++i, ii += 2)
        {
            attachmentVertices->_triangles->verts[i].texCoords.u = mesh->uvs[ii];
            attachmentVertices->_triangles->verts[i].texCoords.v = mesh->uvs[ii + 1];
        }
        mesh->rendererObject = attachmentVertices;
        break;
    }
    default:
        break;
    }
}

void relinkSkin(spSkin* skin, spAtlas* newAtlas)
{
    if (!skin)
        return;
    _spSkin* self = SUB_CAST(_spSkin, skin);
    for (_Entry* entry = self->entries; entry; entry = entry->next)
        relinkAttachment(entry->attachment, newAtlas);
}

// 用给定 loader 解析 skeletonData；失败返回 nullptr（loader/atlas 由调用方处理）
spSkeletonData* parseSkeletonData(const ax::Data& skelData,
                                  spAttachmentLoader* attachmentLoader,
                                  float scale,
                                  std::string_view skeletonFile)
{
    spSkeletonData* skeletonData = nullptr;
    const bool isJson            = checkIsJsonFormat(skelData);
    if (isJson)
    {
        std::string jsonText(reinterpret_cast<const char*>(skelData.getBytes()),
                             static_cast<size_t>(skelData.getSize()));
        spSkeletonJson* reader = spSkeletonJson_createWithLoader(attachmentLoader);
        reader->scale          = scale;
        skeletonData           = spSkeletonJson_readSkeletonData(reader, jsonText.c_str());
        if (!skeletonData)
            MG_LOG_E("Spine: SkeletonJson failed '{}': {}", skeletonFile, reader->error ? reader->error : "unknown");
        spSkeletonJson_dispose(reader);
    }
    else
    {
        spSkeletonBinary* reader = spSkeletonBinary_createWithLoader(attachmentLoader);
        reader->scale            = scale;
        skeletonData =
            spSkeletonBinary_readSkeletonData(reader, skelData.getBytes(), static_cast<int>(skelData.getSize()));
        if (!skeletonData)
            MG_LOG_E("Spine: SkeletonBinary failed '{}': {}", skeletonFile, reader->error ? reader->error : "unknown");
        spSkeletonBinary_dispose(reader);
    }
    return skeletonData;
}

MgSkeletonData* wrapData(spSkeletonData* skeletonData, spAtlas* atlas, spAttachmentLoader* attachmentLoader)
{
    auto* out = new (std::nothrow) MgSkeletonData();
    if (!out)
        return nullptr;
    out->bindNative(MgSpineRuntime::Spine34, skeletonData, atlas, attachmentLoader);
    return out;
}

// parseWithAtlas 用纯 loader 解析后 rendererObject 仍是 spAtlasRegion*，
// 这里用正式渲染 loader 补调 configureAttachment：建 AttachmentVertices 并设置 attachment->attachmentLoader 回指
void materializeSkin(spSkin* skin, spAttachmentLoader* renderLoader)
{
    if (!skin)
        return;
    _spSkin* self = SUB_CAST(_spSkin, skin);
    for (_Entry* entry = self->entries; entry; entry = entry->next)
        spAttachmentLoader_configureAttachment(renderLoader, entry->attachment);
}

// 逐个访问 skin；defaultSkin 通常就是 skins[0]，需去重避免重复处理
template <typename F>
void forEachSkin(spSkeletonData* skeletonData, F&& fn)
{
    for (int i = 0; i < skeletonData->skinsCount; ++i)
        fn(skeletonData->skins[i]);
    spSkin* defaultSkin = skeletonData->defaultSkin;
    if (defaultSkin)
    {
        bool found = false;
        for (int i = 0; i < skeletonData->skinsCount; ++i)
        {
            if (skeletonData->skins[i] == defaultSkin)
            {
                found = true;
                break;
            }
        }
        if (!found)
            fn(defaultSkin);
    }
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
        spAtlas* atlas = createAtlas(atlasFiles, true);
        if (!atlas)
            return nullptr;

        spAttachmentLoader* attachmentLoader = SUPER(Cocos2dAttachmentLoader_create(atlas));
        spSkeletonData* skeletonData         = parseSkeletonData(skelData, attachmentLoader, scale, skeletonFile);
        if (!skeletonData)
        {
            spAttachmentLoader_dispose(attachmentLoader);
            spAtlas_dispose(atlas);
            return nullptr;
        }

        auto* out = wrapData(skeletonData, atlas, attachmentLoader);
        if (!out)
        {
            spSkeletonData_dispose(skeletonData);
            spAttachmentLoader_dispose(attachmentLoader);
            spAtlas_dispose(atlas);
        }
        return out;
    }

    void* createAtlasHandle(const std::vector<std::string>& atlasFiles) const override
    {
        // 不建纹理（纯文本解析），纹理由 bindAtlasTextures 在材质化前绑定
        return createAtlas(atlasFiles, false);
    }

    void disposeAtlasHandle(void* atlasHandle) const override
    {
        if (atlasHandle)
            spAtlas_dispose(static_cast<spAtlas*>(atlasHandle));
    }

    void bindAtlasTextures(void* atlasHandle) const override
    {
        if (atlasHandle)
            spAtlas_bindTextures(static_cast<spAtlas*>(atlasHandle));
    }

    MgSkeletonData* parseWithAtlas(const ax::Data& skelData,
                                   void* atlasHandle,
                                   float scale,
                                   std::string_view skeletonFile) const override
    {
        spAtlas* atlas = SUB_CAST(spAtlas, atlasHandle);
        if (!atlas)
            return nullptr;

        // 纯 spAtlasAttachmentLoader：不建 AttachmentVertices 渲染对象，后台线程安全
        spAttachmentLoader* attachmentLoader = SUPER(spAtlasAttachmentLoader_create(atlas));
        spSkeletonData* skeletonData         = parseSkeletonData(skelData, attachmentLoader, scale, skeletonFile);
        if (!skeletonData)
        {
            spAttachmentLoader_dispose(attachmentLoader);
            spAtlas_dispose(atlas);
            return nullptr;
        }

        auto* out = wrapData(skeletonData, atlas, attachmentLoader);
        if (!out)
        {
            spSkeletonData_dispose(skeletonData);
            spAttachmentLoader_dispose(attachmentLoader);
            spAtlas_dispose(atlas);
        }
        return out;
    }

    void materialize(MgSkeletonData& data) const override
    {
        auto* skeletonData = static_cast<spSkeletonData*>(data.nativeSkeletonData());
        auto* atlas        = static_cast<spAtlas*>(data.nativeAtlas());
        if (!skeletonData || !atlas)
            return;

        spAttachmentLoader* renderLoader = SUPER(Cocos2dAttachmentLoader_create(atlas));
        forEachSkin(skeletonData, [&](spSkin* skin) { materializeSkin(skin, renderLoader); });

        // 纯 loader 仅 parse 期使用
        if (auto* old = static_cast<spAttachmentLoader*>(data.nativeAttachmentLoader()))
            spAttachmentLoader_dispose(old);
        data.bindNative(MgSpineRuntime::Spine34, skeletonData, atlas, renderLoader);
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

    bool replaceAtlas(MgSkeletonData& data, const std::vector<std::string>& atlasFiles) const override
    {
        auto* skeletonData = static_cast<spSkeletonData*>(data.nativeSkeletonData());
        if (!skeletonData)
        {
            MG_LOG_E("Spine: replaceAtlas on invalid skeleton data");
            return false;
        }
        spAtlas* atlas = createAtlas(atlasFiles, true);
        if (!atlas)
        {
            MG_LOG_E("Spine: replaceAtlas failed to build atlas");
            return false;
        }

        // 就地重指所有 skin 中的 attachment region；使用该数据的节点下一帧自动生效
        forEachSkin(skeletonData, [&](spSkin* skin) { relinkSkin(skin, atlas); });

        // attachmentLoader 仅 parse 期使用，且 attachment 持有它的回指指针（disposeAttachment 用），保留不动
        if (auto* old = static_cast<spAtlas*>(data.nativeAtlas()))
            spAtlas_dispose(old);
        data.bindNative(MgSpineRuntime::Spine34, skeletonData, atlas, data.nativeAttachmentLoader());
        return true;
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

#    endif
#endif
