#pragma once

#include "mugen/render/spine/MgAnimation.h"

#ifdef RUNTIME_IN_AXMOL

#    include <memory>

NS_MG_BEGIN

class MgSkeletonData;
using MgSkeletonDataPtr = std::shared_ptr<MgSkeletonData>;

// 合并 atlas 的原生句柄（RAII）：未被接管时析构释放
class MgAtlasHandle
{
public:
    MgAtlasHandle(MgSpineRuntime runtime, void* native) : m_runtime(runtime), m_native(native) {}
    ~MgAtlasHandle();

    MgAtlasHandle(const MgAtlasHandle&)            = delete;
    MgAtlasHandle& operator=(const MgAtlasHandle&) = delete;

    MgSpineRuntime runtime() const { return m_runtime; }
    void* native() const { return m_native; }

    // 交出所有权
    void* release()
    {
        void* native = m_native;
        m_native     = nullptr;
        return native;
    }

private:
    MgSpineRuntime m_runtime = MgSpineRuntime::Axmol;
    void* m_native           = nullptr;
};
using MgAtlasHandlePtr = std::unique_ptr<MgAtlasHandle>;

// 骨架数据（SkeletonData + 合并 atlas + attachment loader）
//
// 归属：
// - shared：来自 SpineSkeletonCache，多个节点共用，只读
// - exclusive：实例私有（换装身体），允许就地改写（replaceAtlas / 皮肤嫁接）
// 归属在创建时确定，不从 use_count 推断
class MgSkeletonData
{
public:
    MgSkeletonData() = default;
    ~MgSkeletonData();

    MgSkeletonData(const MgSkeletonData&)            = delete;
    MgSkeletonData& operator=(const MgSkeletonData&) = delete;

    static MgSkeletonDataPtr load(const ax::Data& skelData,
                                  const std::vector<std::string>& atlasFiles,
                                  float scale,
                                  std::string_view skeletonFile,
                                  bool exclusive);
    // atlasFiles 为空时使用骨架同目录同名 .atlas
    static MgSkeletonDataPtr loadFromFile(std::string_view skeletonFile,
                                          const std::vector<std::string>& atlasFiles,
                                          float scale,
                                          bool exclusive);

    MgSpineRuntime runtime() const { return m_runtime; }
    bool valid() const { return m_skeletonData != nullptr; }
    bool isExclusive() const { return m_exclusive; }

    // 运行时替换图集（换装）：所有 attachment 按同名 region 重指到新合并的 atlas。
    // 就地改写 SkeletonData，仅 exclusive 数据可调用
    bool replaceAtlas(const std::vector<std::string>& atlasFiles);
    // 同上，使用已绑定纹理的 atlas 句柄（见 MgSpineLoadPipeline::loadAtlas）；成功时接管句柄
    bool replaceAtlas(MgAtlasHandlePtr atlas);

    MgAnimation findAnimation(const char* name) const;
    int animationCount() const;
    MgAnimation animationAt(int index) const;
    bool hasSkin(const char* name) const;

    void bindNative(MgSpineRuntime runtime, void* skeletonData, void* atlas, void* attachmentLoader);
    void clearNative();
    void* nativeSkeletonData() const { return m_skeletonData; }
    void* nativeAtlas() const { return m_atlas; }
    void* nativeAttachmentLoader() const { return m_attachmentLoader; }

private:
    friend class MgSpineLoadPipeline;

    MgSpineRuntime m_runtime = MgSpineRuntime::Axmol;
    void* m_skeletonData     = nullptr;
    void* m_atlas            = nullptr;
    void* m_attachmentLoader = nullptr;
    bool m_exclusive         = false;
};

NS_MG_END

#endif
