#pragma once
#include "Components.h"
#include "Singletons.h"
#include "mugen/core/ecs/ECSManager.h"
#include "role/RoleUpdate.h"
#include "collision/Collision.h"
#include "hit/HitSeams.h"
#include <map>

namespace mugen::sim
{
namespace trace
{
class TraceSink;
}
// 仅用于非序列化生成序号表的句柄全序，避免依赖 Entity 的 operator<。
struct EntityValueLess
{
    bool operator()(Entity a, Entity b) const { return a.value < b.value; }
};
// 定义世界使用的组件与单例 ECS 存储类型。
using WorldECS = ECSManager<Components, Singletons>;
class World;
// 保存驱动提供的可执行策略，不属于可序列化的战斗状态。
struct WorldHooks
{
    // 定义实体组更新和移除回调的函数指针签名。
    using EntityHook = void (*)(World&, Entity);
    // 定义世界阶段回调的函数指针签名。
    using WorldHook = void (*)(World&);
    // 提供不执行实体逻辑的默认回调。
    static void noEntityUpdate(World&, Entity);
    // 提供不执行世界阶段逻辑的默认回调。
    static void noWorldUpdate(World&);
    // 按实体组顺序保存更新回调。
    std::array<EntityHook, 5> update{role::update, effect::EffectActor::update, room_entities::update, room_entities::update, room_entities::update};
    // 按实体组顺序保存移除回调。
    std::array<EntityHook, 5> remove{effect::EffectActor::removeOwner, effect::EffectActor::remove, noEntityUpdate, noEntityUpdate, noEntityUpdate};
    // 在实体组更新前执行相机阶段回调。
    WorldHook camera = noWorldUpdate;
    // 在实体组更新后执行场景阶段回调。
    WorldHook scene = noWorldUpdate;
    // 在场景阶段后执行碰撞阶段回调。
    WorldHook collision = collision_system::update;
    // 命中涉及的同步外部系统接缝，随世界克隆保留。
    HitHooks m_hit;
};
// 持有单场战斗全部可变状态并按固定顺序推进逻辑。
class World
{
public:
    // 使用驱动提供的回调策略创建独立战斗世界。
    explicit World(WorldHooks hooks = {}) : m_hooks(hooks) {}
    // 重置 ECS 并以房间配置初始化本场世界。
    void init(WorldSetup setup);
    // 校验并推进一帧确定性输入。
    void tick(const FrameInput& input);
    // 在碰撞完成后提交房间切换，保留玩家和可跨房间角色。
    bool changeRoom(int32_t roomId, FixedVec3 position, int32_t toward);
    // 将可序列化世界状态写入快照缓冲区。
    void serialize(ByteBuffer& buffer) const;
    // 仅在完整有效快照时替换当前世界状态。
    bool deserialize(ByteBuffer& buffer);
    // 返回包含 ECS 状态和驱动回调策略的独立副本。
    World clone() const;
    // 计算当前可序列化状态的确定性哈希。
    uint64_t hash() const;
    uint64_t hashSequence() const { return m_hashSequence; }
    const WorldSetup& setup() const { return m_setup; }
    const HitHooks& hitHooks() const { return m_hooks.m_hit; }
    WorldECS& ecs() { return m_ecs; }
    const WorldECS& ecs() const { return m_ecs; }
    Entity create(GroupKind kind);
    // 通过世界销毁分组实体，以执行对应的移除回调。
    void destroy(Entity entity);
    // 向表现层队列追加当前帧事件。
    void emit(Entity entity, PresentationPayload payload);
    // 挂载语义追踪输出，传入空指针即关闭追踪。
    void setTrace(trace::TraceSink* sink) { m_trace = sink; }
    // 返回当前追踪输出，未挂载时为空。
    trace::TraceSink* trace() const { return m_trace; }
    // 开关怪物 AI。不改变回放与快照格式。
    void setAiEnabled(bool enabled) { m_setup.m_aiEnabled = enabled; }
    // 返回实体创建时分配的稳定生成序号，未记录返回 0。
    uint64_t spawnSeq(Entity entity) const
    {
        const auto it = m_spawnSeq.find(entity);
        return it == m_spawnSeq.end() ? 0 : it->second;
    }

private:
    // 应用本帧系统、玩家与木偶输入。
    void applyInput(const FrameInput& input);
    // 更新指定实体组并处理逻辑删除。
    void updateGroup(GroupKind kind);
    // 保存初始化时传入的房间与地图配置。
    WorldSetup m_setup;
    // 保存本场战斗独占的实体、组件和单例状态。
    WorldECS m_ecs;
    // 保存不参与快照的驱动阶段回调。
    WorldHooks m_hooks;
    // 记录已完成帧数，故意不参与状态哈希。
    uint64_t m_hashSequence = 0;
    // 标识世界是否已经完成初始化。
    bool m_initialized = false;
    // 语义追踪输出，非拥有、不参与快照/哈希/克隆，为空时零开销。
    trace::TraceSink* m_trace = nullptr;
    // 实体到生成序号的映射，仅用于追踪中的稳定标识，不参与快照。
    std::map<Entity, uint64_t, EntityValueLess> m_spawnSeq;
    // 下一个待分配的生成序号。
    uint64_t m_nextSpawnSeq = 1;
};
}  // namespace mugen::sim
