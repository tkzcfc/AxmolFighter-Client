#pragma once

#include "mugen/core/StdC.h"
#include "mugen/sim_legacy/World.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class AvatarPresenter;

class DebugDraw
{
public:
    explicit DebugDraw(ax::Node& root);
    // avatars 提供与角色相同的插值偏移；为空时画在逻辑坐标上。
    void present(sim_legacy::World& world, const AvatarPresenter* avatars, float alpha);
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool enabled() const { return m_enabled; }

private:
    ax::DrawNode* m_node = nullptr;
    bool m_enabled       = false;
};

NS_MG_END

#endif
