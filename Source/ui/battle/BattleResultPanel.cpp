#include "ui/battle/BattleResultPanel.h"

#include "fmt/format.h"

#include <string>

namespace gameui
{

BattleResultPanel::BattleResultPanel(DungeonBattleResult result,
                                     std::function<void()> onReturn,
                                     std::function<void()> onRetry)
    : m_result(result), m_onReturn(std::move(onReturn)), m_onRetry(std::move(onRetry))
{}

void BattleResultPanel::onCreate()
{
    getContent()->getController("result")->setSelectedPage(m_result.win ? "win" : "lose");

    const std::string copyText =
        m_result.copyId > 0 ? fmt::format("副本 {}", m_result.copyId) : fmt::format("房间 {}", m_result.roomId);
    getChild<GTextField>("txtCopy")->setText(copyText);

    const int32_t total   = m_result.timeSec > 0 ? m_result.timeSec : 0;
    const int32_t minutes = total / 60;
    const int32_t seconds = total % 60;
    getChild<GTextField>("txtTime")->setText(fmt::format("{:02}:{:02}", minutes, seconds));
    getChild<GTextField>("txtCombo")->setText(std::to_string(m_result.maxCombo));
    getChild<GTextField>("txtEvaluate")->setText(std::to_string(m_result.maxEvaluate));

    addClickListener(getChild<GButton>("btnReturn"), AX_CALLBACK_1(BattleResultPanel::onClickReturn, this));
    addClickListener(getChild<GButton>("btnRetry"), AX_CALLBACK_1(BattleResultPanel::onClickRetry, this));
}

void BattleResultPanel::onShow()
{
    if (auto* transition = m_content->getTransition("fadeIn"))
        transition->play();
}

void BattleResultPanel::onStopAnimations()
{
    if (m_content)
        if (auto* transition = m_content->getTransition("fadeIn"))
            transition->stop();
}

void BattleResultPanel::onClickReturn(EventContext* /*context*/)
{
    if (!m_onReturn)
        return;
    auto callback = std::move(m_onReturn);
    m_onRetry     = nullptr;
    callback();
}

void BattleResultPanel::onClickRetry(EventContext* /*context*/)
{
    if (!m_onRetry)
        return;
    auto callback = std::move(m_onRetry);
    m_onReturn    = nullptr;
    callback();
}

}  // namespace gameui
