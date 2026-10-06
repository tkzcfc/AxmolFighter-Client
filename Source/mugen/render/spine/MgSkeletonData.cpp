#include "MgSkeletonData.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/spine/MgSpineBackend.h"
#    include "mugen/render/spine/MgSpineUtils.h"

NS_MG_BEGIN

MgAtlasHandle::~MgAtlasHandle()
{
    if (m_native)
        MgSpineBackend::of(m_runtime).disposeAtlasHandle(m_native);
}

MgSkeletonData::~MgSkeletonData()
{
    if (!m_skeletonData && !m_atlas && !m_attachmentLoader)
        return;
    MgSpineBackend::of(m_runtime).dispose(*this);
}

void MgSkeletonData::bindNative(MgSpineRuntime runtime, void* skeletonData, void* atlas, void* attachmentLoader)
{
    m_runtime          = runtime;
    m_skeletonData     = skeletonData;
    m_atlas            = atlas;
    m_attachmentLoader = attachmentLoader;
}

void MgSkeletonData::clearNative()
{
    m_skeletonData     = nullptr;
    m_atlas            = nullptr;
    m_attachmentLoader = nullptr;
}

MgSkeletonDataPtr MgSkeletonData::load(const ax::Data& skelData,
                                       const std::vector<std::string>& atlasFiles,
                                       float scale,
                                       std::string_view skeletonFile,
                                       bool exclusive)
{
    auto runtime = runtimeFromSkeletonData(skelData);
    auto data    = MgSpineBackend::of(runtime).load(skelData, atlasFiles, scale, skeletonFile);
    if (data)
        data->m_exclusive = exclusive;
    return data;
}

MgSkeletonDataPtr MgSkeletonData::loadFromFile(std::string_view skeletonFile,
                                               const std::vector<std::string>& atlasFiles,
                                               float scale,
                                               bool exclusive)
{
    if (skeletonFile.empty())
    {
        MG_LOG_E("Spine: empty skeleton path");
        return nullptr;
    }

    ax::Data skelData = ax::FileUtils::getInstance()->getDataFromFile(std::string(skeletonFile));
    if (skelData.isNull() || skelData.getSize() <= 0)
    {
        MG_LOG_E("Spine: failed to read skeleton '{}'", skeletonFile);
        return nullptr;
    }

    return load(skelData, resolveAtlasFiles(skeletonFile, atlasFiles), scale, skeletonFile, exclusive);
}

MgAnimation MgSkeletonData::findAnimation(const char* name) const
{
    return MgSpineBackend::of(m_runtime).findAnimation(*this, name);
}

bool MgSkeletonData::replaceAtlas(const std::vector<std::string>& atlasFiles)
{
    const auto& backend = MgSpineBackend::of(m_runtime);
    void* native        = backend.createAtlasHandle(atlasFiles);
    if (!native)
    {
        MG_LOG_E("Spine: replaceAtlas failed to build atlas");
        return false;
    }
    backend.bindAtlasTextures(native);
    return replaceAtlas(std::make_unique<MgAtlasHandle>(m_runtime, native));
}

bool MgSkeletonData::replaceAtlas(MgAtlasHandlePtr atlas)
{
    MG_ASSERT(m_exclusive && "replaceAtlas requires exclusive skeleton data");
    if (!m_exclusive || !m_skeletonData)
    {
        MG_LOG_E("Spine: replaceAtlas requires valid exclusive skeleton data");
        return false;
    }
    if (atlas->runtime() != m_runtime)
    {
        MG_LOG_E("Spine: replaceAtlas runtime mismatch");
        return false;
    }
    MgSpineBackend::of(m_runtime).replaceAtlas(*this, atlas->release());
    return true;
}

int MgSkeletonData::animationCount() const
{
    return MgSpineBackend::of(m_runtime).animationCount(*this);
}

MgAnimation MgSkeletonData::animationAt(int index) const
{
    return MgSpineBackend::of(m_runtime).animationAt(*this, index);
}

bool MgSkeletonData::hasSkin(const char* name) const
{
    return MgSpineBackend::of(m_runtime).hasSkin(*this, name);
}

NS_MG_END

#endif
