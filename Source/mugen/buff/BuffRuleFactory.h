#pragma once

#include "mugen/buff/BuffRuleBase.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

NS_MG_BEGIN

class BuffRuleFactory
{
public:
    using Creator = std::function<std::unique_ptr<BuffRuleBase>()>;

    static BuffRuleFactory& instance();

    void registerRule(const std::string& className, Creator creator);
    /** 按 className 取共享规则实例（惰性创建并缓存） */
    BuffRuleBase* get(const std::string& className);
    /** 注册本阶段核心规则 */
    void registerBuiltinRules();

private:
    BuffRuleFactory() = default;
    std::unordered_map<std::string, Creator> creators_;
    std::unordered_map<std::string, std::unique_ptr<BuffRuleBase>> cache_;
    bool builtinsRegistered_ = false;
};

NS_MG_END
