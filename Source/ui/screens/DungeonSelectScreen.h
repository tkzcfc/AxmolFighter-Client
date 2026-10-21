#pragma once

#include "ui/core/Screen.h"

#include <cstdint>
#include <vector>

namespace gameui
{

// 副本选择界面（纯 ImGui，叠加在 TownScreen 上）
class DungeonSelectScreen : public Screen
{
public:
    typedef Screen Super;

public:
    void onEnter() override;
    void onUpdate(float delta) override;
    void onImGUIRender() override;

private:
    void rebuildChapterList();
    void enterCopy(int32_t copyId);
    void requestClose();

private:
    struct ChapterItem
    {
        int32_t id     = 0;
        int32_t nameId = 0;
        std::vector<int32_t> mainCopys;
    };

    std::vector<ChapterItem> m_chapters;
    int32_t m_selectedChapterIndex = 0;
    int32_t m_selectedCopyId       = 0;
    bool m_requestClose            = false;
};

}  // namespace gameui
