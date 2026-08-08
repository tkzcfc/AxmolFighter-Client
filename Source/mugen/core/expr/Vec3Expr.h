#include "mugen/core/Object.h"
#include "mugen/core/math/Vec3.h"
#include "ExprEval.h"

NS_MG_BEGIN

class Vec3Expr : public Object
{
public:
    typedef Object Super;

public:
    Vector3f toVec3f(ExprEval& exprEval) const
    {
        float xx = static_cast<float>(exprEval.eval(x));
        float yy = static_cast<float>(exprEval.eval(y));
        float zz = static_cast<float>(exprEval.eval(z));
        return Vector3f(xx, yy, zz);
    }

    Vector3i toVec3i(ExprEval& exprEval) const
    {
        int32_t xx = static_cast<int32_t>(exprEval.eval(x));
        int32_t yy = static_cast<int32_t>(exprEval.eval(y));
        int32_t zz = static_cast<int32_t>(exprEval.eval(z));
        return Vector3i(xx, yy, zz);
    }

    float f32_x(ExprEval& exprEval) const { return static_cast<float>(exprEval.eval(x)); }

    float f32_y(ExprEval& exprEval) const { return static_cast<float>(exprEval.eval(y)); }

    float f32_z(ExprEval& exprEval) const { return static_cast<float>(exprEval.eval(z)); }

    int32_t i32_x(ExprEval& exprEval) const { return static_cast<int32_t>(exprEval.eval(x)); }

    int32_t i32_y(ExprEval& exprEval) const { return static_cast<int32_t>(exprEval.eval(y)); }

    int32_t i32_z(ExprEval& exprEval) const { return static_cast<int32_t>(exprEval.eval(z)); }

public:
    std::string x = "0";
    std::string y = "0";
    std::string z = "0";

    MG_DEFINE_SERIALIZABLE(x, y, z);
};

NS_MG_END
