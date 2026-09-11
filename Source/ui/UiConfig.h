#pragma once

#include "mugen/conf/GameDef.h"
#include <cstdint>

namespace gameui
{

// 当前开放的角色数量
inline constexpr int32_t kOpenCharacterCount = 4;

// spine预览配置
struct SpinePreview
{
    // 展示用 spine 资源 id
    int32_t id                = 0;
    float scaleX              = 1.0f;
    float scaleY              = 1.0f;
    float offsetX             = 0.0f;
    float offsetY             = 0.0f;
    const char* animationName = "";
};

struct Profession
{
    // 职业名称
    const char* name = "";
    // 难度,范围 [1, 5]
    int difficulty = 1;
    // 职业描述
    const char* description = "";
    // 职业名图片
    const char* icon = "";
    // 角色模型预览
    SpinePreview spine;
    // 视频
    const char* video = "";
};

// 职业数量 (基础职业+2个可转职职业)
inline constexpr int kProfessionCount = 3;
// 角色职业介绍
struct RoleIntro
{
    // 角色职业列表
    Profession professions[kProfessionCount];
    // 职业属性图片
    const char* attributeIcon = "";
};

// 创建角色界面的外观配置选项数量
inline constexpr int32_t kCreateRoleVariantCount = 5;
// 创建角色界面的外观配置选项
struct LookSet
{
    // 头发图标
    const char* hairIcons[kCreateRoleVariantCount];
    // 头发对应的 res_fashion id
    int32_t hairIds[kCreateRoleVariantCount];
    // 服饰图标
    const char* clothIcons[kCreateRoleVariantCount];
    // 服饰对应的 res_fashion id
    int32_t clothIds[kCreateRoleVariantCount];
    // 皮肤对应的 res_fashion id
    int32_t skinIds[kCreateRoleVariantCount];
};

// 角色职业介绍配置
extern const RoleIntro kRoleIntros[kOpenCharacterCount];
// 创建角色界面的外观配置选项
extern const LookSet kLookIcons[kOpenCharacterCount];

}  // namespace gameui
