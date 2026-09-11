#pragma once

#include "mugen/core/StdC.h"

#ifdef RUNTIME_IN_AXMOL

#    ifndef MG_SPINE_USE_3_4
#        define MG_SPINE_USE_3_4 0
#    endif

NS_MG_BEGIN

enum class MgSpineRuntime
{
    Axmol,
    Spine34,
};

NS_MG_END

#endif
