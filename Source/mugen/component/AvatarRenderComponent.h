#pragma once

#include "mugen/core/ecs/Component.h"

#ifdef RUNTIME_IN_AXMOL
#    include "mugen/avatar/render/Avatar.h"
#    include "base/RefPtr.h"
#endif

NS_MG_BEGIN

class AvatarRenderComponent : public Component
{
public:
    typedef Component Super;

public:
    AvatarRenderComponent() {}

    virtual ~AvatarRenderComponent()
    {
#ifdef RUNTIME_IN_AXMOL
        avatar = nullptr;
#endif
    }

#ifdef RUNTIME_IN_AXMOL
    Avatar* avatar = nullptr;
    // 已同步到渲染层的动作（供 AvatarRenderSystem 检测逻辑动作变化，驱动 setMotion）
    std::string syncedMotion;
    std::string syncedEntry;
#endif
};

NS_MG_END
