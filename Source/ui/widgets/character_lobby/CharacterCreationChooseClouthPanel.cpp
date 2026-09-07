#include "CharacterCreationChooseClouthPanel.h"
#include "ui/views/CharacterLobbyView.h"
#include "ui/widgets/common/MessageDialog.h"
#include "mugen/render/SpineSkeletonLoader.h"
#include "mugen/conf/GameDef.h"
#include "ui/core/AudioManager.h"
#include <net/client_game.pb.h>
#include "CharacterCreationPanel.h"

namespace gameui
{

constexpr int kHairCount = 5;
constexpr int kClouthCount = 5;

struct CharacterClouthConfig
{
    const char* hairs[kHairCount];
    const char* cloths[kClouthCount];
};

static const CharacterClouthConfig kCharacterClouthConfig[mugen::CharacterClass::kCount] = {
    {
        {"ui://CharacterLobby/ui_juesechuangjian_tou_01_1", "ui://CharacterLobby/ui_juesechuangjian_tou_01_2", "ui://CharacterLobby/ui_juesechuangjian_tou_01_3", "ui://CharacterLobby/ui_juesechuangjian_tou_01_4", "ui://CharacterLobby/ui_juesechuangjian_tou_01_5"},
        {"ui://CharacterLobby/ui_juesechuangjian_shizhuang_01_1", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_01_2", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_01_3", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_01_4", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_01_5"},
    },
    {
        {"ui://CharacterLobby/ui_juesechuangjian_tou_03_1", "ui://CharacterLobby/ui_juesechuangjian_tou_03_2", "ui://CharacterLobby/ui_juesechuangjian_tou_03_3", "ui://CharacterLobby/ui_juesechuangjian_tou_03_4", "ui://CharacterLobby/ui_juesechuangjian_tou_03_5"},
        {"ui://CharacterLobby/ui_juesechuangjian_shizhuang_03_1", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_03_2", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_03_3", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_03_4", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_03_5"},
    },
    {
        {"ui://CharacterLobby/ui_juesechuangjian_tou_02_1", "ui://CharacterLobby/ui_juesechuangjian_tou_02_2", "ui://CharacterLobby/ui_juesechuangjian_tou_02_3", "ui://CharacterLobby/ui_juesechuangjian_tou_02_4", "ui://CharacterLobby/ui_juesechuangjian_tou_02_5"},
        {"ui://CharacterLobby/ui_juesechuangjian_shizhuang_02_1", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_02_2", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_02_3", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_02_4",  "ui://CharacterLobby/ui_juesechuangjian_shizhuang_02_5"},
    },
    {
        {"ui://CharacterLobby/ui_juesechuangjian_tou_04_1", "ui://CharacterLobby/ui_juesechuangjian_tou_04_2", "ui://CharacterLobby/ui_juesechuangjian_tou_04_3", "ui://CharacterLobby/ui_juesechuangjian_tou_04_4", "ui://CharacterLobby/ui_juesechuangjian_tou_04_5"},
        {"ui://CharacterLobby/ui_juesechuangjian_shizhuang_04_1", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_04_2", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_04_3", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_04_4", "ui://CharacterLobby/ui_juesechuangjian_shizhuang_04_5"},
    },
};

void CharacterCreationChooseClouthPanel::onCreate()
{
    auto backButton = this->getChild<GButton>("backButton");
    this->addClickListener(backButton, AX_CALLBACK_1(CharacterCreationChooseClouthPanel::onClickBackButton, this));

    auto container = this->getChild<GComponent>("container");

    m_choiceHairList     = container->getChild("choiceHair")->as<GComponent>()->getChild("list")->as<GList>();
    m_choiceClothingList = container->getChild("choiceClothing")->as<GComponent>()->getChild("list")->as<GList>();

        m_choiceHairList->addEventListener(UIEventType::Scroll, [this](EventContext*) {
        this->m_choiceHairListScrollEnd = false;
    });
    m_choiceClothingList->addEventListener(UIEventType::Scroll,
                                               [this](EventContext*) { this->m_choiceClothingListScrollEnd = false; });
    m_choiceHairList->addEventListener(UIEventType::ScrollEnd,
                                       [this](EventContext*) { this->m_choiceHairListScrollEnd = true;
        this->updateUI();
        });
    m_choiceClothingList->addEventListener(UIEventType::ScrollEnd, [this](EventContext*) {
        this->m_choiceClothingListScrollEnd = true;
        this->updateUI();
    });

    m_inputTextName = container->getChild("inputTextName")->as<GTextInput>();

    // 创建角色按钮
    auto createButton = container->getChild("createButton")->as<GButton>();
    this->addClickListener(createButton, AX_CALLBACK_1(CharacterCreationChooseClouthPanel::onClickCreateButton, this));
}

void CharacterCreationChooseClouthPanel::onShow()
{
    m_choiceHairList->itemRenderer = [this](int index, GObject* obj) {
        auto& curConfig = kCharacterClouthConfig[static_cast<int32_t>(this->m_curClass) - 1];
        obj->setIcon(curConfig.hairs[index]);
        obj->setText(std::to_string(index));
        };
    m_choiceHairList->setVirtualAndLoop();
    m_choiceHairList->setNumItems(kHairCount);
    m_choiceHairList->addEventListener(UIEventType::Scroll,
                                       AX_CALLBACK_1(CharacterCreationChooseClouthPanel::doSpecialEffect, this));
    
    m_choiceClothingList->itemRenderer = [this](int index, GObject* obj) {
        auto& curConfig = kCharacterClouthConfig[static_cast<int32_t>(this->m_curClass) - 1];
        obj->setIcon(curConfig.cloths[index]);
        obj->setText(std::to_string(index));
    };
    m_choiceClothingList->setVirtualAndLoop();
    m_choiceClothingList->setNumItems(kClouthCount);
    m_choiceClothingList->addEventListener(UIEventType::Scroll,
                                       AX_CALLBACK_1(CharacterCreationChooseClouthPanel::doSpecialEffect, this));

    doSpecialEffect(nullptr);
}

void CharacterCreationChooseClouthPanel::onDestroy()
{
}

void CharacterCreationChooseClouthPanel::updateUI()
{
    // 正在滚动中,不更新UI
    if (!m_choiceHairListScrollEnd || !m_choiceClothingListScrollEnd)
        return;

    // 当前选择的索引无效
    if (m_curHairIndex == -1 || m_curClothingIndex == -1)
        return;

    // 当前显示的索引与当前选择的索引相同,不更新UI
    if (m_showHairIndex == m_curHairIndex && m_showClothingIndex == m_curClothingIndex)
        return;

    m_showHairIndex = m_curHairIndex;
    m_showClothingIndex = m_curClothingIndex;

    auto container     = this->getChild<GComponent>("container");
    auto textCurSelect = container->getChild("textCurSelect")->as<GTextField>();
    textCurSelect->setText(std::to_string(m_curHairIndex) + "-" + std::to_string(m_curClothingIndex));

    AXLOGI("CharacterCreationChooseClouthPanel::updateUI: hairIndex={}, clothingIndex={}", m_curHairIndex,
           m_curClothingIndex);

    auto t0 = container->getTransition("t0");
    if (t0)
    {
        t0->stop();
        t0->play();
    }

    auto loaderAvatar = container->getChild("loaderAvatar")->as<GLoader3D>();
    // 创建spine骨骼动画
    // ...
}

void CharacterCreationChooseClouthPanel::doSpecialEffect(EventContext* context)
{
    int32_t nextHairIndex   = (m_choiceHairList->getFirstChildInView() + 1) % m_choiceHairList->getNumItems();
    int32_t nextClouthIndex = (m_choiceClothingList->getFirstChildInView() + 1) % m_choiceClothingList->getNumItems();

    adjustListItem(m_choiceHairList, nextHairIndex);
    adjustListItem(m_choiceClothingList, nextClouthIndex);

    if (nextHairIndex != m_curHairIndex || nextClouthIndex != m_curClothingIndex)
    {
        m_curHairIndex = nextHairIndex;
        m_curClothingIndex = nextClouthIndex;
        this->updateUI();
    }
}

void CharacterCreationChooseClouthPanel::adjustListItem(GList* list, int32_t curIndex)
{
    float midX = list->getScrollPane()->getPosX() + list->getViewWidth() / 2;
    int cnt    = list->numChildren();
    for (int i = 0; i < cnt; i++)
    {
        GButton* obj = list->getChildAt(i)->as<GButton>();
        //float dist   = std::abs(midX - obj->getX() - obj->getWidth() / 2);
        //if (dist > obj->getWidth())  // no intersection
        //{
        //    obj->setScale(1, 1);
        //}
        //else
        //{
        //    float ss = 1 + (1 - dist / obj->getWidth()) * 0.24f;
        //    obj->setScale(ss, ss);
        //}

        obj->setSelected(i == curIndex);
    }
}

void CharacterCreationChooseClouthPanel::onClickCreateButton(EventContext* context)
{
    
}

void CharacterCreationChooseClouthPanel::onClickBackButton(EventContext* context)
{
    this->getUIManager()->open<CharacterCreationPanel>();
    this->close();
}

}  // namespace gameui
