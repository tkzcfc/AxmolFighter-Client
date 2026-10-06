#pragma once

#include "mugen/core/StdC.h"
#include "mugen/render/spine/MgSkeletonData.h"

#ifdef RUNTIME_IN_AXMOL

#    include <functional>
#    include <memory>
#    include <string>
#    include <vector>

NS_MG_BEGIN

class MgSpineBackend;

// 异步管线：工作线程解析（atlas 文本 + 骨架）与主线程贴图解码并行，二者都完成后在主线程收尾。
// 回调总在主线程调用；调用方自行保证回调时自身仍存活
class MgSpineLoadPipeline : public std::enable_shared_from_this<MgSpineLoadPipeline>
{
public:
    using SkeletonCallback = std::function<void(MgSkeletonDataPtr)>;
    using AtlasCallback    = std::function<void(MgAtlasHandlePtr)>;

    // 加载骨架数据；exclusive 决定数据归属（见 MgSkeletonData）
    static void start(std::string_view skeletonFile,
                      std::vector<std::string> atlasFiles,
                      float scale,
                      bool exclusive,
                      SkeletonCallback onDone);

    // 仅加载合并 atlas（换装用）：返回已绑定纹理的句柄，失败返回 nullptr
    static void loadAtlas(MgSpineRuntime runtime, std::vector<std::string> atlasFiles, AtlasCallback onDone);

    ~MgSpineLoadPipeline();

    MgSpineLoadPipeline(const MgSpineLoadPipeline&)            = delete;
    MgSpineLoadPipeline& operator=(const MgSpineLoadPipeline&) = delete;
    MgSpineLoadPipeline(MgSpineLoadPipeline&&)                 = delete;
    MgSpineLoadPipeline& operator=(MgSpineLoadPipeline&&)      = delete;

private:
    MgSpineLoadPipeline() = default;

    void run(float scale);
    void parseOnWorker(float scale);

    void notifyTexturesReady();
    void notifyParseDone();
    void tryFinish();

    SkeletonCallback m_onSkeleton;
    AtlasCallback m_onAtlas;
    const MgSpineBackend* m_backend = nullptr;
    MgSpineRuntime m_runtime        = MgSpineRuntime::Axmol;
    MgAtlasHandlePtr m_atlas;
    MgSkeletonDataPtr m_data;
    bool m_exclusive     = false;
    bool m_parseDone     = false;
    bool m_texturesReady = false;
    bool m_finished      = false;
    // 任务参数
    std::string m_skeletonFile;
    std::vector<std::string> m_atlasFiles;
};

NS_MG_END

#endif
