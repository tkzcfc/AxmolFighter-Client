#pragma once

#include "mugen/core/reflect/Reflect.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mugen::conf
{

// 创角界面可选的头发展示项。
struct CreateRoleHairLookConfig
{
    // FairyGUI 头发展示图标。
    std::string icon;
    // 对应的 res_fashion 头发 id。
    int32_t id = 0;

    MG_REFLECT(CreateRoleHairLookConfig, icon, id)
};

// 创角界面可选的服装展示项。
struct CreateRoleClothLookConfig
{
    // FairyGUI 服装展示图标。
    std::string icon;
    // 对应的 res_fashion 衣服 id。
    int32_t id = 0;
    // 与该服装绑定的 res_fashion 皮肤 id，创建请求不发送。
    int32_t skinId = 0;

    MG_REFLECT(CreateRoleClothLookConfig, icon, id, skinId)
};

// 创角职业列表行，id 为 CharacterClass。
struct CreateRoleConfig
{
    // 职业 id，等于 CharacterClass。
    int32_t id = 0;
    // 是否可在创角界面点选。
    bool enabled = true;
    // 职业名称。
    std::string name;
    // 职业描述。
    std::string description;
    // 操作难度，范围 [1, 5]，对应界面 c_star。
    int32_t difficulty = 1;
    // 是否允许在创角时选择时装。
    bool canSelectFashion = true;
    // 预览用 res_spine id。
    int32_t previewSpineId = 0;
    // 预览水平缩放。
    Fixed previewScaleX;
    // 预览竖直缩放。
    Fixed previewScaleY;
    // 预览水平偏移。
    Fixed previewOffsetX;
    // 预览竖直偏移。
    Fixed previewOffsetY;
    // 预览待机动画名。
    std::string previewAnim;
    // 创角可选头发列表。
    std::vector<CreateRoleHairLookConfig> hairLooks;
    // 创角可选服装列表。
    std::vector<CreateRoleClothLookConfig> clothLooks;

    MG_REFLECT(CreateRoleConfig,
               id,
               enabled,
               name,
               description,
               difficulty,
               canSelectFashion,
               previewSpineId,
               previewScaleX,
               previewScaleY,
               previewOffsetX,
               previewOffsetY,
               previewAnim,
               hairLooks,
               clothLooks)
};

}  // namespace mugen::conf
