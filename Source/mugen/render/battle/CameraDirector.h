#pragma once

#include "mugen/core/StdC.h"
#include "mugen/sim_legacy/World.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/battle/MapPresenter.h"

NS_MG_BEGIN

class CameraDirector
{
public:
    explicit CameraDirector(MapPresenter& map) : m_map(map) {}

    void follow(sim_legacy::World& world, Entity entity);
    void consume(const sim_legacy::PresentationEvent& event);
    void update(float delta);

private:
    MapPresenter& m_map;
    float m_displaySlowScale = 1.0f;
};

NS_MG_END

#endif
