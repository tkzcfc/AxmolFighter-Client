#include "mugen/render/spine/MgSpineBackend.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/spine/MgSpineUtils.h"

#    include "spine/spine-axmol.h"
#    include "spine/Animation.h"
#    include "spine/AnimationState.h"
#    include "spine/Atlas.h"
#    include "spine/AtlasAttachmentLoader.h"
#    include "spine/AttachmentLoader.h"
#    include "spine/AttachmentVertices.h"
#    include "spine/MeshAttachment.h"
#    include "spine/RegionAttachment.h"
#    include "spine/RTTI.h"
#    include "spine/SkeletonBinary.h"
#    include "spine/SkeletonData.h"
#    include "spine/SkeletonJson.h"
#    include "spine/Skin.h"

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

spine::Atlas* createAtlas(std::string_view atlasFile, bool withTextures)
{
    const std::string path(atlasFile);
    // withTextures=false 时仅解析文本，page->texturePath 保留完整路径，纹理由 bindAtlasTextures 后续绑定
    auto* atlas = new (__FILE__, __LINE__) spine::Atlas(path.c_str(), &sharedTextureLoader(), withTextures);
    if (!atlas || atlas->getPages().size() == 0)
    {
        MG_LOG_E("Spine: failed to read atlas '{}'", atlasFile);
        delete atlas;
        return nullptr;
    }
    return atlas;
}

void atlasAppend(spine::Atlas* self, std::string_view path, bool withTextures)
{
    spine::Atlas* extra = createAtlas(path, withTextures);
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

spine::Atlas* createAtlas(const std::vector<std::string>& atlasFiles, bool withTextures)
{
    if (atlasFiles.empty())
        return nullptr;
    spine::Atlas* merged = nullptr;
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

// rendererObject 渲染包装：AttachmentVertices（纹理+UV 烘焙）。
// setRendererObject 会先 dispose 旧的渲染对象（若创建时带 dispose 回调）
void deleteAttachmentVertices(void* vertices)
{
    delete static_cast<spine::AttachmentVertices*>(vertices);
}

unsigned short quadTriangles[6] = {0, 1, 2, 2, 3, 0};

void wrapAttachmentVertices(spine::Attachment* attachment, spine::AtlasRegion* region)
{
    const spine::RTTI& rtti = attachment->getRTTI();
    if (rtti.instanceOf(spine::RegionAttachment::rtti))
    {
        auto* regionAtt            = static_cast<spine::RegionAttachment*>(attachment);
        auto* attachmentVertices   = new spine::AttachmentVertices(
            static_cast<ax::Texture2D*>(region->page->getRendererObject()), 4, quadTriangles, 6);
        auto& uvs = regionAtt->getUVs();
        for (int i = 0, ii = 0; i < 4; ++i, ii += 2)
        {
            attachmentVertices->_triangles->verts[i].texCoords.u = uvs[ii];
            attachmentVertices->_triangles->verts[i].texCoords.v = uvs[ii + 1];
        }
        regionAtt->setRendererObject(attachmentVertices, deleteAttachmentVertices);
        return;
    }
    if (rtti.instanceOf(spine::MeshAttachment::rtti))
    {
        auto* mesh               = static_cast<spine::MeshAttachment*>(attachment);
        auto* attachmentVertices =
            new spine::AttachmentVertices(static_cast<ax::Texture2D*>(region->page->getRendererObject()),
                                          mesh->getWorldVerticesLength() >> 1, mesh->getTriangles().buffer(),
                                          static_cast<int>(mesh->getTriangles().size()));
        auto& uvs = mesh->getUVs();
        for (int i = 0, ii = 0, nn = mesh->getWorldVerticesLength(); ii < nn; ++i, ii += 2)
        {
            attachmentVertices->_triangles->verts[i].texCoords.u = uvs[ii];
            attachmentVertices->_triangles->verts[i].texCoords.v = uvs[ii + 1];
        }
        mesh->setRendererObject(attachmentVertices, deleteAttachmentVertices);
    }
}

// 把 attachment 的贴图区域重指到 newAtlas 中的同名 region；找不到时保留旧 region
// region 名取自 attachment 的 path，重指后需同步重建 AttachmentVertices
void relinkAttachment(spine::Attachment* attachment, spine::Atlas* newAtlas)
{
    if (!attachment)
        return;

    const spine::RTTI& rtti = attachment->getRTTI();
    if (rtti.instanceOf(spine::RegionAttachment::rtti))
    {
        auto* regionAtt            = static_cast<spine::RegionAttachment*>(attachment);
        spine::AtlasRegion* region = newAtlas->findRegion(regionAtt->getPath());
        if (!region)
        {
            MG_LOG_W("Spine: replaceAtlas missing region '{}'", regionAtt->getPath().buffer());
            return;
        }
        regionAtt->setUVs(region->u, region->v, region->u2, region->v2, region->rotate);
        regionAtt->setRegionOffsetX(region->offsetX);
        regionAtt->setRegionOffsetY(region->offsetY);
        regionAtt->setRegionWidth(static_cast<float>(region->width));
        regionAtt->setRegionHeight(static_cast<float>(region->height));
        regionAtt->setRegionOriginalWidth(static_cast<float>(region->originalWidth));
        regionAtt->setRegionOriginalHeight(static_cast<float>(region->originalHeight));
        regionAtt->updateOffset();
        wrapAttachmentVertices(attachment, region);
        return;
    }
    if (rtti.instanceOf(spine::MeshAttachment::rtti))
    {
        auto* mesh                 = static_cast<spine::MeshAttachment*>(attachment);
        spine::AtlasRegion* region = newAtlas->findRegion(mesh->getPath());
        if (!region)
        {
            MG_LOG_W("Spine: replaceAtlas missing region '{}'", mesh->getPath().buffer());
            return;
        }
        mesh->setRegionU(region->u);
        mesh->setRegionV(region->v);
        mesh->setRegionU2(region->u2);
        mesh->setRegionV2(region->v2);
        mesh->setRegionRotate(region->rotate);
        mesh->setRegionDegrees(region->degrees);
        mesh->setRegionOffsetX(region->offsetX);
        mesh->setRegionOffsetY(region->offsetY);
        mesh->setRegionWidth(static_cast<float>(region->width));
        mesh->setRegionHeight(static_cast<float>(region->height));
        mesh->setRegionOriginalWidth(static_cast<float>(region->originalWidth));
        mesh->setRegionOriginalHeight(static_cast<float>(region->originalHeight));
        mesh->updateUVs();
        wrapAttachmentVertices(attachment, region);
    }
}

void relinkSkin(spine::Skin* skin, spine::Atlas* newAtlas)
{
    if (!skin)
        return;
    auto entries = skin->getAttachments();
    while (entries.hasNext())
    {
        spine::Skin::AttachmentMap::Entry& entry = entries.next();
        relinkAttachment(entry._attachment, newAtlas);
    }
}

// parseWithAtlas 用纯 loader 解析后 rendererObject 仍是 AtlasRegion*，这里补建渲染对象
void materializeAttachment(spine::Attachment* attachment)
{
    if (!attachment)
        return;
    const spine::RTTI& rtti      = attachment->getRTTI();
    spine::AtlasRegion* region   = nullptr;
    if (rtti.instanceOf(spine::RegionAttachment::rtti))
        region = static_cast<spine::AtlasRegion*>(static_cast<spine::RegionAttachment*>(attachment)->getRendererObject());
    else if (rtti.instanceOf(spine::MeshAttachment::rtti))
        region = static_cast<spine::AtlasRegion*>(static_cast<spine::MeshAttachment*>(attachment)->getRendererObject());
    if (!region)
        return;
    wrapAttachmentVertices(attachment, region);
}

void materializeSkin(spine::Skin* skin)
{
    if (!skin)
        return;
    auto entries = skin->getAttachments();
    while (entries.hasNext())
    {
        spine::Skin::AttachmentMap::Entry& entry = entries.next();
        materializeAttachment(entry._attachment);
    }
}

// 逐个访问 skin；defaultSkin 通常也在 getSkins() 里，需去重避免重复处理
template <typename F>
void forEachSkin(spine::SkeletonData* skeletonData, F&& fn)
{
    auto& skins = skeletonData->getSkins();
    for (size_t i = 0; i < skins.size(); ++i)
        fn(skins[i]);
    spine::Skin* defaultSkin = skeletonData->getDefaultSkin();
    if (defaultSkin)
    {
        bool found = false;
        for (size_t i = 0; i < skins.size(); ++i)
        {
            if (skins[i] == defaultSkin)
            {
                found = true;
                break;
            }
        }
        if (!found)
            fn(defaultSkin);
    }
}

// 用给定 loader 解析 skeletonData；失败返回 nullptr（loader/atlas 由调用方处理）
spine::SkeletonData* parseSkeletonData(const ax::Data& skelData,
                                       spine::AttachmentLoader* attachmentLoader,
                                       float scale,
                                       std::string_view skeletonFile)
{
    spine::SkeletonData* skeletonData = nullptr;
    const bool isJson                 = checkIsJsonFormat(skelData);
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
        }
    }
    return skeletonData;
}

MgSkeletonData* wrapData(spine::SkeletonData* skeletonData, spine::Atlas* atlas, spine::AttachmentLoader* attachmentLoader)
{
    auto* out = new (std::nothrow) MgSkeletonData();
    if (!out)
        return nullptr;
    out->bindNative(MgSpineRuntime::Axmol, skeletonData, atlas, attachmentLoader);
    return out;
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
        spine::Atlas* atlas = createAtlas(atlasFiles, true);
        if (!atlas)
            return nullptr;

        auto* attachmentLoader            = new (__FILE__, __LINE__) spine::AxmolAtlasAttachmentLoader(atlas);
        spine::SkeletonData* skeletonData = parseSkeletonData(skelData, attachmentLoader, scale, skeletonFile);
        if (!skeletonData)
        {
            delete attachmentLoader;
            delete atlas;
            return nullptr;
        }

        auto* out = wrapData(skeletonData, atlas, attachmentLoader);
        if (!out)
        {
            delete skeletonData;
            delete attachmentLoader;
            delete atlas;
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
        delete static_cast<spine::Atlas*>(atlasHandle);
    }

    void bindAtlasTextures(void* atlasHandle) const override
    {
        auto* atlas = static_cast<spine::Atlas*>(atlasHandle);
        if (!atlas)
            return;
        auto& pages = atlas->getPages();
        for (size_t i = 0; i < pages.size(); ++i)
        {
            spine::AtlasPage* page = pages[i];
            if (!page->getRendererObject())
                sharedTextureLoader().load(*page, page->texturePath);
        }
    }

    MgSkeletonData* parseWithAtlas(const ax::Data& skelData,
                                   void* atlasHandle,
                                   float scale,
                                   std::string_view skeletonFile) const override
    {
        auto* atlas = static_cast<spine::Atlas*>(atlasHandle);
        if (!atlas)
            return nullptr;

        // 纯 AtlasAttachmentLoader：不建 AttachmentVertices 渲染对象，后台线程安全
        auto* attachmentLoader            = new (__FILE__, __LINE__) spine::AtlasAttachmentLoader(atlas);
        spine::SkeletonData* skeletonData = parseSkeletonData(skelData, attachmentLoader, scale, skeletonFile);
        if (!skeletonData)
        {
            delete attachmentLoader;
            delete atlas;
            return nullptr;
        }

        auto* out = wrapData(skeletonData, atlas, attachmentLoader);
        if (!out)
        {
            delete skeletonData;
            delete attachmentLoader;
            delete atlas;
        }
        return out;
    }

    void materialize(MgSkeletonData& data) const override
    {
        auto* skeletonData = static_cast<spine::SkeletonData*>(data.nativeSkeletonData());
        auto* atlas        = static_cast<spine::Atlas*>(data.nativeAtlas());
        if (!skeletonData || !atlas)
            return;

        forEachSkin(skeletonData, [](spine::Skin* skin) { materializeSkin(skin); });

        // 纯 loader 仅 parse 期使用，换上正式渲染 loader 保持语义一致
        delete static_cast<spine::AttachmentLoader*>(data.nativeAttachmentLoader());
        auto* attachmentLoader = new (__FILE__, __LINE__) spine::AxmolAtlasAttachmentLoader(atlas);
        data.bindNative(MgSpineRuntime::Axmol, skeletonData, atlas, attachmentLoader);
    }

    void dispose(MgSkeletonData& data) const override
    {
        delete static_cast<spine::SkeletonData*>(data.nativeSkeletonData());
        delete static_cast<spine::AttachmentLoader*>(data.nativeAttachmentLoader());
        delete static_cast<spine::Atlas*>(data.nativeAtlas());
        data.clearNative();
    }

    bool replaceAtlas(MgSkeletonData& data, const std::vector<std::string>& atlasFiles) const override
    {
        auto* skeletonData = static_cast<spine::SkeletonData*>(data.nativeSkeletonData());
        if (!skeletonData)
        {
            MG_LOG_E("Spine: replaceAtlas on invalid skeleton data");
            return false;
        }
        spine::Atlas* atlas = createAtlas(atlasFiles, true);
        if (!atlas)
        {
            MG_LOG_E("Spine: replaceAtlas failed to build atlas");
            return false;
        }

        // 就地重指所有 skin 中的 attachment region；使用该数据的节点下一帧自动生效
        forEachSkin(skeletonData, [&](spine::Skin* skin) { relinkSkin(skin, atlas); });

        delete static_cast<spine::AttachmentLoader*>(data.nativeAttachmentLoader());
        delete static_cast<spine::Atlas*>(data.nativeAtlas());
        auto* attachmentLoader = new (__FILE__, __LINE__) spine::AxmolAtlasAttachmentLoader(atlas);
        data.bindNative(MgSpineRuntime::Axmol, skeletonData, atlas, attachmentLoader);
        return true;
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
