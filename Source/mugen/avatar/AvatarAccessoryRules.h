#pragma once

#include "mugen/avatar/AvatarDesc.h"

#include <string>
#include <vector>

NS_MG_BEGIN

// 挂件规则：挂件节点在 Avatar 内的 z-order、循环动画、显示条件，以及嫁接到身体的插槽名（按职业）
struct AvatarAccessoryRule
{
    // 相对身体（z=0）的 z-order
    int zOrder = 0;
    // 挂件自身循环播放的动画
    const char* loopAnim = "";
    // 仅在身体播放待机动作时显示（其余动作由嫁接到身体插槽的 attachment 表现）
    bool standOnly = false;
    // 嫁接到身体的插槽/attachment 名；为空表示不嫁接
    const std::vector<std::string>* graftSlots = nullptr;
};

struct AvatarAccessoryRules
{
    static constexpr int kBodyZOrder            = 0;
    static constexpr const char* kStandAnim     = "stand";
    static constexpr const char* kHaloBackAnim  = "guanghuanhou";
    static constexpr const char* kHaloFrontAnim = "guanghuanqian";
    static constexpr const char* kDonorSkin     = "default";

    static AvatarAccessoryRule rule(AvatarAccessorySlot slot, CharacterClass cls)
    {
        AvatarAccessoryRule r;
        switch (slot)
        {
        case AvatarAccessorySlot::kWeapon:
            r.zOrder     = cls == CharacterClass::kRanger ? 1 : -1;
            r.loopAnim   = kStandAnim;
            r.standOnly  = true;
            r.graftSlots = &weaponSlots(cls);
            break;
        case AvatarAccessorySlot::kWing:
            r.zOrder     = cls == CharacterClass::kFighter ? -2 : -1;
            r.loopAnim   = kStandAnim;
            r.standOnly  = true;
            r.graftSlots = &wingSlots(cls);
            break;
        case AvatarAccessorySlot::kHaloBack:
            r.zOrder   = cls == CharacterClass::kFighter ? -3 : -4;
            r.loopAnim = kHaloBackAnim;
            break;
        case AvatarAccessorySlot::kHaloFront:
            r.zOrder   = 1;
            r.loopAnim = kHaloFrontAnim;
            break;
        default:
            break;
        }
        return r;
    }

    static const std::vector<std::string>& weaponSlots(CharacterClass cls)
    {
        static const std::vector<std::string> kSword   = {"body/dao", "body/dao2", "body/daobing", "body/daoshen"};
        static const std::vector<std::string> kRanger  = {"body/youdao", "body/youdao2"};
        static const std::vector<std::string> kFighter = {"body/zuoshouwuqi", "body/youshouwuqi"};
        static const std::vector<std::string> kMage    = {"wuqi"};
        static const std::vector<std::string> kEmpty;
        switch (cls)
        {
        case CharacterClass::kSwordman:
            return kSword;
        case CharacterClass::kRanger:
            return kRanger;
        case CharacterClass::kFighter:
            return kFighter;
        case CharacterClass::kMage:
            return kMage;
        default:
            return kEmpty;
        }
    }

    static const std::vector<std::string>& wingSlots(CharacterClass cls)
    {
        static const std::vector<std::string> kTwo = {"body/chibang", "body/chibang2"};
        static const std::vector<std::string> kOne = {"body/chibang"};
        static const std::vector<std::string> kEmpty;
        switch (cls)
        {
        case CharacterClass::kSwordman:
        case CharacterClass::kRanger:
        case CharacterClass::kMage:
            return kTwo;
        case CharacterClass::kFighter:
            return kOne;
        default:
            return kEmpty;
        }
    }
};

NS_MG_END
