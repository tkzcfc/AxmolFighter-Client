#pragma once

#include "mugen/conf/GameDef.h"

#include "base/EventKeyboard.h"

#include <map>

namespace gameui
{

// 城镇：仅四向移动
inline void fillTownInputSlotMap(std::map<ax::EventKeyboard::KeyCode, uint32_t>& out)
{
    out.clear();
    out[ax::EventKeyboard::KeyCode::KEY_LEFT_ARROW]  = mugen::INPUT_SLOT_MOVE_LEFT;
    out[ax::EventKeyboard::KeyCode::KEY_RIGHT_ARROW] = mugen::INPUT_SLOT_MOVE_RIGHT;
    out[ax::EventKeyboard::KeyCode::KEY_UP_ARROW]    = mugen::INPUT_SLOT_MOVE_UP;
    out[ax::EventKeyboard::KeyCode::KEY_DOWN_ARROW]  = mugen::INPUT_SLOT_MOVE_DOWN;
}

// 战斗：移动 + Z/X/C + 技能栏 A / 1-9 / 0
// X 不是冲刺（黑月冲刺=双击同向方向键）；X/C 为技能/特殊槽位输入
inline void fillCombatInputSlotMap(std::map<ax::EventKeyboard::KeyCode, uint32_t>& out)
{
    fillTownInputSlotMap(out);

    out[ax::EventKeyboard::KeyCode::KEY_Z] = mugen::INPUT_SLOT_Z;
    out[ax::EventKeyboard::KeyCode::KEY_X] = mugen::INPUT_SLOT_X;
    out[ax::EventKeyboard::KeyCode::KEY_C] = mugen::INPUT_SLOT_C;

    out[ax::EventKeyboard::KeyCode::KEY_A] = mugen::INPUT_SLOT_0;
    out[ax::EventKeyboard::KeyCode::KEY_1] = mugen::INPUT_SLOT_1;
    out[ax::EventKeyboard::KeyCode::KEY_2] = mugen::INPUT_SLOT_2;
    out[ax::EventKeyboard::KeyCode::KEY_3] = mugen::INPUT_SLOT_3;
    out[ax::EventKeyboard::KeyCode::KEY_4] = mugen::INPUT_SLOT_4;
    out[ax::EventKeyboard::KeyCode::KEY_5] = mugen::INPUT_SLOT_5;
    out[ax::EventKeyboard::KeyCode::KEY_6] = mugen::INPUT_SLOT_6;
    out[ax::EventKeyboard::KeyCode::KEY_7] = mugen::INPUT_SLOT_7;
    out[ax::EventKeyboard::KeyCode::KEY_8] = mugen::INPUT_SLOT_8;
    out[ax::EventKeyboard::KeyCode::KEY_9] = mugen::INPUT_SLOT_9;
    out[ax::EventKeyboard::KeyCode::KEY_0] = mugen::INPUT_SLOT_10;
}

}  // namespace gameui
