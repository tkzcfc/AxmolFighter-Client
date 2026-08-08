#pragma once

#include "mugen/component/TransformComponent.h"
#include "mugen/core/math/DamageBox.h"

#include <algorithm>
#include <cmath>

NS_MG_BEGIN

namespace combat_box
{

// 单轴本地坐标 → 世界坐标（含缩放与镜像）
inline void transformBoxAxis(int32_t localPos,
                             int32_t localSize,
                             float scale,
                             int32_t worldOrigin,
                             int32_t& outPos,
                             int32_t& outSize)
{
    auto start = static_cast<int32_t>(std::lround(static_cast<float>(localPos) * scale)) + worldOrigin;
    auto end   = static_cast<int32_t>(std::lround(static_cast<float>(localPos + localSize) * scale)) + worldOrigin;

    outPos  = std::min(start, end);
    outSize = std::max(start, end) - outPos;
}

// 本地判定盒 → 世界坐标（scale.x < 0 时水平镜像）
// contentScale：内容缩放（如 ResSpineConfig::scale / AvatarComponent::spineScale），与渲染骨骼 setScale 一致；.box
// 存的是未缩放骨架空间
inline DamageBox transformDamageBoxToWorld(const DamageBox& box,
                                           const TransformComponent* transformComp,
                                           float contentScale = 1.0f)
{
    if (transformComp == nullptr)
        return box;

    if (!(contentScale > 0.0f))
        contentScale = 1.0f;

    const float scaleX = transformComp->scale.x * contentScale;
    const float scaleY = transformComp->scale.y * contentScale;

    DamageBox worldBox = box;
    transformBoxAxis(box.pos.x, box.size.x, scaleX, transformComp->position.x, worldBox.pos.x, worldBox.size.x);
    transformBoxAxis(box.pos.y, box.size.y, scaleY, transformComp->position.y, worldBox.pos.y, worldBox.size.y);
    transformBoxAxis(box.pos.z, box.size.z, scaleY, transformComp->position.y + transformComp->position.z,
                     worldBox.pos.z, worldBox.size.z);
    return worldBox;
}

}  // namespace combat_box

NS_MG_END
