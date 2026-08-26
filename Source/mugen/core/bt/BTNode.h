#pragma once

#include "mugen/core/Object.h"

NS_MG_BEGIN

// 行为树节点状态
enum class BTStatus : int8_t
{
    // 节点已就绪，尚未进入
    Readied  = 0,
    // 节点正在运行中
    Running  = 1,
    // 节点运行成功
    Success  = 2,
    // 节点运行失败
    Failure  = 3,
};

struct BTContext;

// 行为树节点基类
class BTNode : public Object
{
public:
    typedef Object Super;

public:
    BTNode();

    virtual ~BTNode();

    virtual const char* typeName() const = 0;

    virtual void enter(BTContext& /*ctx*/) {}

    virtual void exit(BTContext& /*ctx*/) {}

    virtual void update(BTContext& /*ctx*/, int32_t /*dtMs*/) {}

public:
    BTNode* parent  = nullptr;
    BTStatus status = BTStatus::Readied;

public:
    MG_DEFINE_SERIALIZABLE(status)
};

NS_MG_END
