#pragma once

#include "mugen/core/StdC.h"
#include "mugen/avatar/AvatarDesc.h"
#include "mugen/avatar/FashionResolver.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/render/AvatarAccessory.h"
#    include "mugen/avatar/render/SpineBody.h"

#    include <array>
#    include <memory>

NS_MG_BEGIN

// 角色渲染节点：一个身体骨架（SpineBody）+ 若干挂件（AvatarAccessory）。
//
// 时间模型：Avatar 持有当前 Motion 与绝对时间，时长取 Motion 时长（与逻辑侧 MotionPlayer 同源），
// step/seek 只更新时间并按绝对时间给身体摆 pose；身体未就绪时时间照常推进，就绪后对齐当前时间。
// 挂件的循环动画独立推进，不跟随动作时间。
class Avatar final : public ax::Node
{
public:
    // 按资源描述创建（战斗单位/特效用 kSyncShared）
    static Avatar* create(const AvatarDesc& desc, AvatarLoadMode mode);
    // 按外观创建（异步 + 实例私有数据，可 setAppearance 换装）
    static Avatar* create(const FashionAppearance& appearance);

    // 整体更新外观（仅 kAsyncExclusive）：与当前外观对比，身体 atlas/皮肤走换装，挂件按 id 增删；
    // 骨架变化（换角色）则整体重建
    bool setAppearance(const FashionAppearance& appearance);

    // 切换动作（entryId 为空从首段起播）
    void setMotion(const std::string& motionName, const std::string& entryId, bool loop);

    // 步进时间
    void step(int dtMs);

    // 跳跃到绝对时间点
    void seek(int timeMs);

    // 自动播放时步进
    void update(float delta) override;

    // 身体与初始挂件均已加载并浮现
    bool isReady() const { return m_revealed; }

    // 骨骼包围盒（Avatar 本地，脚底原点）；未就绪时为空
    ax::Rect localSkeletonBounds() const;

    // 当前动作时长
    int durationMs() const { return m_motion ? m_motion->durationMs() : 0; }

    // 非循环且已播完
    bool isFinished() const;

    // 当前时间
    MG_SYNTHESIZE_READONLY(int, m_timeMs, CurrentTimeMs)
    // 当前动作名
    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_motionName, CurrentMotionName)
    // 当前 entryId
    MG_SYNTHESIZE_READONLY_BY_REF(std::string, m_entryId, CurrentEntryId)
    // 是否循环
    MG_SYNTHESIZE_IS_READONLY(bool, m_loop, Loop)
    // 是否自动播放（UI 预览；战斗中保持 false，由 AvatarRenderSystem 驱动）
    MG_SYNTHESIZE_WRITEONLY(bool, m_autoPlay, AutoPlay)

private:
    Avatar();
    ~Avatar() override;

    bool initWithDesc(const AvatarDesc& desc, AvatarLoadMode mode);

    // 按 desc 重建身体与全部挂件（隐藏直到就绪）；.motion 不存在时失败且保持原状
    bool rebuild(const AvatarDesc& desc);
    // 按 m_desc 增删挂件（id 未变的保留）
    void syncAccessories();
    void createAccessory(AvatarAccessorySlot slot);

    void onBodyLoaded();
    void onAccessorySettled(AvatarAccessory* accessory);
    // 把武器/翅膀的数据嫁接到身体（仅 exclusive）
    void syncGrafts();
    void tryReveal();
    void refreshVisibility();

    void normalizeTime();
    void applyTime();

    AvatarDesc m_desc;
    AvatarLoadMode m_mode = AvatarLoadMode::kSyncShared;

    const MotionMap* m_motionMap = nullptr;
    const Motion* m_motion       = nullptr;

    std::unique_ptr<SpineBody> m_body;
    std::array<std::unique_ptr<AvatarAccessory>, static_cast<size_t>(AvatarAccessorySlot::kCount)> m_accessories;

    bool m_revealed       = false;
    float m_autoPlayCarry = 0.0f;
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
