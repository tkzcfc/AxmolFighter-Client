#pragma once

#include "axmol.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace gameui::UiQualityRes
{

// 品质资源使用场景。数值与 config_quality 行号一致，便于对表。
enum class PlaceScenario : int32_t
{
    Ordinary          , // 装备图标框
    Fashion           , // 时装部位图标框（shizhuang_kuang_*）
    EquipNameDark     , // 暗底装备/套装名字颜色
    BagpackIcon       , // 背包图标名字颜色
    FashionTipQuality , // 套装提示品级徽章
    CommonItemBg      , // 通用道具底板
};

// 稀有度分档，对应 fashion.quality / fashion_suit.suitQuality。
enum class Rarity : int32_t
{
    None   = 0,
    White  = 100,
    Blue   = 200,
    Green  = 300,
    Purple = 400,
    Pink   = 450,
    Orange = 500,
    Red    = 600,
};


namespace detail
{

// 把任意品质整数收束到最近的稀有度档。
inline Rarity normalizeRarity(int32_t quality)
{
    if (quality >= static_cast<int32_t>(Rarity::Red))
        return Rarity::Red;
    if (quality >= static_cast<int32_t>(Rarity::Orange))
        return Rarity::Orange;
    if (quality >= static_cast<int32_t>(Rarity::Pink))
        return Rarity::Pink;
    if (quality >= static_cast<int32_t>(Rarity::Purple))
        return Rarity::Purple;
    if (quality >= static_cast<int32_t>(Rarity::Green))
        return Rarity::Green;
    if (quality >= static_cast<int32_t>(Rarity::Blue))
        return Rarity::Blue;
    if (quality >= static_cast<int32_t>(Rarity::White))
        return Rarity::White;
    return Rarity::None;
}

// 查找表下标顺序：白/绿/蓝/紫/橙/红/粉（与 config_quality 列顺序一致，不是 100/200/300 数值序）。
inline int colorIndex(Rarity rarity)
{
    switch (rarity)
    {
    case Rarity::White:
        return 0;
    case Rarity::Green:
        return 1;
    case Rarity::Blue:
        return 2;
    case Rarity::Purple:
        return 3;
    case Rarity::Orange:
        return 4;
    case Rarity::Red:
        return 5;
    case Rarity::Pink:
        return 6;
    default:
        return 0;
    }
}

// 颜色序（白→粉），用于 qualityOrder；不是品质数值序。
inline int colorOrdinal(Rarity rarity)
{
    switch (rarity)
    {
    case Rarity::White:
        return 1;
    case Rarity::Green:
        return 2;
    case Rarity::Blue:
        return 3;
    case Rarity::Purple:
        return 4;
    case Rarity::Orange:
        return 5;
    case Rarity::Red:
        return 6;
    case Rarity::Pink:
        return 7;
    default:
        return 0;
    }
}

// 时装品级徽章
inline std::string fashionTipQualityBadge(Rarity rarity)
{
    static constexpr std::string_view kNames[] = {
        "ui://Res_GeneralIcons/image_nfy_common_ui_fuyong_pinzhi_n",      // N  
        "ui://Res_GeneralIcons/image_nfy_common_ui_fuyong_pinzhi_r",      // R
        "ui://Res_GeneralIcons/image_nfy_common_ui_fuyong_pinzhi_r_j",    // R+
        "ui://Res_GeneralIcons/image_nfy_common_ui_fuyong_pinzhi_sr",     // SR
        "ui://Res_GeneralIcons/image_nfy_common_ui_fuyong_pinzhi_sr_j",   // SR+
        "ui://Res_GeneralIcons/image_nfy_common_ui_fuyong_pinzhi_ssr",    // SSR
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_pinji_ur",// UR
    };
    return std::string(kNames[colorIndex(rarity)]);
}

// 时装图标框
inline std::string fashionIconFrame(Rarity rarity)
{
    static constexpr std::string_view kNames[] = {
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_bai",   // 白
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_lv",    // 绿
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_lan",   // 蓝
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_zi",    // 紫
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_huang", // 橙
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_huang", // 红（与橙同图）
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_fen",   // 粉
    };
    return std::string(kNames[colorIndex(rarity)]);
}

// 装备图标框
inline std::string ordinaryEquipFrame(Rarity rarity)
{
    static constexpr std::string_view kNames[] = {
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_bai",
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_lv",
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_lan",
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_zi",
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_huang",
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_fen", // 红：cai 缺失，回退 fen
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_fen",
    };
    return std::string(kNames[colorIndex(rarity)]);
}

// 通用道具底板
inline std::string CommonItemBg(Rarity rarity)
{
    static constexpr std::string_view kNames[] = {
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_bai",   // 白
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_lv",    // 绿
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_lan",   // 蓝
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_zise",  // 紫
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_cheng", // 橙
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_cai",   // 红
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_fen",   // 粉
    };
    return std::string(kNames[colorIndex(rarity)]);
}

// 暗底名字色（config_quality [4] / 色表 1001–1006）。粉档该行无字段，走默认色 94。
// RGB 表不能写成 constexpr Color3B（MSVC C2131）。
inline ax::Color3B equipNameDarkColor(Rarity rarity)
{
    static const uint8_t kRgb[][3] = {
        {255, 255, 255}, // 白 1001
        {94, 241, 150},  // 绿 1002
        {165, 186, 255}, // 蓝 1003
        {214, 152, 235}, // 紫 1004
        {228, 149, 50},  // 橙 1005
        {228, 149, 50},  // 红 1006（与橙同色）
        {255, 88, 190},  // 粉 94
    };
    const auto& rgb = kRgb[colorIndex(rarity)];
    return ax::Color3B(rgb[0], rgb[1], rgb[2]);
}

// 背包图标名字色（config_quality [7] / 色表 1027–1032）。白–红与暗底名字色相同，粉档同样用 94。
inline ax::Color3B bagpackIconColor(Rarity rarity)
{
    return equipNameDarkColor(rarity);
}

}  // namespace detail

// 品质背景图 → FairyGUI ui:// URL。
inline std::string backgroundUrl(PlaceScenario useType, int32_t quality)
{
    const auto rarity = detail::normalizeRarity(quality);
    switch (useType)
    {
    case PlaceScenario::Fashion:
        return detail::fashionIconFrame(rarity);
    case PlaceScenario::FashionTipQuality:
        return detail::fashionTipQualityBadge(rarity);
    case PlaceScenario::CommonItemBg:
        return detail::CommonItemBg(rarity);
    case PlaceScenario::Ordinary:
    default:
        return detail::ordinaryEquipFrame(rarity);
    }
}

// 品质文字色 → ax::Color3B。
inline ax::Color3B textColor(PlaceScenario useType, int32_t quality)
{
    const auto rarity = detail::normalizeRarity(quality);
    switch (useType)
    {
    case PlaceScenario::BagpackIcon:
        return detail::bagpackIconColor(rarity);
    case PlaceScenario::EquipNameDark:
    default:
        return detail::equipNameDarkColor(rarity);
    }
}

// 颜色序（白 1 … 粉 7），不是 100–600 品质数值序。数值越大越好。
inline int qualityOrder(int32_t quality)
{
    return detail::colorOrdinal(detail::normalizeRarity(quality));
}

}  // namespace gameui::UiQualityRes
