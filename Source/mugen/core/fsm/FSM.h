#pragma once

#include "EventData.h"
#include "State.h"

NS_MG_BEGIN

using StateCreateFuncType      = std::function<State*()>;
using TransitionCreateFuncType = std::function<Transition*()>;

class FSM : public Object
{
public:
    typedef Object Super;

public:
    FSM();

    virtual ~FSM();

    // 添加状态
    State* addState(const std::string& stateName);

    // 通过状态名称切换到某个状态
    bool changeToStateByName(const std::string& stateName);

    // 重置当前状态的运行计时器
    void resetRunStateTime();

    // 设置默认状态
    void setEntryStateByName(const std::string& stateName);

    // 发射事件
    void progressEvent(const std::string& evetName, const EventData& eventData = {});

    // 状态机更新
    void update(int32_t ms);

    // 重置状态机运行时与状态集合
    void reset();

    // 通过名称获取状态
    State* getStateByKey(const std::string& stateName) const;

    // 设置任意状态
    void setAnyState(const std::string& stateName);

    // 获取当前状态名称
    std::string_view getCurStateName() const;

    // 获取上一个状态名称
    std::string_view getPreStateName() const;

    // 获取任意状态名称
    std::string_view getAnyStateName() const;

protected:
    // 切换到某个状态
    void changeToState(State* state);

public:
    virtual void serialize(ByteBuffer& byteBuffer) const override;

    virtual bool deserialize(ByteBuffer& byteBuffer) override;

    // 反序列化完成之后还原状态机的运行时数据
    void restoreRuntimeData();

private:
    std::unordered_map<std::string, State*> m_statesDict;

    MG_SYNTHESIZE_READONLY(State*, m_curState, CurState);
    MG_SYNTHESIZE_READONLY(State*, m_preState, PreState);
    MG_SYNTHESIZE_READONLY(State*, m_anyState, AnyState);
    MG_SYNTHESIZE_READONLY(uint32_t, m_runStateTime, RunStateTime);
    MG_SYNTHESIZE_READONLY(int32_t, m_frameTime, FrameTime);
    MG_SYNTHESIZE(void*, m_userData, UserData);

    struct DeserializeContext
    {
        std::string preStateName;
        std::string curStateName;
        std::string anyStateName;
    };
    std::unique_ptr<DeserializeContext> m_deserializeContext;
};

NS_MG_END
