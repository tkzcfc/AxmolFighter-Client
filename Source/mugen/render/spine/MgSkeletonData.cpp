#include "MgSkeletonData.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/spine/MgSpineBackend.h"
#    include "mugen/render/spine/MgSpineUtils.h"

NS_MG_BEGIN

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

MgSkeletonData* MgSkeletonData::load(const ax::Data& skelData,
                                     const std::vector<std::string>& atlasFiles,
                                     float scale,
                                     std::string_view skeletonFile)
{
    auto runtime = runtimeFromSkeletonData(skelData);
    return MgSpineBackend::of(runtime).load(skelData, atlasFiles, scale, skeletonFile);
}

MgSkeletonData* MgSkeletonData::loadFromFile(std::string_view skeletonFile,
                                             const std::vector<std::string>& atlasFiles,
                                             float scale)
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

    if (atlasFiles.empty() || atlasFiles.front().empty())
        return load(skelData, {atlasFromSpine(skeletonFile)}, scale, skeletonFile);
    return load(skelData, atlasFiles, scale, skeletonFile);
}

MgAnimation MgSkeletonData::findAnimation(const char* name) const
{
    return MgSpineBackend::of(m_runtime).findAnimation(*this, name);
}

bool MgSkeletonData::replaceAtlas(const std::vector<std::string>& atlasFiles)
{
    if (!m_skeletonData)
    {
        MG_LOG_E("Spine: replaceAtlas on invalid skeleton data");
        return false;
    }
    return MgSpineBackend::of(m_runtime).replaceAtlas(*this, atlasFiles);
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
