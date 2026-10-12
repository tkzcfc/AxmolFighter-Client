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
    // 异步或同步加载结束时的通知回调。
    using SettledCallback = std::function<void(AvatarAccessory*)>;

    // 创建附着到父节点的挂件控制对象。
    AvatarAccessory(ax::Node* parent, AvatarAccessorySlot slot, int32_t resSpineId, const AvatarAccessoryRule& rule);
    // 释放挂件及其异步回调令牌。
    ~AvatarAccessory();

    AvatarAccessory(const AvatarAccessory&)            = delete;
    AvatarAccessory& operator=(const AvatarAccessory&) = delete;

    // 开始加载；完成（成功或失败）时回调。同步模式或命中缓存时在返回前回调
    void load(AvatarLoadMode mode, SettledCallback onSettled);

    // 返回挂件槽位、资源 id 和表现规则。
    AvatarAccessorySlot slot() const { return m_slot; }
    int32_t resSpineId() const { return m_resSpineId; }
    const AvatarAccessoryRule& rule() const { return m_rule; }
    // 加载已结束（成功或失败）
    bool isSettled() const { return m_settled; }
    // 返回已加载的 Spine 节点，未加载时为空。
    MgSkeletonAnimation* node() const { return m_node; }

    // 刷新显示：已浮现且（不限待机 或 身体正在待机）；从隐藏变为显示时重启循环动画
    void refresh(bool revealed, bool standing);

    // 推进挂件自身的循环动画时间。
    void step(float dtSec);

private:
    // 接收 Spine 资源加载结果。
    void onLoaded(MgSkeletonAnimation* node);
    // 从起点重新播放循环动画。
    void restartLoopAnim();

    // 父节点、槽位、资源、规则和完成回调。
    ax::Node* m_parent;
    AvatarAccessorySlot m_slot;
    int32_t m_resSpineId;
    AvatarAccessoryRule m_rule;
    SettledCallback m_onSettled;

    // 已创建的 Spine 节点及其显示状态。
    MgSkeletonAnimation* m_node = nullptr;
    bool m_settled              = false;
    bool m_shown                = false;

    // 保护异步回调生命周期的令牌。
    std::shared_ptr<char> m_token;
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
