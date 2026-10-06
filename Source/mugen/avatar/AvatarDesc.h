#pragma once

#include "mugen/conf/GameDef.h"
#include "mugen/core/StdC.h"

NS_MG_BEGIN

// 挂件槽位：独立 spine，与身体同节点叠放（武器/翅膀同时嫁接到身体插槽）
enum class AvatarAccessorySlot : int32_t
{
    kWeapon    = 0,
    kWing      = 1,
    kHaloBack  = 2,  // 光环后层（与 kHaloFront 同一资源，不同动画）
    kHaloFront = 3,
    kCount     = 4,
};

// 渲染一个角色所需的全部资源描述（逻辑侧可构造，不依赖渲染）
struct AvatarDesc
{
    // spine 骨架路径
    std::string skeleton;
    // 合并加载的图集列表；为空取骨架同名 .atlas
    std::vector<std::string> atlases;
    // spine 皮肤名；为空不切换
    std::string skin;
    // spine 缩放
    float scale = 1.0f;
    // .motion 文件（动作名 → spine 动画 + .box 时间轴）
    std::string motionFile;
    // 各挂件的 ResSpine id（0 = 无）
    std::array<int32_t, static_cast<size_t>(AvatarAccessorySlot::kCount)> accessories = {};
    // 职业（决定挂件插槽名与 z-order）
    CharacterClass characterClass = CharacterClass::kUnknown;

    int32_t accessory(AvatarAccessorySlot slot) const { return accessories[static_cast<size_t>(slot)]; }
    void setAccessory(AvatarAccessorySlot slot, int32_t resSpineId)
    {
        accessories[static_cast<size_t>(slot)] = resSpineId;
    }
};

NS_MG_END
