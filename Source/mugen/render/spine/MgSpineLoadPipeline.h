#pragma once

#include "mugen/core/StdC.h"

#ifdef RUNTIME_IN_AXMOL

#    include <functional>
#    include <memory>
#    include <string>
#    include <vector>

NS_MG_BEGIN

class MgSkeletonData;
class MgSpineBackend;

// 异步管线：parse 与纹理解码并行，二者都完成才收尾
class MgSpineLoadPipeline : public std::enable_shared_from_this<MgSpineLoadPipeline>
{
public:
    static void start(std::string_view skeletonFile,
                      std::vector<std::string> atlasFiles,
                      float scale,
                      std::function<void(MgSkeletonData*)> onDone);

    ~MgSpineLoadPipeline();

    MgSpineLoadPipeline(const MgSpineLoadPipeline&)            = delete;
    MgSpineLoadPipeline& operator=(const MgSpineLoadPipeline&) = delete;
    MgSpineLoadPipeline(MgSpineLoadPipeline&&)                 = delete;
    MgSpineLoadPipeline& operator=(MgSpineLoadPipeline&&)      = delete;

private:
    explicit MgSpineLoadPipeline(std::function<void(MgSkeletonData*)> onDone);

    void run(std::string_view skeletonFile, std::vector<std::string> atlasFiles, float scale);
    void parseOnWorker(float scale);

    void notifyTexturesReady();
    void notifyParseDone();
    void tryFinish();

    std::function<void(MgSkeletonData*)> m_onDone;
    const MgSpineBackend* m_backend = nullptr;
    void* m_atlasHandle             = nullptr;
    MgSkeletonData* m_data          = nullptr;
    bool m_parseDone                = false;
    bool m_texturesReady            = false;
    bool m_finished                 = false;
    // 任务参数
    std::string m_skeletonFile;
    std::vector<std::string> m_atlasFiles;
};

NS_MG_END

#endif
