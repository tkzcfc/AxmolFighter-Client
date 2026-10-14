#pragma once
#include "BuffPool.h"
#include "BuffRules.h"

namespace mugen::sim
{
class World;
namespace buff
{
// Buff 持有者的临时视图，不存入组件、不拥有资源，生命周期限于单次调用。
class BuffHolder
{
public:
    // 构造指向指定世界实例中某个持有 Buff 池实体的视图。
    BuffHolder(World& world, Entity entity);
    // 查询尚在池中的指定配置实例。
    bool hasBuff(int32_t id) const;
    // 校验免疫、减益免疫、冷却和场景限定。
    bool buffCheck(const conf::BuffConfig& config, int32_t fightType) const;
    // 按配置标识强制销毁，空标识销毁整个池并取消对应退出定时器。
    void destroyBuff(std::optional<int32_t> id = {});
    // 按规则主键强制销毁，使用逆插入顺序执行退出钩子。
    void destroyBuffByRule(int32_t ruleId);
    // 推进实体池，显式将世界毫秒步长转换为配置秒数。
    void update();
    // 向指定实体池添加实例，返回成功创建或匹配的稳定序号。
    std::optional<uint64_t> addBuff(int32_t id, const BuffContext& context, int32_t fightType, bool notify = true);
    // 普通移除保留消失阶段，随后同步发送移除事件。
    void removeBuff(int32_t id, bool all = false, bool notify = true);
    // 加载角色自带效果，完成初始化后按配置目标分派。
    void loadBuffs(const std::vector<LoadEntry>& additional = {});
    // 施加器按配置选择目标并保留技能绑定与来源，指定目标仅作用于匹配阵营。
    void addBuffForApplicator(int32_t id, const BuffContext& context, int32_t fightType, bool notify = true);
    // 按实例插入顺序同步派发事件，回调后重新定位实例。
    void dispatchEvent(BFEvent event, const BuffContext& context, int32_t targetCamp);
    // 行为状态只转发给状态施加器规则，具体规则保持未移植分派。
    void dispatchBehaviorState(BFEvent event, const char* actionName);
    // 调用实例对应的规则钩子；进入条件由生命周期调用者先检查。
    void invokeRule(uint64_t serial, RuleHook hook, Fixed delta = {});
    // 重复添加时更新层数和配置，保留旧配置执行叠层回调的顺序。
    void resetInstance(uint64_t serial, const conf::BuffConfig& config);
    // 移除指定层数，退出钩子的次数遵循全部移除和强制移层分支。
    void removeRepeat(uint64_t serial, bool all = false, int32_t count = 1, bool force = false);
    // 创建或重建实例的出现与持续表现。
    void beginPresentation(uint64_t serial);
    // 出现定时器到期后请求持续表现，退出中的实例不会恢复显示。
    void continuePresentation(uint64_t serial);
    // 取消实例的出现阶段定时器。
    void cancelAppearance(uint64_t serial);
    // 请求实例退出，已解析的消失动作由逻辑定时器完成退出钩子。
    void exitInstance(uint64_t serial);
    // 定时器到期完成退出，序号避免误操作同配置的新实例。
    void finishExit(uint64_t serial);
    // 按稳定序号重新定位实例，不跨回调保留返回的指针。
    BuffInstance* findInstance(uint64_t serial);
    // 返回目标当前最高嘲讽权重对应的施加者。
    Entity sneerTarget() const;
    // 判断角色是否处于持续移动状态。
    bool isMoved() const;

private:
    // 强制删除不等待表现，回调后重新取得组件以规避扩容失效。
    void _destroySerial(uint64_t serial, bool byRule);

    World& m_world;
    Entity m_entity{};
};
}  // namespace buff
}  // namespace mugen::sim
