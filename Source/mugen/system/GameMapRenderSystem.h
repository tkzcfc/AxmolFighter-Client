#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

#ifdef RUNTIME_IN_AXMOL
class VirtualCamera;
#endif

class GameMapRenderSystem : public System
{
public:
    typedef System Super;

public:
    GameMapRenderSystem();

    virtual ~GameMapRenderSystem();

    virtual void init(ECSManager* ecs) override;

    virtual void update() override;

    virtual void onEntityAdded(Entity* entity) override;

    virtual void onEntityRemoved(Entity* entity) override;

#ifdef RUNTIME_IN_AXMOL
    bool bindToGameMapRenderComponent(Entity* entity, ax::ParallaxNode* node, std::unique_ptr<VirtualCamera> camera);
#endif
};

NS_MG_END
