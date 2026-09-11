#pragma once

#include "ui/core/UIWidget.h"
#include "ui/UIMediaPlayer.h"
#include "mugen/conf/GameDef.h"

namespace gameui
{

class CharacterCreationChooseClouthPanel : public UIWidget
{
public:
    CharacterCreationChooseClouthPanel(mugen::CharacterClass curClass) : m_curClass(curClass)
    {
        m_options.hasBackground  = false;
        m_options.draggable      = false;
        m_options.fullscreen     = true;
        m_options.closeOnClickBg = false;
    }

    virtual ~CharacterCreationChooseClouthPanel() {}

protected:
    virtual std::vector<std::string> getPackages() const override { return {"UI/CharacterLobby"}; }

    virtual GComponent* onCreateContent() override
    {
        return createCenteredComponent("CharacterLobby", "CharacterCreationChooseClouthPanel");
    }

    virtual void onCreate() override;

    virtual void onShow() override;

    virtual void onDestroy() override;

    void updateUI();

    void doSpecialEffect(EventContext*);

    void adjustListItem(GList* list, int32_t curIndex);

    void onClickCreateButton(EventContext* context);

    void onClickBackButton(EventContext* context);

private:
    GList* m_choiceHairList          = nullptr;
    GList* m_choiceClothingList      = nullptr;
    GTextInput* m_inputTextName      = nullptr;
    mugen::CharacterClass m_curClass = mugen::kUnknown;

    bool m_choiceHairListScrollEnd     = true;
    bool m_choiceClothingListScrollEnd = true;

    int32_t m_curHairIndex     = -1;
    int32_t m_curClothingIndex = -1;

    int32_t m_showHairIndex     = -1;
    int32_t m_showClothingIndex = -1;
};

}  // namespace gameui
