#pragma once

#include "mugen/core/StdC.h"
#include "RenderLayer.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/spine/MgSkeletonAnimation.h"
#    include "mugen/avatar/data/MotionMap.h"
#    include "mugen/avatar/FashionSpine.h"

#    include <atomic>
#    include <memory>

NS_MG_BEGIN

class SpineLayer : public RenderLayer
{
public:
    // asyncLoad=true：异步装配 + 实例私有数据（可换装），立即返回空层，就绪自动浮现
    // asyncLoad=false：同步 + 共享缓存（返回即就绪，不可换装）
    static SpineLayer* create(const FashionSpineDesc& desc, bool asyncLoad);

    bool setMotion(const std::string& motionName, const std::string& entryId) override;
    void step(int dtMs) override;
    void seek(int timeMs) override;
    int durationMs() const override;

    int currentTimeMs() const override { return m_timeMs; }
    bool isReady() const override { return m_skeleton != nullptr; }

    bool setSkin(const std::string& skinName);

    // 换装（仅实例私有骨架数据可用）：异步预解码贴图后主线程应用；骨架未就绪则暂存；多次调用最新优先
    bool replaceAtlases(const std::vector<std::string>& atlasFiles, const std::string& skin = "");

    // 应用新的外观描述：身体图集/皮肤变化走 replaceAtlases；武器/翅膀/光环附件按 id 对比增删
    void applyFashionDesc(const FashionSpineDesc& desc);

    ax::Rect skeletonBoundingBox() const;

private:
    ~SpineLayer() override;

    bool initWithDesc(const FashionSpineDesc& desc, bool asyncLoad);
    bool initSkeleton(const FashionSpineDesc& desc, bool asyncLoad);
    // 骨架数据就绪后的装配（同步/异步共用）
    bool setupSkeleton(MgSkeletonData* data, bool owned);
    // 异步装配完成（主线程）
    void onSkeletonReady(MgSkeletonData* data);
    void applyTrackTime(int timeMs);

    // 换装最新优先：预解码完成后仅当仍是最后一次请求才应用
    void applyRequestedAtlases();

    // 附件（武器/翅膀/光环）：独立 spine 子节点，与身体骨架同一层
    void syncAttachments(const FashionSpineDesc& desc);
    MgSkeletonAnimation* createAttachment(int32_t resSpineId, int zOrder);
    void setAttachmentsVisible(bool visible);

    const MotionMap* m_motionMap    = nullptr;
    const Motion* m_motion          = nullptr;
    MgSkeletonAnimation* m_skeleton = nullptr;
    int m_timeMs                    = 0;
    size_t m_clipIndex              = static_cast<size_t>(-1);

    // 异步装配令牌（析构置 false）与状态
    std::shared_ptr<std::atomic<bool>> m_asyncAlive;
    bool m_skeletonLoading = false;

    // 换装状态：最近一次请求 + 序号（最新优先）
    std::vector<std::string> m_requestedAtlases;
    std::string m_requestedSkin;
    bool m_swapDirty   = false;
    uint32_t m_swapSeq = 0;

    // 骨架未就绪时的动作暂存
    std::string m_pendingMotionName;
    std::string m_pendingMotionEntry;
    bool m_hasPendingMotion = false;
    // 骨架未就绪时收到的绝对 seek 目标（-1 无；战斗 sync 切动作的主路径）
    int m_pendingSeekMs = -1;

    // 附件节点（武器/翅膀/光环）
    MgSkeletonAnimation* m_weapon = nullptr;
    MgSkeletonAnimation* m_wing   = nullptr;
    MgSkeletonAnimation* m_halo   = nullptr;
    int32_t m_weaponSpineId       = 0;
    int32_t m_wingSpineId         = 0;
    int32_t m_ringSpineId         = 0;
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
