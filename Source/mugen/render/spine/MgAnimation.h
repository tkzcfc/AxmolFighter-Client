#pragma once

#include "mugen/render/spine/MgSpineRuntime.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class MgAnimation
{
public:
    MgAnimation() = default;
    MgAnimation(MgSpineRuntime runtime, void* native, const char* name, float duration)
        : m_runtime(runtime), m_native(native), m_name(name ? name : ""), m_duration(duration)
    {}

    explicit operator bool() const { return m_native != nullptr; }
    bool valid() const { return m_native != nullptr; }
    MgSpineRuntime runtime() const { return m_runtime; }
    const char* name() const { return m_name; }
    float duration() const { return m_duration; }

private:
    MgSpineRuntime m_runtime = MgSpineRuntime::Axmol;
    void* m_native           = nullptr;
    const char* m_name       = "";
    float m_duration         = 0.0f;
};

NS_MG_END

#endif
