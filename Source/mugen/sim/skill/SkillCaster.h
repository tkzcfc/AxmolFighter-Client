#pragma once
#include "Skill.h"

namespace mugen::sim
{
class World;
namespace skill
{
// 技能施放视图：包装世界实例与实体句柄，替代原先按 (World&, Entity) 传参的施放与技能池自由函数。
// 仅作为临时对象使用，不存入组件、不拥有资源、生命周期限于单次调用。
class SkillCaster
{
public:
    // 构造指向指定世界实例中某个实体的视图。
    SkillCaster(World& world, Entity entity);
    // 按显式装配创建或替换角色的技能池。
    void loadSkill(const SkillLoadout& loadout);
    // 按角色 AI 配置加载默认怪物技能池。
    void loadSkill();
    // 推进角色全部技能的冷却和释放状态。
    void update();
    // 推进角色暴走状态关联的技能逻辑。
    void updateCrazy();
    // 检查 AI 条件是否允许选择技能。
    bool checkAi(SkillAi& ai);
    // 检查 AI 组合条件是否满足。
    bool checkComposition(const SkillAi& ai) const;
    // 检查技能是否满足内部施放约束。
    bool isAllowCastInternal(int32_t index, bool autoCast = false);
    // 检查技能是否可以由当前输入或自动施放触发。
    bool isAllowCast(int32_t index, bool autoCast = false);
    // 预设指定槽位的待施放技能。
    bool presetSkill(std::string_view slot, bool autoCast = false);
    // 根据按钮按下或抬起处理技能输入。
    bool dealWithButton(ControlEvent event, bool up = false, bool autoCast = false);
    // 分发单个控制事件到技能输入处理。
    bool dealButtonEvent(ControlEvent event, bool up = false, bool autoCast = false);
    // 重置角色技能池的临时施放状态。
    void resetSkill();
    // 强制中断角色当前技能施放。
    void forcedInterrupt();
    // 推进当前技能到下一基础技能步骤。
    bool dealWithNextSkillBase();
    // 进入技能施放的开始阶段。
    void castBegan(int32_t index);
    // 处理施放管线开始时的状态变化。
    void dealWithCastPipeBegan(int32_t index);
    // 处理施放管线结束时的状态变化。
    void dealWithCastPipeEnded(int32_t index);
    // 中断角色正在进行的跑步技能。
    void interruptRun();

private:
    // 按配置条件编号验证自动施放前提。
    bool _checkCondition(const SkillAi& ai, SkillAiCondition condition) const;

    World& m_world;
    Entity m_entity{};
};
}  // namespace skill
}  // namespace mugen::sim
