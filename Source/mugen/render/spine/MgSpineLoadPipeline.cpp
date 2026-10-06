#include "MgSpineLoadPipeline.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/spine/MgSpineBackend.h"
#    include "mugen/render/spine/MgSpineUtils.h"

#    include <algorithm>
#    include <optional>

NS_MG_BEGIN

namespace
{

inline void runOnMain(std::function<void()> fn)
{
    ax::Director::getInstance()->getScheduler()->runOnAxmolThread(std::move(fn));
}

inline void runAsync(std::function<void()> fn)
{
    ax::Director::getInstance()->getJobSystem()->enqueue(std::move(fn));
}

// 读文件头部一小段做版本探测（主线程可承受的小读）；文件打不开返回 std::nullopt
std::optional<MgSpineRuntime> probeSpineRuntime(std::string_view skeletonFile)
{
#    if MG_SPINE_USE_3_4
    auto fullPath = ax::FileUtils::getInstance()->fullPathForFilename(skeletonFile);
    auto stream   = ax::FileUtils::getInstance()->openFileStream(fullPath, ax::IFileStream::Mode::READ);
    if (!stream)
        return std::nullopt;
    char buf[256];
    const int readBytes = stream->read(buf, sizeof(buf));
    if (readBytes <= 0)
        return std::nullopt;
    return runtimeFromSkeletonData(buf, static_cast<size_t>(readBytes));
#    else
    if (ax::FileUtils::getInstance()->isFileExist(skeletonFile))
        return MgSpineRuntime::Axmol;
    return std::nullopt;
#    endif
}

}  // namespace

void MgSpineLoadPipeline::start(std::string_view skeletonFile,
                                std::vector<std::string> atlasFiles,
                                float scale,
                                bool exclusive,
                                SkeletonCallback onDone)
{
    if (skeletonFile.empty())
    {
        onDone(nullptr);
        return;
    }

    const auto runtime = probeSpineRuntime(skeletonFile);
    if (!runtime.has_value())
    {
        MG_LOG_E("MgSpineLoadPipeline: skeleton '{}' not readable", skeletonFile);
        onDone(nullptr);
        return;
    }

    // 在 start() 内 new，才能既用私有构造，又正确初始化 enable_shared_from_this
    std::shared_ptr<MgSpineLoadPipeline> self(new MgSpineLoadPipeline());
    self->m_onSkeleton   = std::move(onDone);
    self->m_runtime      = *runtime;
    self->m_backend      = &MgSpineBackend::of(*runtime);
    self->m_exclusive    = exclusive;
    self->m_skeletonFile = std::string(skeletonFile);
    self->m_atlasFiles   = resolveAtlasFiles(skeletonFile, atlasFiles);
    self->run(scale);
}

void MgSpineLoadPipeline::loadAtlas(MgSpineRuntime runtime, std::vector<std::string> atlasFiles, AtlasCallback onDone)
{
    atlasFiles.erase(std::remove(atlasFiles.begin(), atlasFiles.end(), std::string()), atlasFiles.end());
    if (atlasFiles.empty())
    {
        onDone(nullptr);
        return;
    }

    std::shared_ptr<MgSpineLoadPipeline> self(new MgSpineLoadPipeline());
    self->m_onAtlas    = std::move(onDone);
    self->m_runtime    = runtime;
    self->m_backend    = &MgSpineBackend::of(runtime);
    self->m_atlasFiles = std::move(atlasFiles);
    self->run(0.0f);
}

MgSpineLoadPipeline::~MgSpineLoadPipeline() = default;

void MgSpineLoadPipeline::run(float scale)
{
    // —— 以下在主线程 ——
    auto self = shared_from_this();

    // 纹理解码任务
    prewarmTexturesOnMain(collectTexturePaths(m_atlasFiles), [self]() { self->notifyTexturesReady(); });

    // atlas 文本 / 骨架解析任务
    runAsync([self, scale]() { self->parseOnWorker(scale); });
}

void MgSpineLoadPipeline::parseOnWorker(float scale)
{
    auto self = shared_from_this();

    // 不建纹理（纯文本解析），纹理在主线程收尾时绑定
    if (void* native = m_backend->createAtlasHandle(m_atlasFiles))
        m_atlas = std::make_unique<MgAtlasHandle>(m_runtime, native);
    else
        MG_LOG_E("MgSpineLoadPipeline: failed to build atlas for '{}'",
                 m_skeletonFile.empty() ? m_atlasFiles.front() : m_skeletonFile);

    if (m_atlas && m_onSkeleton)
    {
        ax::Data skelData = ax::FileUtils::getInstance()->getDataFromFile(m_skeletonFile);
        if (skelData.isNull() || skelData.getSize() <= 0)
        {
            MG_LOG_E("MgSpineLoadPipeline: failed to read skeleton '{}'", m_skeletonFile);
        }
        else
        {
            // parseWithAtlas 成功接管 atlas，失败时内部释放
            m_data = m_backend->parseWithAtlas(skelData, m_atlas->release(), scale, m_skeletonFile);
            m_atlas.reset();
        }
    }

    runOnMain([self]() { self->notifyParseDone(); });
}

void MgSpineLoadPipeline::notifyTexturesReady()
{
    m_texturesReady = true;
    tryFinish();
}

void MgSpineLoadPipeline::notifyParseDone()
{
    m_parseDone = true;
    tryFinish();
}

void MgSpineLoadPipeline::tryFinish()
{
    if (m_finished || !m_parseDone || !m_texturesReady)
        return;
    m_finished = true;

    if (m_onSkeleton)
    {
        if (m_data)
        {
            // 先绑定纹理再材质化（AttachmentVertices 需要纹理）
            m_backend->bindAtlasTextures(m_data->nativeAtlas());
            m_backend->materialize(*m_data);
            m_data->m_exclusive = m_exclusive;
        }
        auto onDone = std::move(m_onSkeleton);
        onDone(std::move(m_data));
        return;
    }

    if (m_atlas)
        m_backend->bindAtlasTextures(m_atlas->native());
    auto onDone = std::move(m_onAtlas);
    onDone(std::move(m_atlas));
}

NS_MG_END

#endif
