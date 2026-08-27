#pragma once

#include "mugen/core/StdC.h"

NS_MG_BEGIN

namespace BTTypeRegistry
{

// 向 BTFactory 注册全部节点与条件类型名。GameWord::init 时调用。
void registerAll();

}  // namespace BTTypeRegistry

NS_MG_END
