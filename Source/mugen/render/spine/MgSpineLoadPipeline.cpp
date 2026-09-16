#include "MgSpineLoadPipeline.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/spine/MgSpineBackend.h"
#    include "mugen/render/spine/MgSpineUtils.h"

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
std::optional<MgSpineRuntime> probeSpine34File(std::string_view skeletonFile)
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
    else
        return std::nullopt;
#    endif
}

}  // namespace

void MgSpineLoadPipeline::start(std::string_view skeletonFile,
                                std::vector<std::string> atlasFiles,
                                float scale,
                                std::function<void(MgSkeletonData*)> onDone)
{
    if (skeletonFile.empty())
    {
        if (onDone)
            onDone(nullptr);
        return;
    }

    // 在 start() 内 new，才能既用私有构造，又正确初始化 enable_shared_from_this
    std::shared_ptr<MgSpineLoadPipeline>(new MgSpineLoadPipeline(std::move(onDone)))
        ->run(skeletonFile, std::move(atlasFiles), scale);
}

MgSpineLoadPipeline::MgSpineLoadPipeline(std::function<void(MgSkeletonData*)> onDone) : m_onDone(std::move(onDone)) {}

MgSpineLoadPipeline::~MgSpineLoadPipeline()
{
    // parse 已接管则 atlas 在 data 里；未接管的裸 handle 由此释放
    if (m_atlasHandle && !m_data && m_backend)
        m_backend->disposeAtlasHandle(m_atlasHandle);
    m_atlasHandle = nullptr;
}

void MgSpineLoadPipeline::run(std::string_view skeletonFile, std::vector<std::string> atlasFiles, float scale)
{
    // —— 以下在主线程 ——
    auto self = shared_from_this();

    // 记录任务参数
    m_skeletonFile = std::string(skeletonFile);
    m_atlasFiles   = std::move(atlasFiles);

    // 探测 skeleton 版本；打不开/不可读说明文件不存在，直接失败不再加载
    const auto runtime = probeSpine34File(m_skeletonFile);
    if (!runtime.has_value())
    {
        MG_LOG_E("MgSpineLoadPipeline: skeleton '{}' not readable", m_skeletonFile);
        m_onDone(nullptr);
        return;
    }

    m_backend = &MgSpineBackend::of(*runtime);

    // 纹理解码任务
    prewarmTexturesOnMain(collectTexturePaths(m_atlasFiles), [self]() { self->notifyTexturesReady(); });

    // 动画数据解析任务
    runAsync([self, scale]() mutable { self->parseOnWorker(scale); });
}

void MgSpineLoadPipeline::parseOnWorker(float scale)
{
    auto self     = shared_from_this();
    m_atlasHandle = m_backend->createAtlasHandle(m_atlasFiles);

    MgSkeletonData* data = nullptr;
    if (m_atlasHandle)
    {
        ax::Data skelData = ax::FileUtils::getInstance()->getDataFromFile(m_skeletonFile);
        if (skelData.isNull() || skelData.getSize() <= 0)
            MG_LOG_E("MgSpineLoadPipeline: failed to read skeleton '{}'", m_skeletonFile);

        // 失败时 parseWithAtlas 内部会释放 atlasHandle
        data = m_backend->parseWithAtlas(skelData, m_atlasHandle, scale, m_skeletonFile);
        if (data)
            m_data = data;  // 立刻记下所有权，避免 RAII 把已接管的 atlas 当裸 handle 释放
        else
            m_atlasHandle = nullptr;
    }
    else
    {
        MG_LOG_E("MgSpineLoadPipeline: failed to build atlas for '{}'", m_skeletonFile);
    }

    runOnMain([self, data, scale]() mutable {
        // 注释掉回退逻辑,如果异步加载失败,直接返回 nullptr,不再回退同步加载
        // if (!data)
        //{
        //    // 罕见路径：后台 parse 失败（如版本探测偏差）回退同步加载（自包含完整数据，不再材质化）
        //    MG_LOG_W("MgSpineLoadPipeline: async parse failed '{}', fallback to sync load", skeletonFile);
        //    self->m_atlasHandle = nullptr;
        //    self->m_data        = MgSkeletonData::loadFromFile(skeletonFile, atlasFiles, scale);
        //}
        self->notifyParseDone();
    });
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
    if (m_data && m_atlasHandle)
    {
        // 先绑定纹理再材质化（AttachmentVertices 需要纹理）
        m_backend->bindAtlasTextures(m_atlasHandle);
        m_backend->materialize(*m_data);
        // 所有权已在 data 中，避免析构二次释放
        m_atlasHandle = nullptr;
    }
    m_onDone(m_data);
}

NS_MG_END

#endif
