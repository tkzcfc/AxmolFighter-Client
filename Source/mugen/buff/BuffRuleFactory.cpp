#include "mugen/buff/BuffRuleFactory.h"
#include "mugen/buff/BuffRules.h"

NS_MG_BEGIN

BuffRuleFactory& BuffRuleFactory::instance()
{
    static BuffRuleFactory s;
    return s;
}

void BuffRuleFactory::registerRule(const std::string& className, Creator creator)
{
    creators_[className] = std::move(creator);
}

BuffRuleBase* BuffRuleFactory::get(const std::string& className)
{
    if (className.empty())
        return nullptr;
    if (!builtinsRegistered_)
        registerBuiltinRules();

    auto it = cache_.find(className);
    if (it != cache_.end())
        return it->second.get();

    auto cit = creators_.find(className);
    if (cit == creators_.end())
        return nullptr;

    auto rule = cit->second();
    BuffRuleBase* raw = rule.get();
    cache_[className] = std::move(rule);
    return raw;
}

void BuffRuleFactory::registerBuiltinRules()
{
    if (builtinsRegistered_)
        return;
    builtinsRegistered_ = true;

    registerRule("BuffInvincible", []() { return std::make_unique<BuffRuleInvincible>(); });
    registerRule("BuffSuperArmor", []() { return std::make_unique<BuffRuleSuperArmor>(); });

    auto periodic = []() { return std::make_unique<BuffRulePeriodicHurt>(); };
    registerRule("BuffBurn", periodic);
    registerRule("BuffPoison", periodic);
    registerRule("BuffBleeding", periodic);
    registerRule("BuffPeriodicHurt", periodic);
    registerRule("BuffBurn2", periodic);
    registerRule("BuffPurplePoison", periodic);

    registerRule("BuffDamageHurt", []() { return std::make_unique<BuffRuleDamageHurt>(); });
    registerRule("BuffDamageReduction", []() { return std::make_unique<BuffRuleDamageReduction>(); });
    registerRule("BuffBreakArmor", []() { return std::make_unique<BuffRuleDamageReduction>(); });
    registerRule("BuffDamageSlot", []() { return std::make_unique<BuffRuleDamageSlot>(); });
    registerRule("BuffCDSkill", []() { return std::make_unique<BuffRuleCDSkill>(); });
    registerRule("BuffModifyCDSkill", []() { return std::make_unique<BuffRuleModifyCDSkill>(); });
    registerRule("BuffTPConsumeScale", []() { return std::make_unique<BuffRuleTPConsumeScale>(); });
    registerRule("BuffEPConsumeScale", []() { return std::make_unique<BuffRuleEPConsumeScale>(); });
    registerRule("BuffCrit", []() { return std::make_unique<BuffRuleCrit>(); });
    registerRule("BuffHPMAX", []() { return std::make_unique<BuffRuleHPMAX>(); });
    registerRule("BuffHP", []() { return std::make_unique<BuffRuleHP>(); });
}

NS_MG_END
