#pragma once

#include "mugen/core/StdC.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

// 角色外观定义
struct FashionAppearance
{
    // 角色id
    int32_t roleId = 0;

    // 角色基础时装配置
    std::unordered_map<FashionPosition, int32_t> baseFashion;

    // 已穿的时装配置
    std::unordered_map<FashionPosition, int32_t> equipFashion;
};

struct FashionSpineDesc
{
    bool valid = false;
    // spine 资源路径
    std::string skeleton;
    // spine 资源图集路径
    std::vector<std::string> atlases;
    // spine 皮肤名
    std::string skin;
    // spine 缩放比例
    float scale = 1.0f;
    // 动画文件
    std::string motionFile;

    // 各部位在 atlases 中的索引，-1 表示这个部位没有对应的 atlas
    std::array<int32_t, static_cast<size_t>(FashionPosition::kCount)> atlasSlotIndex = {};
    // 武器id
    int32_t weaponSpineId = 0;
    // 翅膀id
    int32_t wingSpineId = 0;
    // 光环id
    int32_t ringSpineId = 0;
};

class FashionResolver
{
public:
    static FashionSpineDesc resolve(const FashionAppearance& appearance);
};

NS_MG_END
