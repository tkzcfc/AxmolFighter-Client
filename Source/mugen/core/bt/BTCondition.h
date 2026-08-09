#pragma once

#include "mugen/core/bt/BTNode.h"

#include <string>

NS_MG_BEGIN

class BTCondition : public Object
{
public:
    typedef Object Super;

    BTCondition() {}
    virtual ~BTCondition() {}

    virtual bool check(BTContext& ctx) = 0;
    virtual void onEnter(BTContext& /*ctx*/) {}
    virtual void onExit(BTContext& /*ctx*/) {}

    std::string debugName;
};

NS_MG_END
