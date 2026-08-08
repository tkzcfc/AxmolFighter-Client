#pragma once

#include "mugen/core/StdC.h"
#include "mugen/conf/GameDef.h"
#include "mugen/avatar/render/SpineLayer.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class Avatar;
class AvatarComponent;

class AvatarBuilder
{
public:
    static Avatar* createAvatar(const AvatarComponent* avatarComp);
    static Avatar* createAvatar(const SpineAvatarDesc& desc);
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
