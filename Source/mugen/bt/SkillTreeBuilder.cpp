#include "mugen/bt/SkillTreeBuilder.h"

#include "mugen/Components.h"
#include "mugen/bt/actions/AttackAction.h"
#include "mugen/bt/actions/RoleActions.h"
#include "mugen/bt/conditions/CondAttack.h"
#include "mugen/conf/Config.h"
#include "mugen/core/bt/BTSelector.h"
#include "mugen/core/bt/BTSequence.h"

#include <algorithm>
#include <unordered_set>

NS_MG_BEGIN

namespace SkillTreeBuilder
{

namespace
{

void collectSkillChain(Config* config, int32_t rootId, std::vector<int32_t>& chainOut)
{
    chainOut.clear();
    std::unordered_set<int32_t> seen;
    int32_t id = rootId;
    while (id > 0)
    {
        if (!seen.insert(id).second)
            break;
        const auto* atk = config->getSkillAttackConfigById(id);
        if (!atk)
            break;
        chainOut.push_back(id);
        id = atk->nextSkill > 0 ? atk->nextSkill : 0;
    }
}

std::unique_ptr<BTNode> buildToward(BehaviorTreeComponent* bt,
                                    int32_t skillAttackId,
                                    int32_t towardIndex,
                                    const IntListRow& row)
{
    auto seq           = std::make_unique<BTSequence>();
    seq->memorySlot    = bt->allocMemorySlot();
    seq->debugName     = "Toward";
    seq->addCondition(std::make_unique<CondAttackToward>(towardIndex));

    int32_t actionIndex = 0;
    for (int32_t aid : row.values)
    {
        if (aid <= 0)
            continue;
        seq->addChild(std::make_unique<AttackAction>(aid, actionIndex++, skillAttackId));
    }
    if (seq->childCount() == 0)
        return nullptr;
    return seq;
}

std::unique_ptr<BTNode> buildPipe(BehaviorTreeComponent* bt,
                                  int32_t skillAttackId,
                                  int32_t pipeIndex,
                                  const SkillAttackConfig& skillAtk)
{
    auto pipeSel           = std::make_unique<BTSelector>();
    pipeSel->memorySlot    = bt->allocMemorySlot();
    pipeSel->debugName     = "Pipe";
    pipeSel->addCondition(std::make_unique<CondAttackPipe>(pipeIndex));

    // towardIndex 1-based，对应 actionIds 行下标 0
    for (size_t j = 0; j < skillAtk.actionIds.size(); ++j)
    {
        const auto& row = skillAtk.actionIds[j];
        if (row.values.empty() || row.values.front() == -1)
            continue;
        if (auto toward = buildToward(bt, skillAttackId, static_cast<int32_t>(j + 1), row))
            pipeSel->addChild(std::move(toward));
    }

    // 兼容扁平 primaryActionIds
    if (pipeSel->childCount() == 0 && !skillAtk.primaryActionIds.empty())
    {
        IntListRow row;
        row.values = skillAtk.primaryActionIds;
        if (auto toward = buildToward(bt, skillAttackId, 1, row))
            pipeSel->addChild(std::move(toward));
    }

    if (pipeSel->childCount() == 0)
        return nullptr;
    return pipeSel;
}

std::unique_ptr<BTNode> buildStep(BehaviorTreeComponent* bt,
                                  int32_t slot,
                                  int32_t stepIndex,
                                  int32_t skillAttackId)
{
    const auto* skillAtk = Config::getInstance()->getSkillAttackConfigById(skillAttackId);
    if (!skillAtk)
        return nullptr;

    auto stepSel           = std::make_unique<BTSelector>();
    stepSel->memorySlot    = bt->allocMemorySlot();
    stepSel->debugName     = "Step";
    stepSel->addCondition(std::make_unique<CondAttackStep>(slot, stepIndex));

    int32_t pipeMax = skillAtk->cdCount > 0 ? skillAtk->cdCount : 1;
    // 倒序挂载 Max..1
    for (int32_t p = pipeMax; p >= 1; --p)
    {
        if (auto pipe = buildPipe(bt, skillAttackId, p, *skillAtk))
            stepSel->addChild(std::move(pipe));
    }

    if (stepSel->childCount() == 0)
        return nullptr;
    return stepSel;
}

std::unique_ptr<BTNode> buildSlot(BehaviorTreeComponent* bt,
                                  int32_t slot,
                                  const std::vector<int32_t>& chain)
{
    auto slotSel           = std::make_unique<BTSelector>();
    slotSel->memorySlot    = bt->allocMemorySlot();
    slotSel->debugName     = "Slot";
    slotSel->addCondition(std::make_unique<CondAttackSlot>(slot));

    for (size_t step = 0; step < chain.size(); ++step)
    {
        if (auto stepNode = buildStep(bt, slot, static_cast<int32_t>(step), chain[step]))
            slotSel->addChild(std::move(stepNode));
    }

    if (slotSel->childCount() == 0)
        return nullptr;
    return slotSel;
}

}  // namespace

void fill(Entity* entity)
{
    if (!entity)
        return;
    auto* bt       = MG_GET_COMPONENT(entity, BehaviorTreeComponent);
    auto* skillBar = MG_GET_COMPONENT(entity, SkillBarComponent);
    auto* deck     = MG_GET_COMPONENT(entity, SkillDeckComponent);
    if (!bt || !bt->attackSelector || !skillBar || !deck)
        return;

    auto* attackSel = static_cast<BTSelector*>(bt->attackSelector);
    if (!attackSel)
        return;

    attackSel->clearChildren();

    auto* config = Config::getInstance();
    int32_t filled = 0;

    for (size_t i = 0; i < skillBar->skillSlots.size(); ++i)
    {
        const auto& barSlot = skillBar->skillSlots[i];
        if (i >= deck->slotSkillIndices.size())
            break;
        const auto& indices = deck->slotSkillIndices[i];
        if (indices.empty())
            continue;

        // 链：用槽内第一个 deck 技能作根展开（与 ActorSpawner 同槽链一致）
        const int32_t firstDeck = indices.front();
        if (firstDeck < 0 || firstDeck >= static_cast<int32_t>(deck->skills.size()))
            continue;
        const int32_t rootId = deck->skills[static_cast<size_t>(firstDeck)].skillAttackId;

        std::vector<int32_t> chain;
        collectSkillChain(config, rootId, chain);
        if (chain.empty())
        {
            // 回退：按 deck 槽索引顺序
            for (int32_t di : indices)
            {
                if (di >= 0 && di < static_cast<int32_t>(deck->skills.size()))
                    chain.push_back(deck->skills[static_cast<size_t>(di)].skillAttackId);
            }
        }

        if (auto slotNode = buildSlot(bt, barSlot.slotIndex, chain))
        {
            attackSel->addChild(std::move(slotNode));
            ++filled;
        }
    }

    if (filled == 0)
    {
        // 无技能时保留占位，避免 Attack 条件通过却无子节点
        attackSel->addChild(std::make_unique<HoldAttackAction>());
        MG_LOG_W("SkillTreeBuilder: no skill slots filled, HoldAttack placeholder");
    }
    else
    {
        // 灌树后可能新增 memory 槽，扩容
        const int32_t slots = (std::max)(bt->nextMemorySlot, 1);
        if (static_cast<int32_t>(bt->selectorMemory.size()) < slots)
            bt->selectorMemory.resize(static_cast<size_t>(slots), static_cast<int8_t>(-1));
        MG_LOG_W("SkillTreeBuilder: filled {} attack slots", filled);
    }
}

void rebuild(Entity* entity)
{
    fill(entity);
}

}  // namespace SkillTreeBuilder

NS_MG_END
