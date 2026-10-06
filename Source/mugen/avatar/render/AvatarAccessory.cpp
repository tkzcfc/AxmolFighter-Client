#include "AvatarAccessory.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

AvatarAccessory::AvatarAccessory(ax::Node* parent,
                                 AvatarAccessorySlot slot,
                                 int32_t resSpineId,
                                 const AvatarAccessoryRule& rule)
    : m_parent(parent), m_slot(slot), m_resSpineId(resSpineId), m_rule(rule), m_token(std::make_shared<char>(0))
{}

AvatarAccessory::~AvatarAccessory()
{
    m_token.reset();
    if (m_node)
        m_node->removeFromParent();
}

void AvatarAccessory::load(AvatarLoadMode mode, SettledCallback onSettled)
{
    m_onSettled = std::move(onSettled);

    if (mode == AvatarLoadMode::kSyncShared)
    {
        onLoaded(MgSkeletonAnimation::createFromCache(m_resSpineId));
        return;
    }

    std::weak_ptr<char> token = m_token;
    MgSkeletonAnimation::createFromCacheAsync(m_resSpineId, [this, token](MgSkeletonAnimation* node) {
        if (!token.expired())
            onLoaded(node);
    });
}

void AvatarAccessory::onLoaded(MgSkeletonAnimation* node)
{
    m_settled = true;
    if (!node)
    {
        MG_LOG_W("AvatarAccessory: spine load failed {}", m_resSpineId);
        m_onSettled(this);
        return;
    }

    m_node = node;
    m_node->setAutoUpdate(false);
    m_node->setUpdateOnlyIfVisible(false);
    m_node->setTimeScale(1.0f);
    m_node->setVisible(false);
    m_parent->addChild(m_node, m_rule.zOrder);
    restartLoopAnim();

    m_onSettled(this);
}

void AvatarAccessory::restartLoopAnim()
{
    if (m_rule.loopAnim[0] && m_node->findAnimation(m_rule.loopAnim))
        m_node->setAnimation(0, m_rule.loopAnim, true);
}

void AvatarAccessory::refresh(bool revealed, bool standing)
{
    if (!m_node)
        return;
    const bool shown = revealed && (!m_rule.standOnly || standing);
    if (shown && !m_shown && m_rule.standOnly)
        restartLoopAnim();
    m_shown = shown;
    m_node->setVisible(shown);
}

void AvatarAccessory::step(float dtSec)
{
    if (m_node)
        m_node->update(dtSec);
}

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
