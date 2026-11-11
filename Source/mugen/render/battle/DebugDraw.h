#pragma once

#include "mugen/core/StdC.h"
#include "mugen/sim_legacy/World.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class DebugDraw
{
public:
    explicit DebugDraw(ax::Node& root);
    void present(sim_legacy::World& world);
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool enabled() const { return m_enabled; }

private:
    ax::DrawNode* m_node = nullptr;
    bool m_enabled       = false;
};

NS_MG_END

#endif
