#pragma once

#include "Fixed.h"

NS_MG_BEGIN

namespace FixedMath
{

Fixed sqrt(Fixed value);
Fixed sinDeg(Fixed degrees);
Fixed cosDeg(Fixed degrees);
Fixed atan2Deg(Fixed y, Fixed x);

}  // namespace FixedMath

NS_MG_END
