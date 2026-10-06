#pragma once

#include "mugen/avatar/AvatarDesc.h"

NS_MG_BEGIN

// 角色外观（按部位的时装选择）
struct FashionAppearance
{
    // 角色id
    int32_t roleId = 0;

    // 基础外观：部位 → ResFashion id（身体部位）或 ResSpine id（武器/翅膀/光环）
    std::unordered_map<FashionPosition, int32_t> baseFashion;

    // 已穿时装：部位 → Fashion id（优先于 baseFashion）
    std::unordered_map<FashionPosition, int32_t> equipFashion;
};

class FashionResolver
{
public:
    // 外观 → 渲染描述：身体部位合并为 atlases，武器/翅膀/光环为挂件；失败返回 false
    static bool resolve(const FashionAppearance& appearance, AvatarDesc& out);
};

NS_MG_END
