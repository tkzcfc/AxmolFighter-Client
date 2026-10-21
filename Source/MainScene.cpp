/****************************************************************************
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 Copyright (c) 2019-present Axmol Engine contributors (see AUTHORS.md).

 https://axmol.dev/

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "MainScene.h"
#include "ImGui/ImGuiPresenter.h"
#include "AppContext.h"
#include "ui/core/ScreenManager.h"
#include "ui/screens/GameScreen.h"
#include "ui/screens/LaunchScreen.h"
#include "LaunchOptions.h"

#include <charconv>

const std::string_view IMGUI_LOOP_ID = "#MainScene";

using namespace ax;

MainScene::MainScene() {}

MainScene::~MainScene() {}

bool MainScene::init()
{
    if (!Super::init())
    {
        return false;
    }

    return true;
}

void MainScene::onEnter()
{
    Super::onEnter();

    FileUtils::getInstance()->addSearchPath("res_zhcn");

    setupImGui();

    // Initialize application context
    AppContext::create();
    AppContext::get().init(this);

    if (parseOfflineLaunch())
        startOfflineBattle();
    else
        AppContext::get().screenManager()->switchScreen<gameui::LaunchScreen>();
    AppContext::get().screenManager()->flushPendingScreens();

    // scheduleUpdate() is required to ensure update(float) is called on every loop
    scheduleUpdate();
}

bool MainScene::parseOfflineLaunch()
{
    const std::string_view room = LaunchOptions::get("offline-room");
    if (room.empty())
        return false;
    const auto parseInt = [](std::string_view value, int32_t& result) {
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
        return error == std::errc{} && end == value.data() + value.size();
    };
    if (!parseInt(room, m_offline.roomId) || m_offline.roomId <= 0)
    {
        AXLOGE("Invalid offline room argument");
        return false;
    }
    const std::string_view role = LaunchOptions::get("offline-role");
    if (!role.empty())
    {
        if (!parseInt(role, m_offline.roleId))
        {
            if (role == "swordman")
                m_offline.roleId = 101;
            else if (role == "ranger")
                m_offline.roleId = 102;
            else if (role == "fighter")
                m_offline.roleId = 103;
            else if (role == "mage")
                m_offline.roleId = 104;
            else
            {
                AXLOGE("Invalid offline role argument");
                return false;
            }
        }
    }
    const std::string_view frames = LaunchOptions::get("autotest-frames");
    if (!frames.empty() && (!parseInt(frames, m_offline.autotestFrames) || m_offline.autotestFrames <= 0))
    {
        AXLOGE("Invalid autotest frame argument");
        return false;
    }
    m_offlineRequested = true;
    return true;
}

void MainScene::startOfflineBattle()
{
    auto loader = AppContext::get().globalLoader();
    loader->start({}, [this](const std::vector<gameres::ResourcePtr>& failed) {
        if (!failed.empty())
        {
            AXLOGE("Offline startup resource load failed");
            ax::Director::getInstance()->end();
            return;
        }
        if (!m_offlineStarted)
        {
            m_offlineStarted = true;
            AppContext::get().screenManager()->switchScreen<gameui::GameScreen>(m_offline);
        }
    });
}

void MainScene::onExit()
{
    AppContext::destroy();

    ax::extension::ImGuiPresenter::getInstance()->removeRenderLoop(IMGUI_LOOP_ID);

    Super::onExit();
}

void MainScene::update(float delta)
{
    Super::update(delta);
    AppContext::get().update(delta);
}

void MainScene::setupImGui()
{
    auto* presenter = ax::extension::ImGuiPresenter::getInstance();

    auto fontPath = ax::FileUtils::getInstance()->fullPathForFilename("fonts/fangzhengzhunyuan_GBK.ttf");
    if (!fontPath.empty())
    {
        presenter->addFont(fontPath, 16.0f);
    }

    presenter->addRenderLoop(IMGUI_LOOP_ID, AX_CALLBACK_0(MainScene::onImGuiRender, this), this);
}

void MainScene::onImGuiRender()
{
    auto* screenManager = AppContext::get().screenManager();
    if (auto* curScreen = screenManager->getCurrentScreen())
    {
        if (!curScreen->isLoading())
            curScreen->onImGUIRender();
    }
    // ImGui 回调内可能 switchScreen；必须在当前 Screen 的 onImGUIRender 返回后再销毁
    screenManager->flushPendingScreens();
}
