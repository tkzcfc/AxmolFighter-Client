#pragma once

#ifndef __MACRO_DEFINITION_H__
#    define __MACRO_DEFINITION_H__

#    define NS_MG_BEGIN \
        namespace mugen \
        {
#    define NS_MG_END   }
#    define USING_NS_MG using mugen
#    define NS_MG       ::mugen

// MG_FORCEINLINE
#    define MG_FORCEINLINE       inline

#    define MG_SYNTHESIZE_HEADER MG_FORCEINLINE
// #define MG_SYNTHESIZE_HEADER virtual MG_FORCEINLINE

#    define MG_SYNTHESIZE_REF(varType, varName, funName) \
    protected:                                           \
        varType varName;                                 \
                                                         \
    public:                                              \
        MG_SYNTHESIZE_HEADER varType& get##funName(void) \
        {                                                \
            return varName;                              \
        }

#    define MG_SYNTHESIZE_REF_PTR(varType, varName, funName) \
    protected:                                               \
        varType varName;                                     \
                                                             \
    public:                                                  \
        MG_SYNTHESIZE_HEADER varType* get##funName(void)     \
        {                                                    \
            return &varName;                                 \
        }

#    define MG_SYNTHESIZE(varType, varName, funName)          \
    protected:                                                \
        varType varName;                                      \
                                                              \
    public:                                                   \
        MG_SYNTHESIZE_HEADER varType get##funName(void) const \
        {                                                     \
            return varName;                                   \
        }                                                     \
        MG_SYNTHESIZE_HEADER void set##funName(varType var)   \
        {                                                     \
            varName = var;                                    \
        }

#    define MG_SYNTHESIZE_READONLY(varType, varName, funName) \
    protected:                                                \
        varType varName;                                      \
                                                              \
    public:                                                   \
        MG_SYNTHESIZE_HEADER varType get##funName(void) const \
        {                                                     \
            return varName;                                   \
        }

#    define MG_SYNTHESIZE_WRITEONLY(varType, varName, funName) \
    protected:                                                 \
        varType varName;                                       \
                                                               \
    public:                                                    \
        MG_SYNTHESIZE_HEADER void set##funName(varType var)    \
        {                                                      \
            varName = var;                                     \
        }

#    define MG_SYNTHESIZE_IS(varType, varName, funName)      \
    protected:                                               \
        varType varName;                                     \
                                                             \
    public:                                                  \
        MG_SYNTHESIZE_HEADER varType is##funName(void) const \
        {                                                    \
            return varName;                                  \
        }                                                    \
        MG_SYNTHESIZE_HEADER void set##funName(varType var)  \
        {                                                    \
            varName = var;                                   \
        }

#    define MG_SYNTHESIZE_IS_READONLY(varType, varName, funName) \
    protected:                                                   \
        varType varName;                                         \
                                                                 \
    public:                                                      \
        MG_SYNTHESIZE_HEADER varType is##funName(void) const     \
        {                                                        \
            return varName;                                      \
        }

#    define MG_SYNTHESIZE_PASS_BY_REF(varType, varName, funName)     \
    protected:                                                       \
        varType varName;                                             \
                                                                     \
    public:                                                          \
        MG_SYNTHESIZE_HEADER const varType& get##funName(void) const \
        {                                                            \
            return varName;                                          \
        }                                                            \
        MG_SYNTHESIZE_HEADER void set##funName(const varType& var)   \
        {                                                            \
            varName = var;                                           \
        }

#    define MG_SYNTHESIZE_READONLY_BY_REF(varType, varName, funName) \
    protected:                                                       \
        varType varName;                                             \
                                                                     \
    public:                                                          \
        MG_SYNTHESIZE_HEADER const varType& get##funName(void) const \
        {                                                            \
            return varName;                                          \
        }

#    define MG_SYNTHESIZE_WRITEONLY_BY_REF(varType, varName, funName) \
    protected:                                                        \
        varType varName;                                              \
                                                                      \
    public:                                                           \
        MG_SYNTHESIZE_HEADER void set##funName(const varType& var)    \
        {                                                             \
            varName = var;                                            \
        }

#    define MG_PROPERTY(varType, varName, funName) \
    protected:                                     \
        varType varName;                           \
                                                   \
    public:                                        \
        virtual varType get##funName(void) const;  \
        virtual void set##funName(varType var);

#    define MG_PROPERTY_READONLY(varType, varName, funName) \
    protected:                                              \
        varType varName;                                    \
                                                            \
    public:                                                 \
        virtual varType get##funName(void) const;

#    define MG_PROPERTY_WRITEONLY(varType, varName, funName) \
    protected:                                               \
        varType varName;                                     \
                                                             \
    public:                                                  \
        virtual void set##funName(varType var);

#    define MG_PROPERTY_PASS_BY_REF(varType, varName, funName) \
    protected:                                                 \
        varType varName;                                       \
                                                               \
    public:                                                    \
        virtual const varType& get##funName(void) const;       \
        virtual void set##funName(const varType& var);

#    define MG_PROPERTY_READONLY_BY_REF(varType, varName, funName) \
    protected:                                                     \
        varType varName;                                           \
                                                                   \
    public:                                                        \
        virtual const varType& get##funName(void) const;

#    define MG_PROPERTY_WRITEONLY_BY_REF(varType, varName, funName) \
    protected:                                                      \
        varType varName;                                            \
                                                                    \
    public:                                                         \
        virtual void set##funName(const varType& var);

#    define MG_SAFE_DELETE(p) \
        do                    \
        {                     \
            delete (p);       \
            (p) = nullptr;    \
        } while (0)

#    define MG_SAFE_DELETE_ARRAY(p) \
        do                          \
        {                           \
            if (p)                  \
            {                       \
                delete[] (p);       \
                (p) = nullptr;      \
            }                       \
        } while (0)

#    define MG_BREAK_IF(cond) \
        if (cond)             \
        break

// 设置标记
#    define MG_BIT_SET(b, flag) (b) |= (flag)
// 移除标记
#    define MG_BIT_REMOVE(b, flag) (b) &= ~(flag)
// 存在任意一个
#    define MG_BIT_HAS_ANY(b, flag) (((b) & (flag)) != 0)
// 存在全部
#    define MG_BIT_HAS_ALL(b, flag) (((b) & (flag)) == (flag))
// 不存在任何标记
#    define MG_BIT_HAS_NONE(b, flag) (((b) & (flag)) == 0)

#    define FLOAT_EQUAL(a, b)        (std::fabs((a) - (b)) < 1e-6f)
#    define FLOAT_NOT_EQUAL(a, b)    (std::fabs((a) - (b)) >= 1e-6f)

#endif  // __MACRO_DEFINITION_H__
