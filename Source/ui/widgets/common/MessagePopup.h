#pragma once

#include "ui/core/UIWidget.h"

namespace gameui
{

class MessagePopup : public UIWidget
{
public:
    MessagePopup();

    virtual ~MessagePopup();

    void setMessage(std::string_view message,
                    std::function<void()> onConfirm = nullptr,
                    std::function<void()> onCancel  = nullptr);

public:
    static std::weak_ptr<MessagePopup> showGlobal(std::string_view message,
                                                  std::function<void()> onConfirm = nullptr,
                                                  std::function<void()> onCancel  = nullptr);

    static std::weak_ptr<MessagePopup> show(std::string_view message,
                                            std::function<void()> onConfirm = nullptr,
                                            std::function<void()> onCancel  = nullptr);

    static std::weak_ptr<MessagePopup> showNetErr(std::string_view error, std::function<void()> onRetry = nullptr);

protected:
    virtual std::vector<std::string> getPackages() const override { return {"UI/Common"}; }

    virtual GComponent* onCreateContent() override { return createCenteredComponent("Common", "MessagePopup"); }

    virtual void onCreate() override;

    void onClickCloseButton(EventContext* context);

    void onClickCancelButton(EventContext* context);

    void onClickConfirmButton(EventContext* context);

private:
    std::function<void()> m_onConfirm = nullptr;
    std::function<void()> m_onCancel  = nullptr;
};

}  // namespace gameui
