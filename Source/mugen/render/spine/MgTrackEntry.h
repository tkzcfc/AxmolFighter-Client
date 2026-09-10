#pragma once

#include "mugen/render/spine/MgSpineRuntime.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class MgTrackEntry
{
public:
    MgTrackEntry() = default;
    MgTrackEntry(MgSpineRuntime runtime, void* native, int trackIndex, const char* animationName)
        : m_runtime(runtime)
        , m_native(native)
        , m_trackIndex(trackIndex)
        , m_animationName(animationName ? animationName : "")
    {}

    explicit operator bool() const { return m_native != nullptr; }
    bool valid() const { return m_native != nullptr; }
    MgSpineRuntime runtime() const { return m_runtime; }
    int trackIndex() const { return m_trackIndex; }
    const char* animationName() const { return m_animationName; }

private:
    MgSpineRuntime m_runtime    = MgSpineRuntime::Axmol;
    void* m_native              = nullptr;
    int m_trackIndex            = 0;
    const char* m_animationName = "";
};

NS_MG_END

#endif
