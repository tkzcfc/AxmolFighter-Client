#pragma once

#include "mugen/core/StdC.h"

#ifdef RUNTIME_IN_AXMOL

#    ifndef MG_SPINE_USE_3_4
#        define MG_SPINE_USE_3_4 1
#    endif

#    if MG_SPINE_USE_3_4
#        include "spine_3_4/spine-cocos2dx.h"
#    else
#        include "spine/spine-axmol.h"
#    endif

#    include <limits>

NS_MG_BEGIN

#    if MG_SPINE_USE_3_4
using MgSkeletonAnimation = spine34::SkeletonAnimation;
using MgSkeletonData      = spSkeletonData;
using MgAtlas             = spAtlas;
using MgAttachmentLoader  = spAttachmentLoader;
using MgTrackEntry        = spTrackEntry;
using MgAnimation         = spAnimation;
#    else
using MgSkeletonAnimation = spine::SkeletonAnimation;
using MgSkeletonData      = spine::SkeletonData;
using MgAtlas             = spine::Atlas;
using MgAttachmentLoader  = spine::AttachmentLoader;
using MgTrackEntry        = spine::TrackEntry;
using MgAnimation         = spine::Animation;
#    endif

inline MgSkeletonData* skeletonDataOf(MgSkeletonAnimation* skel)
{
#    if MG_SPINE_USE_3_4
    return skel->getSkeleton()->data;
#    else
    return skel->getSkeleton()->getData();
#    endif
}

inline MgAnimation* findAnimation(MgSkeletonData* data, const char* name)
{
#    if MG_SPINE_USE_3_4
    return spSkeletonData_findAnimation(data, name);
#    else
    return data->findAnimation(name);
#    endif
}

inline float animationDuration(MgAnimation* anim)
{
#    if MG_SPINE_USE_3_4
    return anim->duration;
#    else
    return anim->getDuration();
#    endif
}

inline const char* animationName(MgAnimation* anim)
{
#    if MG_SPINE_USE_3_4
    return anim->name;
#    else
    return anim->getName().buffer();
#    endif
}

inline int animationCount(MgSkeletonData* data)
{
#    if MG_SPINE_USE_3_4
    return data->animationsCount;
#    else
    return static_cast<int>(data->getAnimations().size());
#    endif
}

inline MgAnimation* animationAt(MgSkeletonData* data, int index)
{
#    if MG_SPINE_USE_3_4
    return data->animations[index];
#    else
    return data->getAnimations()[static_cast<size_t>(index)];
#    endif
}

inline bool hasSkin(MgSkeletonData* data, const char* name)
{
#    if MG_SPINE_USE_3_4
    return spSkeletonData_findSkin(data, name) != nullptr;
#    else
    return data->findSkin(name) != nullptr;
#    endif
}

inline void keepTrackAlive(MgTrackEntry* track)
{
#    if MG_SPINE_USE_3_4
    track->endTime = std::numeric_limits<float>::max();
#    else
    track->setTrackEnd(std::numeric_limits<float>::max());
#    endif
}

inline void seekTrack(MgTrackEntry* track, float tSec)
{
#    if MG_SPINE_USE_3_4
    track->time     = tSec;
    track->lastTime = tSec;
#    else
    track->setTrackTime(tSec);
    track->setAnimationLast(tSec);
#    endif
}

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
