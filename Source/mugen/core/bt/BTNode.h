#pragma once

#include "mugen/core/Object.h"

NS_MG_BEGIN

// 行为树节点状态
enum class BTStatus : int8_t
{
    // 节点已就绪，尚未进入
    Readied = 0,
    // 节点正在运行中
    Running = 1,
    // 节点运行成功
    Success = 2,
    // 节点运行失败
    Failure = 3,
};

struct BTContext;

class BTComposite;
class BehaviorTree;

// 行为树节点基类
class BTNode : public Object
{
public:
    typedef Object Super;

public:
    BTNode();

    virtual ~BTNode();

    virtual const char* typeName() const = 0;

    bool enter(BTContext& ctx);

    void exit(BTContext& ctx);

    void update(BTContext& ctx, int32_t dtMs);

protected:
    virtual bool onEnter(BTContext& ctx) = 0;

    virtual BTStatus onUpdate(BTContext& ctx, int32_t dtMs) = 0;

    virtual void onExit(BTContext& /*ctx*/) {}

public:
    MG_SYNTHESIZE_READONLY(BTNode*, m_parent, Parent)
    MG_SYNTHESIZE_READONLY(BTStatus, m_status, Status)

    bool isRunning() const { return m_status == BTStatus::Running; }
    bool isSuccess() const { return m_status == BTStatus::Success; }
    bool isFailure() const { return m_status == BTStatus::Failure; }

public:
    MG_DEFINE_SERIALIZABLE(m_status)

    friend class BTComposite;
    friend class BehaviorTree;
};

NS_MG_END
