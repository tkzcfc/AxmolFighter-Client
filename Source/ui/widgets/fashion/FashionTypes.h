#pragma once

#include "mugen/conf/GameDef.h"

#include <optional>

namespace gameui
{

// Opens FashionMainPanel by default. Pass initialPart to jump straight into dress-up
// for that body part (nullopt with openDress=true opens Suit tab).
struct FashionOpenParams
{
    bool openDress = false;
    std::optional<mugen::FashionPosition> initialPart;
};

}  // namespace gameui
