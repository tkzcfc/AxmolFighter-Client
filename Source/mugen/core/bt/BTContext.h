#pragma once

#include "mugen/core/StdC.h"

NS_MG_BEGIN

class Entity;

// 行为树节点执行时的上下文
struct BTContext
{
    // 当前执行的实体对象
    Entity* entity = nullptr;
};

NS_MG_END
