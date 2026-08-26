#include "State.h"
#include "Transition.h"

NS_MG_BEGIN

using StateCreateFuncType      = std::function<State*()>;
using TransitionCreateFuncType = std::function<Transition*()>;

class FSMFactory
{
    static FSMFactory* s_instance;

public:
    FSMFactory();

    ~FSMFactory();

    static FSMFactory* getInstance();

    static void destroyInstance();

    // 注册状态
    void registerState(const std::string& name, const StateCreateFuncType& createFunc);

    // 是否已存在状态创建器
    bool hasState(const std::string& name) const;

    // 注册转换器
    void registerTransition(const std::string& name, const TransitionCreateFuncType& createFunc);

    // 生成状态
    State* spwanState(const std::string& stateName);

    // 生成转换器
    Transition* spwanTransition(const std::string& transitionName);

private:
    std::unordered_map<std::string, StateCreateFuncType> m_statesCreaterMap;
    std::unordered_map<std::string, TransitionCreateFuncType> m_transitionsCreaterMap;
};

NS_MG_END
