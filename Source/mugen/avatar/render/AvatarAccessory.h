#pragma once

#include "mugen/core/StdC.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/AvatarAccessoryRules.h"
#    include "mugen/avatar/render/SpineBody.h"

#    include <functional>
#    include <memory>

NS_MG_BEGIN

// 挂件：独立 spine 节点（武器/翅膀/光环），数据总是来自共享缓存。
// 不是节点：spine 节点挂在 Avatar 下，由 Avatar 持有本对象
class AvatarAccessory
{
public:
    using SettledCallback = std::function<void(AvatarAccessory*)>;

    AvatarAccessory(ax::Node* parent, AvatarAccessorySlot slot, int32_t resSpineId, const AvatarAccessoryRule& rule);
    ~AvatarAccessory();

    AvatarAccessory(const AvatarAccessory&)            = delete;
    AvatarAccessory& operator=(const AvatarAccessory&) = delete;

    // 开始加载；完成（成功或失败）时回调。同步模式或命中缓存时在返回前回调
    void load(AvatarLoadMode mode, SettledCallback onSettled);

    AvatarAccessorySlot slot() const { return m_slot; }
    int32_t resSpineId() const { return m_resSpineId; }
    const AvatarAccessoryRule& rule() const { return m_rule; }
    // 加载已结束（成功或失败）
    bool isSettled() const { return m_settled; }
    MgSkeletonAnimation* node() const { return m_node; }

    // 刷新显示：已浮现且（不限待机 或 身体正在待机）；从隐藏变为显示时重启循环动画
    void refresh(bool revealed, bool standing);

    void step(float dtSec);

private:
    void onLoaded(MgSkeletonAnimation* node);
    void restartLoopAnim();

    ax::Node* m_parent;
    AvatarAccessorySlot m_slot;
    int32_t m_resSpineId;
    AvatarAccessoryRule m_rule;
    SettledCallback m_onSettled;

    MgSkeletonAnimation* m_node = nullptr;
    bool m_settled              = false;
    bool m_shown                = false;

    std::shared_ptr<char> m_token;
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
