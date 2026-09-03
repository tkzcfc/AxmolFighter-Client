#pragma once

#include "Dialog.h"

namespace gameui
{

class MessageDialog : public Dialog
{
public:
    MessageDialog();

    virtual ~MessageDialog();

    void setMessage(std::string_view message,
                    std::function<void()> onConfirm = nullptr,
                    std::function<void()> onCancel  = nullptr);

public:
    static std::weak_ptr<MessageDialog> showGlobal(std::string_view message,
                                                   std::function<void()> onConfirm = nullptr,
                                                   std::function<void()> onCancel  = nullptr);

    static std::weak_ptr<MessageDialog> show(std::string_view message,
                                             std::function<void()> onConfirm = nullptr,
                                             std::function<void()> onCancel  = nullptr);

    static std::weak_ptr<MessageDialog> showNetErr(std::string_view error, std::function<void()> onRetry = nullptr);

protected:
    virtual std::vector<std::string> getPackages() const override { return {"UI/Common"}; }

    virtual GComponent* onCreateContent() override { return createCenteredComponent("Common", "MessageDialog"); }

    virtual void onCreate() override;

    void onClickCancelButton(EventContext* context);

    void onClickConfirmButton(EventContext* context);

private:
    std::function<void()> m_onConfirm = nullptr;
    std::function<void()> m_onCancel  = nullptr;
};

}  // namespace gameui
