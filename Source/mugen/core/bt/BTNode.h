#pragma once

#include "mugen/core/Object.h"

#include <string>

NS_MG_BEGIN

enum class BTStatus : int8_t
{
    Readied  = 0,
    Running  = 1,
    Success  = 2,
    Failure  = 3,
};

struct BTContext;

class BTNode : public Object
{
public:
    typedef Object Super;

    BTNode() {}
    virtual ~BTNode() {}

    virtual void onEnter(BTContext& /*ctx*/) {}
    virtual void onExit(BTContext& /*ctx*/) {}
    virtual BTStatus tick(BTContext& ctx, int32_t dtMs) = 0;

    /** 组合节点：条件复检；叶节点默认 true */
    virtual bool checkAll(BTContext& /*ctx*/) { return true; }

    std::string debugName;
};

NS_MG_END
