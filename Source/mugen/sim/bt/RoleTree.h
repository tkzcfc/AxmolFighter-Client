#pragma once
#include "mugen/core/bt/BTRunner.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/conf/GameDef.h"
#include "mugen/sim/input/PlayerCommand.h"
#include "mugen/sim/motion/Displacement.h"
#include "mugen/sim/attack/AttackState.h"
#include "HitActionState.h"
#include "mugen/sim/ai/ActionMove.h"

namespace mugen::sim
{
/// 行为树访问的战斗世界。
class World;
namespace behavior
{
/// 按世界模式选择的行为树模板。
enum class Template : uint8_t {
    /// 战斗角色模板。
    Role,
    /// 城镇角色模板。
    City
};
/// 非攻击叶子使用的动作类型。
enum class ActionType : uint8_t
{
    /// 无动作占位参数。
    None,
    /// 死亡动作。
    Death,
    /// 复活动作。
    GetUp,
    /// 起身动作。
    HitFloor,
    /// 倒地受击动作。
    HitDown,
    /// 击飞受击动作。
    HitUp,
    /// 受击切换动作。
    HitSwitch,
    /// 普通受击动作。
    Hit,
    /// 下落动作。
    Fall,
    /// 巡逻动作。
    Patrol,
    /// 追击动作。
    Chase,
    /// 警戒动作。
    Alert,
    /// 跑动动作。
    Run,
    /// 行走动作。
    Walk,
    /// 待机动作。
    Idle,
    /// 冲刺动作。
    Spurt,
    /// 拾取动作。
    Collect,
    /// 激活动作。
    Active,
    /// 传送动作。
    Transfer,
    /// 通用移动动作。
    Move,
    /// 准备待机动作。
    ReadyIdle,
    /// 准备移动动作。
    ReadyMove,
    /// 被挤开动作。
    Jostled,
    /// 跟随动作。
    Follow,
    /// 特殊待机动作。
    SpecIdle,
    /// 受控动作。
    Controlled,
    // 醒来动作。
    Wake,
    // 复活动作。
    Revive,
    // 销毁动作。
    Destroy
};
/// 注册到行为树的叶子类型 id。
enum Leaf : uint16_t {
    /// 尚未实现的叶子占位。
    kUnimplementedLeaf,
    /// 待机叶子。
    kIdleLeaf,
    /// 行走叶子。
    kWalkLeaf,
    /// 跑动叶子。
    kRunLeaf,
    /// 动作攻击叶子。
    kAttackLeaf,
    // 受击与角色生命周期叶子。
    kHitStateLeaf,
    // 巡逻、追击和警戒的公共移动叶子。
    kAiMoveLeaf,
    // 传送门推挤位移叶子。
    kJostledLeaf,
    // 路径点移动叶子。
    kFindingLeaf
};
/// 行为树分支的角色状态守卫。
struct Guard
{
    /// 进入分支所需的主状态。
    EntityRoleStatus status;
    /// 是否使用城镇待机规则。
    bool city = false;
};
/// 攻击分支选择动作时使用的条件类型。
enum class AttackConditionType : uint8_t {
    /// 技能槽位匹配。
    Slot,
    /// 技能地址完全匹配。
    SkillStep,
    /// 连段管线匹配。
    Pipe,
    /// 方向动作列匹配。
    VectorIndex,
    /// 按住阶段匹配。
    Press,
    /// 松开阶段匹配。
    Release
};
/// 攻击分支守卫携带的技能选择条件。
struct AttackCondition
{
    /// 要验证的条件类型。
    AttackConditionType type = AttackConditionType::Slot;
    /// 技能数组下标。
    int32_t skill            = -1;
    /// 需要匹配的管线编号。
    int32_t pipe             = 1;
    /// 需要匹配的方向动作列。
    int32_t toward           = 1;
};
/// 攻击叶子执行所需的动作与技能引用。
struct AttackAction
{
    /// 动作配置 id。
    int32_t actionId    = -1;
    /// 同方向动作列表中的序号。
    int32_t actionIndex = 1;
    /// 来源技能数组下标。
    int32_t skill       = -1;
};
/// 参与行为树定义缓存的技能静态信息。
struct SkillTreeSource
{
    /// 技能槽位下标。
    uint16_t slot                                                           = 0;
    /// 槽位内连段序号。
    int32_t index                                                           = 1;
    /// 连段模式序号。
    int32_t mode                                                            = 1;
    /// 模式内步骤序号。
    int32_t step                                                            = 1;
    /// 技能配置 id。
    int32_t configId                                                        = -1;
    /// 可用管线的最大次数。
    int32_t releaseMax                                                      = 1;
    friend auto operator<=>(const SkillTreeSource&, const SkillTreeSource&) = default;
    MG_REFLECT(SkillTreeSource, slot, index, mode, step, configId, releaseMax)
};
/// 区分不可变行为树拓扑的缓存键。
struct DefinitionKey
{
    /// 使用的行为树模板。
    Template tree = Template::Role;
    /// 决定攻击子树的已加载技能。
    std::vector<SkillTreeSource> skills;
    friend auto operator<=>(const DefinitionKey&, const DefinitionKey&) = default;
    MG_REFLECT(DefinitionKey, tree, skills)
};
/// 行为树节点条件和叶子共用的参数载荷。
using Param = std::variant<std::monostate, Guard, ActionType, AttackCondition, AttackAction>;
/// 普通动作叶子的运行时计时数据。
struct ActionState
{
    /// 叶子已运行的逻辑时间。
    Fixed fTime;
    /// 上一帧动作时间，用于检测动画循环。
    Fixed lastMotionTime;
    MG_REFLECT(ActionState, fTime, lastMotionTime)
};
// 推挤期间保留入场位移，避免后续状态参数修改影响收尾。
struct JostledState
{
    // 入场时的位移配置。
    int32_t m_displacementId = -1;
    // 制动事件只能完成一次。
    bool m_complete = false;
    MG_REFLECT(JostledState, m_displacementId, m_complete)
};
/// 各类叶子可序列化的运行状态。
using LeafState = std::variant<std::monostate, ActionState, attack::AttackRole, HitActionState, ai::ActionMove, JostledState>;
/// 角色行为树的不可变定义类型。
using Def       = BTDef<Param>;
/// 行为树回调执行时传递的上下文。
struct BTCtx
{
    /// 所属战斗世界。
    World& world;
    /// 正在运行行为树的实体。
    Entity entity;
    /// 当前分派的位移事件。
    DisplacementEvent event = DisplacementEvent::Brake;
    /// 由执行器在进入/更新/退出叶子和分派位移事件前写入的当前节点标识。
    int32_t m_node = -1;
};
/// 角色行为树使用的叶子和条件回调表。
using Registry = BTRegistry<BTCtx, Param, LeafState>;
/// 获取与缓存键对应的不可变行为树定义。
const Def& definition(const DefinitionKey& key);
/// 从实体已加载的技能构造行为树缓存键。
DefinitionKey definitionKey(World& world, Entity entity);
/// 获取角色行为树的回调注册表。
const Registry& registry();
/// 返回动作类型对应的动画名称。
const char* animationName(ActionType type);
/// 初始化并推进实体的行为树实例。
void update(World& world, Entity entity);
/// 消费实体本帧的控制命令。
void consumeControl(World& world, Entity entity);
/// 将位移事件路由到监听它的叶子。
bool onDisplacementEvent(World& world, Entity entity, DisplacementEvent event);
/// 通知普通动作叶子开始。
void behaviorStateStart(World& world, Entity entity, ActionType type);
/// 通知普通动作叶子结束。
void behaviorStateEnd(World& world, Entity entity, ActionType type);
}  // namespace behavior
/// 附着在角色实体上的行为树定义、实例和最新输入。
struct RoleTree
{
    /// 查找不可变拓扑的缓存键。
    behavior::DefinitionKey defKey;
    /// 可序列化的节点与叶子状态。
    BTInstance<behavior::LeafState> instance;
    /// 供移动和技能分支读取的最新命令。
    PlayerCommand control;
    MG_REFLECT(RoleTree, defKey, instance, control)
};
}  // namespace mugen::sim
