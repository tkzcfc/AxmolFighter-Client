#pragma once

#include "mugen/core/StdC.h"
#include "mugen/sim/World.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/render/Avatar.h"
#    include "mugen/render/battle/PetFollower.h"
#    include "mugen/render/LayerRuntimeLoader.h"
#    include "mugen/render/VirtualCamera.h"
#    include "mugen/render/spine/MgSkeletonAnimation.h"

#    include <array>
#    include <map>
#    include <memory>
#    include <optional>
#    include <random>
#    include <string>
#    include <vector>

NS_MG_BEGIN

class AvatarPresenter
{
public:
    explicit AvatarPresenter(ax::Node& root) : m_root(root) {}

    void present(sim::World& world, float alpha, float renderDelta);
    void setFollowEntity(Entity entity) { m_followEntity = entity; }
    void setPet(int32_t petId, int32_t spineId);
    void clearPet();
    // 为任意实体绑定/清除宠物（城镇远端玩家或本地玩家）。
    void setEntityPet(Entity entity, int32_t petId, int32_t spineId);
    void clearEntityPet(Entity entity);
    void clear();
    // Re-apply fashion on an already-created exclusive Avatar (town/battle local player).
    bool setEntityAppearance(uint32_t entityValue, const FashionAppearance& appearance);
    // 根据怒气变身事件刷新实体外观。
    void onPartnerRageChanged(sim::World& world, Entity entity, bool active);
    // 清空节点复用池（换图时调用，池内骨架按外观分组，跨图基本不会再命中）。
    void clearPool();
    // 应用受击亮度到实体显示节点；返回是否命中已有实体。
    bool setEntityIntensity(uint32_t entityValue, float intensity);

private:
    // 实体外观身份：回滚后同一句柄可能指向另一个实体，靠它判断是否需要重建显示节点。
    struct LookIdentity
    {
        // 所属实体组。
        sim::GroupKind kind = sim::GroupKind::Count;
        // 该组配置表中的行 id（角色 id、特效 id 等）。
        int32_t configId = 0;
        // 逻辑侧是否绑定了动作文件（决定走 Avatar 还是直接播放 Spine 动画）。
        bool hasMotionFile = false;

        bool operator==(const LookIdentity&) const = default;
    };

    // 单个实体的渲染状态：外观在首次出现时解析，身份变化（回滚后句柄复用）时重新解析。
    struct AvatarState
    {
        // 绑定动作文件的实体由 Avatar 按逻辑动作时间摆姿。
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
        // 插值用前后两帧变换：previous 为上一逻辑帧，current 为最新逻辑帧。
        sim::Transform previous;
        sim::Transform current;
        // 采样 current 时的逻辑帧序号。
        uint64_t lastFrame = 0;
        // 是否已建立 previous/current 采样对。
        bool hasPrevious = false;
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
    };

    // 池中等待复用的显示节点，avatar 与 skeleton 恰有其一非空。
    struct PooledNode
    {
        Avatar* avatar               = nullptr;
        MgSkeletonAnimation* skeleton = nullptr;
    };
    // 每个外观键最多缓存的节点数。
    static constexpr size_t kPoolMaxPerKey = 8;
    // 全部外观键合计最多缓存的节点数。
    static constexpr size_t kPoolMaxTotal = 64;

    // 返回实体当前的显示节点；未创建时为空。
    static ax::Node* displayNode(const AvatarState& state);
    // 读取实体在其配置表中的行 id，用于身份比对。
    static int32_t lookConfigId(sim::World& world, Entity entity, sim::GroupKind kind);
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
                                   const sim::MotionState& motion,
                                   bool transformed,
                                   bool force);
    // 返回配置层级对应的绘制层，首次使用时按层级值作为 z 序创建。
    ax::Node& tierLayer(int32_t tier);

    ax::Node& m_root;
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

class MapPresenter
{
public:
    // root 挂地图节点；worldLayer 挂角色等世界内容，镜头同时平移两者
    MapPresenter(ax::Node& root, ax::Node& worldLayer) : m_root(root), m_worldLayer(worldLayer) {}

    void load(const std::string& mapKey);
    void update(float delta);
    void addNode(ax::Node& node) { m_worldLayer.addChild(&node); }
    VirtualCamera* camera() const { return m_camera.get(); }

private:
    ax::Node& m_root;
    // 世界坐标内容层（角色、伤害数字、调试绘制、地图挂点）
    ax::Node& m_worldLayer;
    ax::ParallaxNode* m_map = nullptr;
    std::unique_ptr<VirtualCamera> m_camera;
    std::string m_mapKey;
};

class CameraDirector
{
public:
    explicit CameraDirector(MapPresenter& map) : m_map(map) {}

    void follow(sim::World& world, Entity entity);
    void consume(const sim::PresentationEvent& event);
    void update(float delta);

private:
    MapPresenter& m_map;
    float m_displaySlowScale = 1.0f;
};

class SoundPlayer
{
public:
    explicit SoundPlayer(uint32_t seed = 0x51A7u) : m_random(seed) {}

    void consume(const sim::PresentationEvent& event);
    void clear();

private:
    using ActionKey = std::pair<uint32_t, int32_t>;

    std::minstd_rand m_random;
    std::map<ActionKey, int> m_loopAudioByAction;
};

// 伤害飘字：位图字体标签池，数字外观与动画时序见 .cpp。
class HurtNumber
{
public:
    explicit HurtNumber(ax::Node& root) : m_root(root) {}
    // 消费伤害数值与闪避提示事件，创建飘字。
    void consume(const sim::PresentationEvent& event, sim::World& world);
    // 按渲染帧推进全部飘字动画。
    void update(float delta);
    // 摘除全部飘字与池内标签。
    void clear();

private:
    // 飘字使用的位图字体种类，同时是标签池下标。
    enum class Face : int32_t
    {
        // 玩家方打出的普通伤害。
        kNormal = 0,
        // 敌方打到玩家方的伤害。
        kHero,
        // 暴击伤害。
        kCrit,
        // 未命中/闪避文字。
        kWord,
        kCount
    };
    // 一条正在播放的飘字。
    struct Item
    {
        ax::Label* label = nullptr;
        Face face        = Face::kNormal;
        // 飘字锚点（世界坐标），动画在其上叠加上升位移。
        float x = 0.0f;
        float y = 0.0f;
        // 已执行的动画步数与当前步内累计毫秒。
        int step       = 0;
        float carryMs  = 0.0f;
        // 当前不透明度（0-255）。
        int alpha = 255;
    };

    // 在锚点创建一条飘字；text 为数字串，Face::kWord 时为文字字符。
    void spawn(Face face, const std::string& text, float x, float y);
    // 执行第 item.step 步动画。
    static void applyStep(Item& item);
    // 取一个同字体的空闲标签，没有则新建。
    ax::Label* acquireLabel(Face face);
    // 回收标签到池中。
    void recycle(Item& item);
    // 随机偏移，范围 [-30, 30] 的整数。
    float randomOffset();

    ax::Node& m_root;
    std::vector<Item> m_active;
    std::array<std::vector<ax::Label*>, static_cast<size_t>(Face::kCount)> m_pool;
    std::minstd_rand m_random{0x7A11u};
};

class DebugDraw
{
public:
    explicit DebugDraw(ax::Node& root);
    void present(sim::World& world);
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool enabled() const { return m_enabled; }

private:
    ax::DrawNode* m_node = nullptr;
    bool m_enabled       = false;
};

class WorldPresenter
{
public:
    explicit WorldPresenter(ax::Node& root);
    ~WorldPresenter();

    void loadMap(const std::string& mapKey);
    void present(sim::World& world, float interpolationAlpha, float renderDelta);
    // 消费回滚会话去重后的表现事件列表（联机路径），不做 seq 单调去重。
    void presentEvents(const std::vector<sim::PresentationEvent>& events,
                       sim::World& world,
                       float interpolationAlpha,
                       float renderDelta);
    void setFollowEntity(Entity entity);
    void setPet(int32_t petId);
    void setEntityPet(Entity entity, int32_t petId);
    void clearEntityPet(Entity entity);
    void addMapNode(ax::Node& node) { m_map.addNode(node); }
    DebugDraw& debugDraw() { return m_debugDraw; }

    // Update an existing Avatar's fashion (kAsyncExclusive only).
    bool setEntityAppearance(Entity entity, const FashionAppearance& appearance);
    // Find local player Role (!isAi + playerSlot) and apply session fashion/weapon.
    bool refreshLocalPlayerAppearance(sim::World& world);

private:
    // 逐条消费表现事件；dedupeBySeq 为真时按全局单调 seq 去重（离线路径）。
    void consumeEvents(const std::vector<sim::PresentationEvent>& events, sim::World& world, bool dedupeBySeq);

    // 世界内容层，必须先于其余成员构造
    ax::Node& m_worldLayer;
    AvatarPresenter m_avatars;
    MapPresenter m_map;
    CameraDirector m_camera;
    SoundPlayer m_sound;
    HurtNumber m_hurtNumbers;
    DebugDraw m_debugDraw;
    Entity m_followEntity{};
    uint64_t m_lastEventSequence = 0;
};

NS_MG_END

#endif
