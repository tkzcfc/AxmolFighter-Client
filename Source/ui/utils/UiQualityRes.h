#pragma once

#include "axmol.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace gameui::UiQualityRes
{

// Mirrors heiyue EnumItemPlaceScenario (GlobalExcelEnum.lua) — scenarios needed by UI so far.
enum class PlaceScenario : int32_t
{
    Ordinary           = 1,   // equip icon frame
    Fashion            = 2,   // fashion part icon frame (shizhuang_kuang_*)
    EquipNameDark      = 4,   // dark-bg equip/suit name text color
    BagpackIcon        = 7,   // backpack icon name text color
    FashionTipQuality1 = 44,  // suit tip quality badge
    FashionTipQuality2 = 45,  // suit list quality badge (FashionSuitItem.loaderQuality)
    NewCommonItemBg    = 57,  // generic item backdrop
};

// Mirrors heiyue EnumItemRarity / fashion.quality / fashion_suit.suitQuality.
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

inline constexpr std::string_view kPkg = "ui://Res_GeneralIcons/";

// Snap arbitrary quality ints onto the nearest EnumItemRarity band used by EnumQualityToColor.
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

// Color key order matches EnumQualityToColor → EnumColorKey
// (white/green/blue/purple/orange/red/pink) — same slotting as config_quality rows.
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

// Mirrors EnumItemColor ordinal used by EnumQualityOrder-style sorts (white→pink).
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

inline std::string url(std::string_view resourceName)
{
    std::string out;
    out.reserve(kPkg.size() + resourceName.size());
    out.append(kPkg);
    out.append(resourceName);
    return out;
}

// FashionTipQuality1/2 (config_quality [44]/[45]): fuyong_pinzhi_* / pinji_ur.
inline std::string fashionTipQualityBadge(Rarity rarity)
{
    static constexpr std::string_view kNames[] = {
        "image_nfy_common_ui_fuyong_pinzhi_n",       // white
        "image_nfy_common_ui_fuyong_pinzhi_r_j",     // green
        "image_nfy_common_ui_fuyong_pinzhi_r",       // blue
        "image_nfy_common_ui_fuyong_pinzhi_sr",      // purple
        "image_nfy_common_ui_fuyong_pinzhi_ssr",     // orange
        "image_nfy_common_ui_new_tongyong_pinji_ur", // red
        "image_nfy_common_ui_fuyong_pinzhi_sr_j",    // pink
    };
    return url(kNames[colorIndex(rarity)]);
}

// PlaceScenario::Fashion (config_quality [2]): ui_shizhuang_kuang_*.
// Note: red reuses huang in heiyue res_image (1086 == 1085).
inline std::string fashionIconFrame(Rarity rarity)
{
    static constexpr std::string_view kNames[] = {
        "image_nfy_common_ui_shizhuang_kuang_bai",   // white
        "image_nfy_common_ui_shizhuang_kuang_lv",    // green
        "image_nfy_common_ui_shizhuang_kuang_lan",   // blue
        "image_nfy_common_ui_shizhuang_kuang_zi",    // purple
        "image_nfy_common_ui_shizhuang_kuang_huang", // orange
        "image_nfy_common_ui_shizhuang_kuang_huang", // red (same as orange in heiyue)
        "image_nfy_common_ui_shizhuang_kuang_fen",   // pink
    };
    return url(kNames[colorIndex(rarity)]);
}

// PlaceScenario::Ordinary (config_quality [1]): zhuangbeikuang_*.
// Red uses cai in heiyue; FairyGUI package currently has no cai — fall back to fen.
inline std::string ordinaryEquipFrame(Rarity rarity)
{
    static constexpr std::string_view kNames[] = {
        "image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_bai",
        "image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_lv",
        "image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_lan",
        "image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_zi",
        "image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_huang",
        "image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_fen", // red≈cai unavailable
        "image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_fen",
    };
    return url(kNames[colorIndex(rarity)]);
}

// PlaceScenario::NewCommonItemBg (config_quality [57]): zhuangbeidiban_* not all in package yet.
inline std::string newCommonItemBg(Rarity rarity)
{
    return ordinaryEquipFrame(rarity);
}

// config_color ids for EquipNameDark (scenario 4) / BagpackIcon (scenario 7).
// Pink is absent in those rows — reuse purple (same as heiyue missing-key fallback risk).
// RGB taken from config_color 1001–1006 / 1027–1032.
inline ax::Color3B equipNameDarkColor(Rarity rarity)
{
    static constexpr ax::Color3B kColors[] = {
        {255, 255, 255}, // white 1001
        {94, 241, 150},  // green 1002
        {165, 186, 255}, // blue 1003
        {214, 152, 235}, // purple 1004
        {228, 149, 50},  // orange 1005
        {228, 149, 50},  // red 1006 (heiyue shares orange RGB)
        {214, 152, 235}, // pink → purple stand-in
    };
    return kColors[colorIndex(rarity)];
}

inline ax::Color3B bagpackIconColor(Rarity rarity)
{
    // BagpackIcon 1027–1032 match EquipNameDark RGB in config_color.
    return equipNameDarkColor(rarity);
}

}  // namespace detail

// heiyue QUALITYBACKGROUND(useType, qualityType) → FairyGUI ui:// URL.
inline std::string backgroundUrl(PlaceScenario useType, int32_t quality)
{
    const auto rarity = detail::normalizeRarity(quality);
    switch (useType)
    {
    case PlaceScenario::Fashion:
        return detail::fashionIconFrame(rarity);
    case PlaceScenario::FashionTipQuality1:
    case PlaceScenario::FashionTipQuality2:
        return detail::fashionTipQualityBadge(rarity);
    case PlaceScenario::NewCommonItemBg:
        return detail::newCommonItemBg(rarity);
    case PlaceScenario::Ordinary:
    default:
        return detail::ordinaryEquipFrame(rarity);
    }
}

// heiyue QUALITYTEXT(useType, qualityType) → ax::Color3B.
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

// heiyue QUALITYORDER(quality) — EnumItemColor ordinal for sorts (higher = better).
inline int qualityOrder(int32_t quality)
{
    return detail::colorOrdinal(detail::normalizeRarity(quality));
}

}  // namespace gameui::UiQualityRes
