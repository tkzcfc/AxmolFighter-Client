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
#include "ui/core/ViewManager.h"
#include "ui/views/LaunchView.h"
#include "ui/views/LoginView.h"

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

    setupImGui();

    // Initialize application context
    AppContext::create();
    AppContext::get().init(this);
    AppContext::get().connectToServer("127.0.0.1", 7000);

    // Switch to login view
    AppContext::get().viewManager()->switchView<gameui::LoginView>();
    AppContext::get().viewManager()->flushPendingViews();

    // scheduleUpdate() is required to ensure update(float) is called on every loop
    scheduleUpdate();
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

    auto fontPath = ax::FileUtils::getInstance()->fullPathForFilename("fonts/Faint-DNF-Song-12px-Medium.ttf");
    if (!fontPath.empty())
    {
        presenter->addFont(fontPath, 16.0f);
    }

    presenter->addRenderLoop(IMGUI_LOOP_ID, AX_CALLBACK_0(MainScene::onImGuiRender, this), this);
}

void MainScene::onImGuiRender()
{
    auto* viewManager = AppContext::get().viewManager();
    if (auto* curView = viewManager->getCurrentView())
    {
        curView->onImGUIRender();
    }
    // ImGui 回调内可能 switchView；必须在当前 View 的 onImGUIRender 返回后再销毁
    viewManager->flushPendingViews();
}
