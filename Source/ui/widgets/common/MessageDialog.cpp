#include "MessageDialog.h"
#include "AppContext.h"
#include "net/NetErr.h"

namespace gameui
{

using net::NET_ERR_CONNECTION_LOST;
using net::NET_ERR_DECODE_FAILED;
using net::NET_ERR_DISCONNECTED;
using net::NET_ERR_SERIALIZE_FAILED;
using net::NET_ERR_SERVER_INTERNAL_ERROR;
using net::NET_ERR_TIMEOUT;
using net::NET_ERR_UNKNOWN_SERVER_ERROR;
using net::NET_GATEWAY_ERR_DECODE_FAILED;
using net::NET_GATEWAY_ERR_INVALID_CMD;
using net::NET_GATEWAY_ERR_OTHER;
using net::NET_GATEWAY_ERR_SERVICE_NOT_BOUND;
using net::NET_GATEWAY_ERR_SERVICE_UNAVAILABLE;
using net::NET_GATEWAY_ERR_UNKNOWN_ROUTE;

MessageDialog::MessageDialog()
{
    m_options.draggable         = true;
    m_options.closeOnClickBg    = false;
    m_options.hasBackground     = true;
    m_options.backgroundOpacity = 0;
}

MessageDialog::~MessageDialog() {}

void MessageDialog::setMessage(std::string_view message,
                               std::function<void()> onConfirm,
                               std::function<void()> onCancel)
{
    m_onConfirm = onConfirm;
    m_onCancel  = onCancel;

    auto content = this->getContent();
    content->getController("c1")->setSelectedPage(onCancel == nullptr ? "single" : "double");

    auto textContent = content->getChild("textContent")->as<GLabel>();
    std::string textStr(message.data(), message.size());
    textContent->setText(textStr);

    // 如果文字渲染高度小于 textContent 高度,则将 title 位置设置到中间
    auto textRender        = textContent->getChild("title")->as<GTextField>();
    auto textHeight        = textRender->getHeight();
    auto textContentHeight = textContent->getHeight();

    if (textHeight < textContentHeight)
    {
        textRender->setX((textContentHeight - textHeight) * 0.5f);
    }
}

void MessageDialog::onCreate()
{
    auto confirmButtonCenter = this->getChild<GButton>("confirmButtonCenter");
    auto confirmButton       = this->getChild<GButton>("confirmButton");
    auto cancelButton        = this->getChild<GButton>("cancelButton");

    this->addClickListener(confirmButtonCenter, AX_CALLBACK_1(MessageDialog::onClickConfirmButton, this));
    this->addClickListener(confirmButton, AX_CALLBACK_1(MessageDialog::onClickConfirmButton, this));
    this->addClickListener(cancelButton, AX_CALLBACK_1(MessageDialog::onClickCancelButton, this));
}

void MessageDialog::onClickCancelButton(EventContext* context)
{
    if (m_onCancel)
    {
        auto onCancel = m_onCancel;
        m_onConfirm   = nullptr;
        m_onCancel    = nullptr;
        onCancel();
    }
    this->close();
}

void MessageDialog::onClickConfirmButton(EventContext* context)
{
    if (m_onConfirm)
    {
        auto onConfirm = m_onConfirm;
        m_onConfirm    = nullptr;
        m_onCancel     = nullptr;
        onConfirm();
    }
    this->close();
}

std::weak_ptr<MessageDialog> MessageDialog::showGlobal(std::string_view message,
                                                       std::function<void()> onConfirm,
                                                       std::function<void()> onCancel)
{
    auto uiManager = AppContext::get().uiManager();
    if (uiManager)
    {
        auto popupWeakPtr = uiManager->openWithOptions<MessageDialog>(
            UIOpenOptions{.layer = UI_OPTIONS_LAYER_MAXVALUE, .lifecycle = UIWidgetLifecycle::Independent});
        if (auto popup = popupWeakPtr.lock())
        {
            popup->setMessage(message, onConfirm, onCancel);
        }
        return popupWeakPtr;
    }
    return {};
}

std::weak_ptr<MessageDialog> MessageDialog::show(std::string_view message,
                                                 std::function<void()> onConfirm,
                                                 std::function<void()> onCancel)
{
    auto uiManager = AppContext::get().uiManager();
    if (uiManager)
    {
        auto popupWeakPtr =
            uiManager->openWithOptions<MessageDialog>(UIOpenOptions{.layer = UI_OPTIONS_LAYER_MAXVALUE - 1});
        if (auto popup = popupWeakPtr.lock())
        {
            popup->setMessage(message, onConfirm, onCancel);
        }
        return popupWeakPtr;
    }
    return {};
}

static std::tuple<std::string_view, bool> translateError(const std::string_view& error)
{
    if (error == NET_GATEWAY_ERR_INVALID_CMD)
    {
        return {"无效的命令", false};
    }
    else if (error == NET_GATEWAY_ERR_SERVICE_UNAVAILABLE)
    {
        return {"服务器维护中", false};
    }
    else if (error == NET_GATEWAY_ERR_SERVICE_NOT_BOUND)
    {
        return {"服务未绑定", false};
    }
    else if (error == NET_GATEWAY_ERR_UNKNOWN_ROUTE)
    {
        return {"未知路由", false};
    }
    else if (error == NET_GATEWAY_ERR_OTHER)
    {
        return {"网关错误", false};
    }
    else if (error == NET_GATEWAY_ERR_DECODE_FAILED)
    {
        return {"网关错误解码失败", false};
    }
    else if (error == NET_ERR_TIMEOUT)
    {
        return {"网络请求超时", true};
    }
    else if (error == NET_ERR_CONNECTION_LOST)
    {
        return {"网络连接断开", false};
    }
    else if (error == NET_ERR_DISCONNECTED)
    {
        return {"未连接到服务器", true};
    }
    else if (error == NET_ERR_SERIALIZE_FAILED)
    {
        return {"数据编码失败", false};
    }
    else if (error == NET_ERR_DECODE_FAILED)
    {
        return {"数据解码失败", false};
    }
    else if (error == NET_ERR_SERVER_INTERNAL_ERROR)
    {
        return {"服务器内部错误", false};
    }
    else if (error == NET_ERR_UNKNOWN_SERVER_ERROR)
    {
        return {"未知错误", false};
    }
    else
    {
        // 未知错误
        return {"网络请求失败", true};
    }
}

std::weak_ptr<MessageDialog> MessageDialog::showNetErr(std::string_view error, std::function<void()> onRetry)
{
    auto [message, canRetry] = translateError(error);

    if (canRetry && onRetry)
    {
        return show(fmt::format("{},是否重试?", message), onRetry, []() {});
    }
    else
    {
        return show(message);
    }
}

}  // namespace gameui
