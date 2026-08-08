#include "mugen/core/Object.h"
#include "mugen/core/math/Vec2.h"
#include "ExprEval.h"

NS_MG_BEGIN

class Vec2Expr : public Object
{
public:
    typedef Object Super;

public:
    Vector2f toVec2f(ExprEval& exprEval) const
    {
        float xx = static_cast<float>(exprEval.eval(x));
        float yy = static_cast<float>(exprEval.eval(y));
        return Vector2f(xx, yy);
    }

    Vector2i toVec2i(ExprEval& exprEval) const
    {
        int32_t xx = static_cast<int32_t>(exprEval.eval(x));
        int32_t yy = static_cast<int32_t>(exprEval.eval(y));
        return Vector2i(xx, yy);
    }

    float f32_x(ExprEval& exprEval) const { return static_cast<float>(exprEval.eval(x)); }

    float f32_y(ExprEval& exprEval) const { return static_cast<float>(exprEval.eval(y)); }

    int32_t i32_x(ExprEval& exprEval) const { return static_cast<int32_t>(exprEval.eval(x)); }

    int32_t i32_y(ExprEval& exprEval) const { return static_cast<int32_t>(exprEval.eval(y)); }

public:
    std::string x = "0";
    std::string y = "0";

    MG_DEFINE_SERIALIZABLE(x, y);
};

NS_MG_END
