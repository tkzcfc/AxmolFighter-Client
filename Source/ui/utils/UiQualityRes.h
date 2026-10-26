#pragma once

#include "axmol.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace gameui::UiQualityRes
{

// 品质资源使用场景。
enum class PlaceScenario : int32_t
{
    CommonItemBg,       // 通用道具底板
    Ordinary          , // 装备图标框
    Fashion           , // 时装部位图标框（shizhuang_kuang_*）
    EquipNameDark     , // 暗底装备/套装名字颜色
    BagpackIcon       , // 背包图标名字颜色
    FashionTipQuality , // 套装提示品级徽章
};

// 稀有度
enum class Rarity : int32_t
{
    None   = 0,
    N      = 100, // N 白色
    R      = 200, // R 蓝色
    RPlus  = 300, // R+ 绿色
    SR     = 400, // SR 紫色
    SRPlus = 450, // SR+ 粉色
    SSR    = 500, // SSR 橙色
    UR     = 600, // UR 粉色和蓝色的渐变色
};

namespace detail
{

// 把任意品质整数收束到最近的稀有度档。
inline Rarity normalizeRarity(int32_t quality)
{
    if (quality >= static_cast<int32_t>(Rarity::UR))
        return Rarity::UR;
    if (quality >= static_cast<int32_t>(Rarity::SSR))
        return Rarity::SSR;
    if (quality >= static_cast<int32_t>(Rarity::SRPlus))
        return Rarity::SRPlus;
    if (quality >= static_cast<int32_t>(Rarity::SR))
        return Rarity::SR;
    if (quality >= static_cast<int32_t>(Rarity::RPlus))
        return Rarity::RPlus;
    if (quality >= static_cast<int32_t>(Rarity::R))
        return Rarity::R;
    if (quality >= static_cast<int32_t>(Rarity::N))
        return Rarity::N;
    return Rarity::None;
}

// 查找表下标：N / R / R+ / SR / SR+ / SSR / UR。
inline int rarityIndex(Rarity rarity)
{
    switch (rarity)
    {
    case Rarity::N:
        return 0;
    case Rarity::R:
        return 1;
    case Rarity::RPlus:
        return 2;
    case Rarity::SR:
        return 3;
    case Rarity::SRPlus:
        return 4;
    case Rarity::SSR:
        return 5;
    case Rarity::UR:
        return 6;
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
    return std::string(kNames[rarityIndex(rarity)]);
}

// 时装图标框
inline std::string fashionIconFrame(Rarity rarity)
{
    static constexpr std::string_view kNames[] = {
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_bai",   // N 白
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_lan",   // R 蓝
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_lv",    // R+ 绿
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_zi",    // SR 紫
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_fen",   // SR+ 粉
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_huang", // SSR 橙
        "ui://Res_GeneralIcons/image_nfy_common_ui_shizhuang_kuang_huang", // UR
    };
    return std::string(kNames[rarityIndex(rarity)]);
}

// 装备图标框
inline std::string ordinaryEquipFrame(Rarity rarity)
{
    static constexpr std::string_view kNames[] = {
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_bai",   // N
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_lan",   // R
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_lv",    // R+
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_zi",    // SR
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_fen",   // SR+
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_huang", // SSR
        "ui://Res_GeneralIcons/image_nfy_common_ui_zhuangbei_zhujiemian_zhuangbeikuang_huang", // UR
    };
    return std::string(kNames[rarityIndex(rarity)]);
}

// 通用道具底板
inline std::string CommonItemBg(Rarity rarity)
{
    static constexpr std::string_view kNames[] = {
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_bai",   // N
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_lan",   // R
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_lv",    // R+
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_zise",  // SR
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_fen",   // SR+
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_cheng", // SSR
        "ui://Res_GeneralIcons/image_nfy_common_ui_new_tongyong_zhuangbeidiban_cai",   // UR
    };
    return std::string(kNames[rarityIndex(rarity)]);
}

// 暗底名字色。SR+ 该行无字段，走默认色 94。
// RGB 表不能写成 constexpr Color3B（MSVC C2131）。
inline ax::Color3B equipNameDarkColor(Rarity rarity)
{
    static const uint8_t kRgb[][3] = {
        {255, 255, 255}, // N
        {165, 186, 255}, // R
        {94, 241, 150},  // R+
        {214, 152, 235}, // SR
        {255, 88, 190},  // SR+
        {228, 149, 50},  // SSR
        {228, 149, 50},  // UR（与 SSR 同色）
    };
    const auto& rgb = kRgb[rarityIndex(rarity)];
    return ax::Color3B(rgb[0], rgb[1], rgb[2]);
}

// 背包图标名字色。N–UR 与暗底名字色相同。
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

// 品级序：N=1 … UR=7，与 100–600 数值序一致。数值越大越好。
inline int qualityOrder(int32_t quality)
{
    const auto rarity = detail::normalizeRarity(quality);
    return rarity == Rarity::None ? 0 : detail::rarityIndex(rarity) + 1;
}

}  // namespace gameui::UiQualityRes
