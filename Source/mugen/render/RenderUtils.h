#include "mugen/core/Object.h"

NS_MG_BEGIN

#ifdef RUNTIME_IN_AXMOL

enum class SpriteType : int
{
    kSpriteType_Simple,
    kSpriteType_Tiled,
    kSpriteType_Sliced,
    kSpriteType_Filled,
};

namespace RenderUtils
{
void setupSpriteType(ax::Sprite* sprite, SpriteType type);
}

#endif
NS_MG_END
