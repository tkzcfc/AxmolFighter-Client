#pragma once

#include "mugen/core/StdC.h"
#include "mugen/avatar/FashionSpine.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class Avatar;
class AvatarComponent;

class AvatarBuilder
{
public:
    // 战斗单位：同步 + 共享缓存
    static Avatar* createAvatar(const AvatarComponent* avatarComp);
    // 固定外观展示（如变身）：asyncLoad 决定数据归属（同步=共享缓存；异步=实例私有）
    static Avatar* createAvatar(const FashionSpineDesc& desc, bool asyncLoad);
    // 外观驱动角色（UI）：默认异步（换装需实例数据）；同步则共享缓存、setFashion 换装不可用
    static Avatar* createAvatar(const FashionAppearance& appearance, bool asyncLoad = true);
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
