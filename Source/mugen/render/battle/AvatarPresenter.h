#pragma once

#include "mugen/core/StdC.h"
#include "mugen/sim_legacy/World.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/render/Avatar.h"
#    include "mugen/render/battle/PetFollower.h"
#    include "mugen/render/battle/PoseTrack.h"
#    include "mugen/render/spine/MgSkeletonAnimation.h"

#    include <map>
#    include <memory>
#    include <optional>
#    include <string>

NS_MG_BEGIN

class AvatarPresenter
{
public:
    void present(sim_legacy::World& world, float alpha, float renderDelta);
    void setFollowEntity(Entity entity) { m_followEntity = entity; }
    void setPet(int32_t petId, int32_t spineId);
    void clearPet();
    // 回滚把实体当前帧的模拟位置改写了 shift（新 - 旧，渲染单位）：平移其插值轨迹并平滑消化这次跳变。
    void applyCorrection(Entity entity, const VisualPos& shift);
    // 为任意实体绑定/清除宠物（城镇远端玩家或本地玩家）。
    void setEntityPet(Entity entity, int32_t petId, int32_t spineId);
    void clearEntityPet(Entity entity);
    void clear();
    // 地图 entity 层：角色/特效/宠物的显示节点与 tier 子层挂在此节点下。
    void setEntityLayer(ax::Node& layer);
    // Re-apply fashion on an already-created exclusive Avatar (town/battle local player).
    bool setEntityAppearance(uint32_t entityValue, const FashionAppearance& appearance);
    // 根据怒气变身事件刷新实体外观。
    void onPartnerRageChanged(sim_legacy::World& world, Entity entity, bool active);
    // 清空节点复用池（换图时调用，池内骨架按外观分组，跨图基本不会再命中）。
    void clearPool();
    // 应用受击亮度到实体显示节点；返回是否命中已有实体。
    bool setEntityIntensity(uint32_t entityValue, float intensity);

private:
    // 实体外观身份：回滚后同一句柄可能指向另一个实体，靠它判断是否需要重建显示节点。
    struct LookIdentity
    {
        // 所属实体组。
        sim_legacy::GroupKind kind = sim_legacy::GroupKind::Count;
        // 该组配置表中的行 id（角色 id、特效 id 等）。
        int32_t configId = 0;
        // 逻辑侧是否绑定了动作文件（决定走 Avatar 还是直接播放 Spine 动画）。
        bool hasMotionFile = false;

        bool operator==(const LookIdentity&) const = default;
    };

    // 单个实体的渲染状态：外观在首次出现时解析，身份变化（回滚后句柄复用）时重新解析。
    struct AvatarState
    {
        // 绑定动作文件的实体由 Avatar 按渲染帧推进、必要时 seek。
        Avatar* avatar = nullptr;
        // 逻辑侧未绑定动作文件的实体（传送门）直接按动作名播放 Spine 动画。
        MgSkeletonAnimation* skeleton = nullptr;
        // 外观已解析；无 Spine 资源的实体两者均为空，身份不变时不再重试。
        bool resolved = false;
        // 已解析外观对应的实体身份。
        LookIdentity identity;
        // 显示节点的复用池键；空串表示节点为实例私有（时装角色），销毁时不入池。
        std::string poolKey;
        // 配置的 Spine 相对偏移（x 随水平朝向、y 向上、z 随纵深朝向）。
        std::vector<int32_t> spineOffset;
        // 位置插值轨迹：previous 为上一逻辑帧，current 为最新逻辑帧，并带回滚纠正偏移。
        PoseTrack track;
        // 最新逻辑帧的水平朝向与纵深朝向（±1）。
        float towardX = 1.0f;
        float towardZ = 1.0f;
        // 是否使用时装外观 Avatar（可热切换骨架与图集）。
        bool fashionAvatar = false;
        // 角色外观是否来自本地会话（使用会话时装与武器）。
        bool useSessionFashion = false;
        // 角色配置 id（用于重建/重算外观）。
        int32_t roleId = 0;
        // 显式下发的外观（远端玩家换装），优先于会话/默认外观。
        std::optional<FashionAppearance> m_overrideAppearance;
        // 最近一次已应用到 Avatar 的变身状态。
        bool partnerRageTransformed = false;
        // 是否已记录过 partnerRageTransformed。
        bool partnerRageStateKnown = false;
        std::string motionName;
        std::string entryId;
        // 上一逻辑帧采样到的 sim 动作时间（毫秒）。
        int lastSimTimeMs = 0;
        // 上一采样对应的逻辑帧号。
        uint64_t lastLogicFrame = 0;
        // 是否已采样过 sim 动作时间。
        bool hasSimTime = false;
        // 最近一个逻辑帧 sim 动作时间是否在推进（顿帧/暂停时为 false）。
        bool simMoving = false;
    };

    // 池中等待复用的显示节点，avatar 与 skeleton 恰有其一非空。
    struct PooledNode
    {
        Avatar* avatar                = nullptr;
        MgSkeletonAnimation* skeleton = nullptr;
    };
    // 每个外观键最多缓存的节点数。
    static constexpr size_t kPoolMaxPerKey = 8;
    // 全部外观键合计最多缓存的节点数。
    static constexpr size_t kPoolMaxTotal = 64;

    // 返回实体当前的显示节点；未创建时为空。
    static ax::Node* displayNode(const AvatarState& state);
    // 读取实体在其配置表中的行 id，用于身份比对。
    static int32_t lookConfigId(sim_legacy::World& world, Entity entity, sim_legacy::GroupKind kind);
    // 从复用池取出同外观键的节点并复位，池空返回 false。
    bool acquirePooled(AvatarState& state);
    // 摘下实体的显示节点：可复用的入池，否则直接销毁。
    void releaseNode(AvatarState& state);
    // 摘下实体显示节点、清理其宠物并把渲染状态重置为未解析。
    void resetState(uint32_t entityValue, AvatarState& state);
    // 渲染插值采样姿态。
    struct RenderPose
    {
        float x       = 0.0f;
        float y       = 0.0f;
        float z       = 0.0f;
        float towardX = 1.0f;
        float towardZ = 1.0f;
    };
    // 用 previous/current 与 alpha 计算显示姿态。
    static RenderPose interpolatePose(const AvatarState& state, float alpha);
    // 读取实体当前插值姿态，失败时返回 false。
    bool samplePose(uint32_t entityValue, float alpha, RenderPose* out) const;
    // 按缓存角色信息构造当前应应用的外观。
    static FashionAppearance buildRoleAppearance(const AvatarState& state, bool transformed);
    // 刷新角色怒气变身外观并重放当前动作时间。
    bool syncPartnerRageAppearance(uint32_t entityValue,
                                   const sim_legacy::MotionState& motion,
                                   bool transformed,
                                   bool force);
    // 驱动 Avatar 时间：平时按渲染帧推进，跳转时 seek。
    void driveAvatarTime(AvatarState& state, const sim_legacy::MotionState& motion, float renderDelta,
                         const sim_legacy::WorldClock& clock, bool motionChanged);
    // 返回配置层级对应的绘制层，首次使用时按层级值作为 z 序创建。
    ax::Node& tierLayer(int32_t tier);

    ax::Node* m_entityLayer = nullptr;
    std::map<uint32_t, AvatarState> m_avatars;
    // 外观键 → 已摘下待复用的节点（池内节点由本类持有一份引用）。
    std::map<std::string, std::vector<PooledNode>> m_pool;
    size_t m_poolCount = 0;
    // 实体 → 宠物跟随器（一个实体可有自己的宠物，城镇远端玩家与本地玩家都走这里）。
    std::map<uint32_t, std::unique_ptr<PetFollower>> m_petFollowers;
    Entity m_followEntity{};
    // 配置层级 → 绘制层；层间按层级值排序，层内按纵深排序。
    std::map<int32_t, ax::Node*> m_tierLayers;
};

NS_MG_END

#endif
