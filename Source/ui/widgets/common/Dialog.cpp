#include "Dialog.h"
#include "ui/core/AudioManager.h"

namespace gameui
{
constexpr float SCALE_TIME    = 0.12f;
constexpr float VWETICAL_TIME = 0.1f;

Dialog::Dialog() {}

Dialog::~Dialog() {}

void Dialog::onStopAnimations()
{
    GTween::kill(m_content, TweenPropType::None, false);
}

void Dialog::doShowAnimation(std::function<void()> done)
{
    if (m_content == nullptr)
    {
        done();
        return;
    }

    AudioManager::getInstance()->playUISFX("ui://Common/ui_open_3");

    m_content->setScale(0.0f, 0.0f);
    GTween::toVec2(ax::Vec2(0.0f, 0.0f), ax::Vec2(1.0f, 0.3f), SCALE_TIME)
        ->setEase(EaseType::Linear)
        ->setTarget(m_content, TweenPropType::Scale)
        ->onComplete([done, this]() {
        GTween::toFloat(0.3f, 1.0f, VWETICAL_TIME)
            ->setEase(EaseType::Linear)
            ->setTarget(m_content, TweenPropType::ScaleY)
            ->onComplete([done]() { done(); });
    });
}

void Dialog::doHideAnimation(std::function<void()> done)
{
    if (m_content == nullptr)
    {
        done();
        return;
    }
    AudioManager::getInstance()->playUISFX("ui://Common/ui_close_3");

    GTween::toFloat(m_content->getScaleY(), 0.3f, VWETICAL_TIME)
        ->setEase(EaseType::Linear)
        ->setTarget(m_content, TweenPropType::ScaleY)
        ->onComplete([done, this]() {
        GTween::toVec2(ax::Vec2(m_content->getScaleX(), 0.3f), ax::Vec2(0.0f, 0.0f), SCALE_TIME)
            ->setEase(EaseType::Linear)
            ->setTarget(m_content, TweenPropType::Scale)
            ->onComplete([done]() { done(); });
    });
}

}  // namespace gameui
