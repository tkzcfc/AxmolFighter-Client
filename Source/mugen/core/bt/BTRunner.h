#pragma once

#include "BTRegistry.h"

namespace mugen
{
// Transient execution view; never stored in a component or snapshot.
template <class BTCtx, class Param, class LeafState>
class BTRunner
{
public:
    BTRunner(const BTDef<Param>& def,
             const BTRegistry<BTCtx, Param, LeafState>& registry,
             BTInstance<LeafState>& instance)
        : m_def(def), m_registry(registry), m_instance(instance)
    {}

    bool enter(BTCtx& ctx)
    {
        m_instance.rootStopLogged = false;
        const bool running        = enterNode(ctx, 0);
        if (!running)
            reportStopped();
        return running;
    }
    bool update(BTCtx& ctx)
    {
        if (m_instance.nodes[0].status != BTStatus::Running)
            return false;
        updateNode(ctx, 0);
        if (m_instance.nodes[0].status == BTStatus::Running)
            return true;
        reportStopped();
        return false;
    }
    void exit(BTCtx& ctx) { exitNode(ctx, 0); }

private:
    bool enterNode(BTCtx& ctx, uint32_t id);
    void exitNode(BTCtx& ctx, uint32_t id);
    void exitChildren(BTCtx& ctx, uint32_t id);
    void updateNode(BTCtx& ctx, uint32_t id);
    BTStatus updateComposite(BTCtx& ctx, uint32_t id);
    BTStatus updateSerial(BTCtx& ctx, uint32_t id);
    bool conditionsHold(BTCtx& ctx, uint32_t id, bool entering);
    uint32_t child(uint32_t id, uint16_t ordinal) const { return m_def.topology().child(id, ordinal); }
    void reportStopped()
    {
        if (!m_instance.rootStopLogged)
        {
            MG_LOG_E("BT root stopped: status={}", static_cast<int>(m_instance.nodes[0].status));
            m_instance.rootStopLogged = true;
        }
    }

    const BTDef<Param>& m_def;
    const BTRegistry<BTCtx, Param, LeafState>& m_registry;
    BTInstance<LeafState>& m_instance;
};
}  // namespace mugen

#include "BTRunnerLifecycle.h"  // IWYU pragma: export
#include "BTRunnerUpdate.h"     // IWYU pragma: export
