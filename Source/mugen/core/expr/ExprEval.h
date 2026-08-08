#pragma once

#include "mugen/core/StdC.h"
#include "3rd/tinyexpr/tinyexpr.h"
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cmath>

NS_MG_BEGIN

/// tinyexpr 的通用 C++ 封装。
///
/// 功能：
///   1. 动态注册具名变量（double 值，运行时可更新）
///   2. 编译表达式后缓存，对同一表达式多次求值无重复解析开销
///   3. 线程安全：每个 ExprEval 实例独立，不共享全局状态
///
/// 最小用例：
///   ExprEval eval;
///   eval.setVar("moveSpeed", 200.0);
///   eval.setVar("scale",     0.3);
///   float v = eval.eval("moveSpeed * scale + 50");  // = 110
///
/// 批量更新属性后求多个表达式：
///   eval.setVar("jumpSpeed", attr.jumpSpeed);
///   float vz = eval.eval("jumpSpeed * 1.2");
///   float vx = eval.eval("moveSpeed * 0.25");

class ExprEval
{
public:
    ExprEval() = default;

    // 禁止拷贝：内部持有 te_variable 指针数组，拷贝会导致悬空指针
    ExprEval(const ExprEval&)            = delete;
    ExprEval& operator=(const ExprEval&) = delete;

    // 允许移动
    ExprEval(ExprEval&&)            = default;
    ExprEval& operator=(ExprEval&&) = default;

    ~ExprEval() { clearCache(); }

    // ──────────────────────────────────────────────────────────────────────
    // 变量管理
    // ──────────────────────────────────────────────────────────────────────

    /// 注册或更新一个变量。
    /// 注意：变量名新增时会使表达式缓存全部失效（变量表结构改变）。
    ///       只是更新已有变量的值时不会失效，可以高频调用。
    void setVar(std::string_view name, double value)
    {
        auto it = m_varIndex.find(std::string(name));
        if (it != m_varIndex.end())
        {
            // 只更新值，指针不变，缓存有效
            m_varValues[it->second] = value;
        }
        else
        {
            // 新变量：重建变量表，清除缓存
            m_varNames.emplace_back(name);
            m_varValues.push_back(value);
            m_varIndex[std::string(name)] = static_cast<int>(m_varNames.size() - 1);
            rebuildVarTable();
            clearCache();
        }
    }

    /// 批量设置变量（减少重建次数）。
    void setVars(std::initializer_list<std::pair<std::string_view, double>> vars)
    {
        bool hasNew = false;
        for (auto& [name, value] : vars)
        {
            auto it = m_varIndex.find(std::string(name));
            if (it != m_varIndex.end())
            {
                m_varValues[it->second] = value;
            }
            else
            {
                m_varNames.emplace_back(name);
                m_varValues.push_back(value);
                m_varIndex[std::string(name)] = static_cast<int>(m_varNames.size() - 1);
                hasNew                        = true;
            }
        }
        if (hasNew)
        {
            rebuildVarTable();
            clearCache();
        }
    }

    // ──────────────────────────────────────────────────────────────────────
    // 求值
    // ──────────────────────────────────────────────────────────────────────

    /// 对表达式求值，返回 float。
    /// 首次调用时编译并缓存，后续调用直接使用缓存的 te_expr。
    /// 表达式非法时返回 fallback（默认 0）并打印警告。
    float eval(const std::string& expr, float fallback = 0.0f) const
    {
        if (expr.empty())
            return fallback;

        // 查缓存
        auto it = m_cache.find(expr);
        if (it != m_cache.end())
        {
            return static_cast<float>(te_eval(it->second));
        }

        // 编译并缓存
        int err    = 0;
        te_expr* e = te_compile(expr.c_str(), m_teVars.data(), static_cast<int>(m_teVars.size()), &err);
        if (!e)
        {
            MG_LOG_W("ExprEval: compile failed for '{}' (err pos={})", expr, err);
            return fallback;
        }

        m_cache.emplace(expr, e);
        return static_cast<float>(te_eval(e));
    }

    /// 判断表达式是否合法（不求值）。
    bool isValid(const std::string& expr) const
    {
        if (expr.empty())
            return true;
        int err    = 0;
        te_expr* e = te_compile(expr.c_str(), m_teVars.data(), static_cast<int>(m_teVars.size()), &err);
        if (!e)
            return false;
        te_free(e);
        return true;
    }

    /// 清空编译缓存（变量表结构变化后自动调用，通常不需要手动调用）。
    void clearCache()
    {
        for (auto& [key, expr] : m_cache)
            te_free(expr);
        m_cache.clear();
    }

private:
    // 根据 m_varNames / m_varValues 重建 te_variable 数组。
    // 必须在 m_varValues 地址稳定之后调用（vector resize 会使旧地址失效）。
    void rebuildVarTable()
    {
        // 先预留足够容量，防止后续 push_back 触发 realloc 使指针失效
        m_varValues.reserve(m_varValues.size() + 16);

        m_teVars.clear();
        m_teVars.reserve(m_varNames.size());
        for (size_t i = 0; i < m_varNames.size(); ++i)
        {
            te_variable v{};
            v.name    = m_varNames[i].c_str();
            v.address = &m_varValues[i];  // 指向 vector 内的 double
            v.type    = TE_VARIABLE;
            v.context = nullptr;
            m_teVars.push_back(v);
        }
    }

private:
    std::vector<std::string> m_varNames;              // 变量名（与 m_varValues 下标对应）
    std::vector<double> m_varValues;                  // 变量当前值
    std::unordered_map<std::string, int> m_varIndex;  // 名称 → 下标，O(1) 查找
    std::vector<te_variable> m_teVars;                // tinyexpr 变量表（指针指向 m_varValues）

    mutable std::unordered_map<std::string, te_expr*> m_cache;  // 编译缓存
};

NS_MG_END
