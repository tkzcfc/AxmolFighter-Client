#pragma once

#include "mugen/avatar/AvatarDesc.h"
#include "mugen/conf/ReflectedConfig.h"

#include <string>
#include <vector>

NS_MG_BEGIN

// 挂件规则：挂件节点在 Avatar 内的 z-order、循环动画、显示条件，以及嫁接到身体的插槽名。
// 层级和嫁接槽来自 avatar_class；循环动画与是否仅待机显示按槽位固定。
struct AvatarAccessoryRule
{
    // 相对身体（z=0）的 z-order
    int zOrder = 0;
    // 挂件自身循环播放的动画
    const char* loopAnim = "";
    // 仅在身体播放待机动作时显示（其余动作由嫁接到身体插槽的 attachment 表现）
    bool standOnly = false;
    // 嫁接到身体的插槽/attachment 名；为空表示不嫁接。指向 avatar_class 行内的 vector。
    const std::vector<std::string>* graftSlots = nullptr;
};

struct AvatarAccessoryRules
{
    // 身体节点的基准层级。
    static constexpr int kBodyZOrder            = 0;
    static constexpr const char* kStandAnim     = "stand";
    static constexpr const char* kHaloBackAnim  = "guanghuanhou";
    static constexpr const char* kHaloFrontAnim = "guanghuanqian";
    static constexpr const char* kDonorSkin     = "default";

    // 按挂件槽位和职业返回表现规则。职业行缺失时 z-order 为 0 且不嫁接。
    static AvatarAccessoryRule rule(AvatarAccessorySlot slot, CharacterClass cls)
    {
        const auto* row =
            conf::Config::instance().find<conf::AvatarClassConfig>(static_cast<int32_t>(cls));
        if (!row)
            MG_LOG_E("AvatarAccessoryRules: avatar_class {} missing", static_cast<int32_t>(cls));

        AvatarAccessoryRule r;
        switch (slot)
        {
        case AvatarAccessorySlot::kWeapon:
            r.zOrder     = row ? row->weaponZOrder : 0;
            r.loopAnim   = kStandAnim;
            r.standOnly  = true;
            r.graftSlots = row ? &row->weaponGraftSlots : nullptr;
            break;
        case AvatarAccessorySlot::kWing:
            r.zOrder     = row ? row->wingZOrder : 0;
            r.loopAnim   = kStandAnim;
            r.standOnly  = true;
            r.graftSlots = row ? &row->wingGraftSlots : nullptr;
            break;
        case AvatarAccessorySlot::kHaloBack:
            r.zOrder   = row ? row->haloBackZOrder : 0;
            r.loopAnim = kHaloBackAnim;
            break;
        case AvatarAccessorySlot::kHaloFront:
            r.zOrder   = row ? row->haloFrontZOrder : 0;
            r.loopAnim = kHaloFrontAnim;
            break;
        default:
            break;
        }
        return r;
    }
};

NS_MG_END
