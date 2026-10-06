#pragma once

#include "mugen/core/StdC.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/AvatarDesc.h"
#    include "mugen/avatar/data/MotionMap.h"
#    include "mugen/render/spine/MgSkeletonAnimation.h"

#    include <array>
#    include <functional>
#    include <memory>

NS_MG_BEGIN

// Avatar 的加载方式（决定身体骨架数据的归属）
enum class AvatarLoadMode
{
    // 同步 + 共享缓存数据：返回即就绪，不可换装/嫁接（战斗单位、特效、固定外观展示）
    kSyncShared,
    // 异步 + 实例私有数据：就绪后浮现，可换装/嫁接（UI 角色预览）
    kAsyncExclusive,
};

// 角色身体骨架：加载、按绝对时间摆 pose、换装（atlas/皮肤）与挂件嫁接。
// 不是节点：骨架节点挂在 Avatar 下，由 Avatar 持有本对象
class SpineBody
{
public:
    using LoadedCallback = std::function<void()>;

    SpineBody(ax::Node* parent, const AvatarDesc& desc, AvatarLoadMode mode);
    ~SpineBody();

    SpineBody(const SpineBody&)            = delete;
    SpineBody& operator=(const SpineBody&) = delete;

    // 开始加载；完成（成功或失败）时回调。同步模式在返回前回调
    void load(LoadedCallback callback);

    bool isReady() const { return m_node != nullptr; }
    bool isExclusive() const { return m_mode == AvatarLoadMode::kAsyncExclusive; }
    MgSkeletonAnimation* node() const { return m_node; }

    // 换装（仅 exclusive）：atlas 列表变化时异步加载新 atlas 后应用；多次调用最新优先；未就绪时暂存
    void setLook(const std::vector<std::string>& atlases, const std::string& skin);

    // 设置挂件嫁接（仅 exclusive）：donor 为空表示撤销；未就绪时暂存
    void setGraft(AvatarAccessorySlot slot, MgSkeletonDataPtr donor, const std::vector<std::string>* slotNames);

    // 动作切换后调用：下次 applyClip 强制重设轨道动画
    void resetClip();

    // 按绝对时间摆 pose（clip 内本地时间，超出 clip 时长冻末帧）
    void applyClip(size_t clipIndex, const MotionClip& clip, int localMs);

    void setVisible(bool visible);
    ax::Rect bounds() const;

#    if _DEBUG
    // 校验 motion 引用的 spine 动画存在且时长与 .box 一致
    void validateMotion(const Motion& motion) const;
#    endif

private:
    struct Look
    {
        std::vector<std::string> atlases;
        std::string skin;
    };

    struct Graft
    {
        MgSkeletonDataPtr desired;
        MgSkeletonDataPtr applied;
        const std::vector<std::string>* slotNames = nullptr;
    };

    void onLoaded(MgSkeletonAnimation* node);
    void syncLook();
    void applySkin(const std::string& skin);
    void syncGrafts();
    // 以当前轨道时间重新摆 pose（setup pose 被重置后调用）
    void applyPose();

    ax::Node* m_parent;
    AvatarLoadMode m_mode;
    std::string m_skeleton;
    float m_scale;
    LoadedCallback m_onLoaded;

    MgSkeletonAnimation* m_node = nullptr;

    // 期望的外观
    Look m_requested;
    // 当前应用的外观
    Look m_applied;
    // 期望的外观序列号（每次 setLook 递增）
    uint32_t m_lookSeq = 0;

    std::array<Graft, static_cast<size_t>(AvatarAccessorySlot::kCount)> m_grafts;

    size_t m_clipIndex = static_cast<size_t>(-1);
    bool m_hasTrack    = false;
    float m_trackSec   = 0.0f;

    // 异步回调的生命周期令牌
    std::shared_ptr<char> m_token;
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
