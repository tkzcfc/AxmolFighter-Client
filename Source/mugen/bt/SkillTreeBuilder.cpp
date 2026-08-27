#include "mugen/bt/SkillTreeBuilder.h"

#include "mugen/Components.h"
#include "mugen/bt/actions/AttackAction.h"
#include "mugen/bt/actions/RoleActions.h"
#include "mugen/bt/conditions/CondAttack.h"
#include "mugen/component/BehaviorTreeComponent.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/bt/BTSelector.h"
#include "mugen/core/bt/BTSequence.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/StdC.h"
#include "mugen/skill/SkillManager.h"

#include <unordered_set>

NS_MG_BEGIN

namespace SkillTreeBuilder
{

namespace
{

// 沿 nextSkill 收集连段 id，环则停
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

// Toward 序列：CondAttackToward + 若干 AttackAction
BTNode* buildToward(int32_t skillAttackId, int32_t towardIndex, const IntListRow& row)
{
    auto* seq = new BTSequence();
    seq->addCondition(new CondAttackToward(towardIndex));

    int32_t actionIndex = 0;
    for (int32_t aid : row.values)
    {
        if (aid <= 0)
            continue;
        seq->addChild(new AttackAction(aid, actionIndex++, skillAttackId));
    }
    if (seq->childCount() == 0)
    {
        delete seq;
        return nullptr;
    }
    return seq;
}

// Pipe：多朝向 Toward 的 Selector；条件 onEnter 扣费、onExit 只清朝向
BTNode* buildPipe(int32_t slot,
                  int32_t stepIndex,
                  int32_t skillAttackId,
                  int32_t pipeIndex,
                  int32_t modeIndex,
                  const SkillAttackConfig& skillAtk)
{
    auto* pipeSel = new BTSelector();
    pipeSel->addCondition(new CondAttackPipe(slot, stepIndex, pipeIndex, modeIndex));

    for (size_t j = 0; j < skillAtk.actionIds.size(); ++j)
    {
        const auto& row = skillAtk.actionIds[j];
        if (row.values.empty() || row.values.front() == -1)
            continue;
        if (BTNode* toward = buildToward(skillAttackId, static_cast<int32_t>(j + 1), row))
            pipeSel->addChild(toward);
    }

    if (pipeSel->childCount() == 0)
    {
        delete pipeSel;
        return nullptr;
    }
    return pipeSel;
}

// Step：按 cdCount 从高到低挂 Pipe
BTNode* buildStep(int32_t slot, int32_t stepIndex, int32_t skillAttackId, int32_t modeIndex)
{
    const auto* skillAtk = Config::getInstance()->getSkillAttackConfigById(skillAttackId);
    if (!skillAtk)
        return nullptr;

    auto* stepSel = new BTSelector();
    stepSel->addCondition(new CondAttackStep(slot, stepIndex));

    int32_t pipeMax = skillAtk->cdCount > 0 ? skillAtk->cdCount : 1;
    for (int32_t p = pipeMax; p >= 1; --p)
    {
        if (BTNode* pipe = buildPipe(slot, stepIndex, skillAttackId, p, modeIndex, *skillAtk))
            stepSel->addChild(pipe);
    }

    if (stepSel->childCount() == 0)
    {
        delete stepSel;
        return nullptr;
    }
    return stepSel;
}

// Mode：普通(0) / 爆气(1)，子节点为连段各 Step
BTNode* buildMode(int32_t slot, int32_t slotIndex, int32_t modeIndex, const std::vector<int32_t>& chain)
{
    auto* modeSel = new BTSelector();
    modeSel->addCondition(new CondAttackMode(slot, slotIndex, modeIndex));

    for (size_t step = 0; step < chain.size(); ++step)
    {
        if (BTNode* stepNode = buildStep(slot, static_cast<int32_t>(step), chain[step], modeIndex))
            modeSel->addChild(stepNode);
    }
    if (modeSel->childCount() == 0)
    {
        delete modeSel;
        return nullptr;
    }
    return modeSel;
}

// Slot：Sequence(CondAttackSlot → SlotIndex Selector)
BTNode* buildSlot(int32_t slot, const std::vector<int32_t>& chain)
{
    constexpr int32_t kSlotIndex = 1;

    auto* slotSeq = new BTSequence();
    slotSeq->addCondition(new CondAttackSlot(slot));

    auto* indexSel = new BTSelector();
    indexSel->addCondition(new CondAttackSlotIndex(slot, kSlotIndex));

    for (int32_t mode = 0; mode <= 1; ++mode)
    {
        if (BTNode* modeNode = buildMode(slot, kSlotIndex, mode, chain))
            indexSel->addChild(modeNode);
    }
    if (indexSel->childCount() == 0)
    {
        delete indexSel;
        delete slotSeq;
        return nullptr;
    }

    slotSeq->addChild(indexSel);
    return slotSeq;
}

// 突刺/闪避/爆气等专用槽：按技能根 id 灌一条 Slot
bool addSlotFromRoot(BTSelector* attackSel,
                     Config* config,
                     int32_t slot,
                     int32_t rootId,
                     std::unordered_set<int32_t>& filledSlots)
{
    if (!attackSel || rootId <= 0 || !filledSlots.insert(slot).second)
        return false;

    std::vector<int32_t> chain;
    collectSkillChain(config, rootId, chain);
    if (chain.empty())
        chain.push_back(rootId);

    if (BTNode* slotNode = buildSlot(slot, chain))
    {
        attackSel->addChild(slotNode);
        return true;
    }
    return false;
}

}  // namespace

void fill(Entity* entity)
{
    if (!entity)
        return;
    auto* bt       = BehaviorTreeComponent::of(entity);
    auto* skillBar = MG_GET_COMPONENT(entity, SkillBarComponent);
    auto* deck     = MG_GET_COMPONENT(entity, SkillDeckComponent);
    auto* mgr      = SkillManager::of(entity);
    if (!bt || !bt->attackSelector || !skillBar || !deck)
        return;

    auto* attackSel = static_cast<BTSelector*>(bt->attackSelector);
    if (!attackSel)
        return;

    attackSel->clearChildren();

    auto* config = Config::getInstance();
    std::unordered_set<int32_t> filledSlots;
    int32_t filled = 0;

    for (size_t i = 0; i < skillBar->skillSlots.size(); ++i)
    {
        const auto& barSlot = skillBar->skillSlots[i];
        if (i >= deck->slotSkillIndices.size())
            break;
        const auto& indices = deck->slotSkillIndices[i];
        if (indices.empty())
            continue;

        const int32_t firstDeck = indices.front();
        if (firstDeck < 0 || firstDeck >= static_cast<int32_t>(deck->skills.size()))
            continue;
        const int32_t rootId = deck->skills[static_cast<size_t>(firstDeck)].skillAttackId;

        std::vector<int32_t> chain;
        collectSkillChain(config, rootId, chain);
        if (chain.empty())
        {
            for (int32_t di : indices)
            {
                if (di >= 0 && di < static_cast<int32_t>(deck->skills.size()))
                    chain.push_back(deck->skills[static_cast<size_t>(di)].skillAttackId);
            }
        }

        if (BTNode* slotNode = buildSlot(barSlot.slotIndex, chain))
        {
            filledSlots.insert(barSlot.slotIndex);
            attackSel->addChild(slotNode);
            ++filled;
        }
    }

    if (mgr)
    {
        if (addSlotFromRoot(attackSel, config, static_cast<int32_t>(INPUT_SLOT_Z), mgr->thrustSkillAttackId,
                            filledSlots))
            ++filled;
        if (addSlotFromRoot(attackSel, config, static_cast<int32_t>(INPUT_SLOT_X), mgr->dodgeSkillAttackId,
                            filledSlots))
            ++filled;
        if (addSlotFromRoot(attackSel, config, static_cast<int32_t>(INPUT_SLOT_C), mgr->crazySkillAttackId,
                            filledSlots))
            ++filled;
    }

    if (filled == 0)
    {
        attackSel->addChild(new HoldAttackAction());
        MG_LOG_W("SkillTreeBuilder: no skill slots filled, HoldAttack placeholder");
    }
    else
    {
        MG_LOG_W("SkillTreeBuilder: filled {} attack slots", filled);
    }
}

void rebuild(Entity* entity)
{
    fill(entity);
}

}  // namespace SkillTreeBuilder

NS_MG_END
