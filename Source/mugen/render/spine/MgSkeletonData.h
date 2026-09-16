#pragma once

#include "mugen/render/spine/MgAnimation.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class MgSkeletonData
{
public:
    MgSkeletonData() = default;
    ~MgSkeletonData();

    MgSkeletonData(const MgSkeletonData&)            = delete;
    MgSkeletonData& operator=(const MgSkeletonData&) = delete;

    static MgSkeletonData* load(const ax::Data& skelData,
                                const std::vector<std::string>& atlasFiles,
                                float scale,
                                std::string_view skeletonFile);
    static MgSkeletonData* loadFromFile(std::string_view skeletonFile,
                                        const std::vector<std::string>& atlasFiles,
                                        float scale);

    MgSpineRuntime runtime() const { return m_runtime; }
    bool valid() const { return m_skeletonData != nullptr; }

    // 运行时替换图集（换装）：所有 attachment 按同名 region 重指到新合并的 atlas。
    // 会就地改写 SkeletonData，仅允许实例私有的数据调用（共享数据会影响所有使用者）。
    bool replaceAtlas(const std::vector<std::string>& atlasFiles);

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
    MgSpineRuntime m_runtime = MgSpineRuntime::Axmol;
    void* m_skeletonData     = nullptr;
    void* m_atlas            = nullptr;
    void* m_attachmentLoader = nullptr;
};

NS_MG_END

#endif
