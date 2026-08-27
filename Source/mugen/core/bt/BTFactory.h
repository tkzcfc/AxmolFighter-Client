#pragma once

#include "mugen/core/bt/BTCondition.h"
#include "mugen/core/bt/BTNode.h"

#include <functional>
#include <string>
#include <unordered_map>

NS_MG_BEGIN

using BTNodeCreateFunc      = std::function<BTNode*()>;
using BTConditionCreateFunc = std::function<BTCondition*()>;

class BTFactory
{
    static BTFactory* s_instance;

public:
    BTFactory();
    ~BTFactory();

    static BTFactory* getInstance();
    static void destroyInstance();

    // 注册节点创建函数，name 须与 typeName() 一致
    void registerNode(const std::string& name, const BTNodeCreateFunc& createFunc);

    // 注册条件创建函数，name 须与 typeName() 一致
    void registerCondition(const std::string& name, const BTConditionCreateFunc& createFunc);

    bool hasNode(const std::string& name) const;
    bool hasCondition(const std::string& name) const;

    BTNode* spawnNode(const std::string& name);

    BTCondition* spawnCondition(const std::string& name);

private:
    std::unordered_map<std::string, BTNodeCreateFunc> m_nodeCreators;
    std::unordered_map<std::string, BTConditionCreateFunc> m_conditionCreators;
};

NS_MG_END
