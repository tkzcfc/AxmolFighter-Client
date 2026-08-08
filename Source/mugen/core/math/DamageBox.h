#pragma once

#include "Vec3.h"

NS_MG_BEGIN

class DamageBox : public Object
{
public:
    typedef Object Super;

public:
    DamageBox() = default;
    virtual ~DamageBox() {}

    // X : 以ani坐标为基点的碰撞框X轴坐标
    // Y : 以ani坐标为基点的碰撞框Y轴坐标
    // Z : 以ani坐标为基点的碰撞框Z轴坐标
    Vector3i pos;

    // X : 宽度
    // Y : 深度
    // Z : 高度
    Vector3i size;

#ifdef RUNTIME_IN_AXMOL
    void drawDebug(ax::DrawNode* drawNode,
                   const ax::Color4F& borderColor = ax::Color4F::RED,
                   const ax::Color4F& fillColor   = ax::Color4F(1.0f, 0.0f, 0.0f, 0.15f)) const
    {
        if (drawNode == nullptr)
            return;

        // 斜二测只表达盒子自身的深度厚度（size.y），不能再用世界 pos.y 做偏移：
        // 世界变换里屏幕纵坐标已是 position.y+position.z（与 Avatar 一致），
        // 若再对绝对 pos.y 做 oblique，角色往深处走时框会整块往右上飘。
        const float oblique = 0.5f;

        float fx = static_cast<float>(pos.x);
        float fz = static_cast<float>(pos.z);
        float sx = static_cast<float>(size.x);
        float sy = static_cast<float>(size.y);
        float sz = static_cast<float>(size.z);

        float dox0 = 0.0f;
        float doz0 = 0.0f;
        float dox1 = sy * oblique;
        float doz1 = sy * oblique;

        // 顶面和侧面的填充色略暗（乘以0.8亮度）
        ax::Color4F fillSide(fillColor.r * 0.8f, fillColor.g * 0.8f, fillColor.b * 0.8f, fillColor.a);
        ax::Color4F borderSide(borderColor.r, borderColor.g, borderColor.b, borderColor.a);

        // 8个顶点：前面(F)=盒子近端、后面(B)=近端 + 自身深度
        ax::Vec2 FL(fx + dox0, fz + doz0);
        ax::Vec2 FR(fx + sx + dox0, fz + doz0);
        ax::Vec2 FRT(fx + sx + dox0, fz + sz + doz0);
        ax::Vec2 FLT(fx + dox0, fz + sz + doz0);

        ax::Vec2 BL(fx + dox1, fz + doz1);
        ax::Vec2 BR(fx + sx + dox1, fz + doz1);
        ax::Vec2 BRT(fx + sx + dox1, fz + sz + doz1);
        ax::Vec2 BLT(fx + dox1, fz + sz + doz1);

        // 顶面（上，x-y平面）— 先画，避免遮挡正面边框
        ax::Vec2 topFace[] = {FLT, FRT, BRT, BLT};
        drawNode->drawSolidPoly(topFace, 4, fillSide, 1.0f, borderSide);

        // 右侧面（右，y-z平面）
        ax::Vec2 rightFace[] = {FR, BR, BRT, FRT};
        drawNode->drawSolidPoly(rightFace, 4, fillSide, 1.0f, borderSide);

        // 正面（前，x-z平面）— 最后画，确保正面边框最清晰
        ax::Vec2 frontFace[] = {FL, FR, FRT, FLT};
        drawNode->drawSolidPoly(frontFace, 4, fillColor, 1.0f, borderColor);
    }
#endif

    bool overlaps(const DamageBox& other) const
    {
        // 3D AABB 碰撞检测：x/y/z 三轴均有重叠
        bool overlapX = (pos.x < other.pos.x + other.size.x) && (pos.x + size.x > other.pos.x);
        bool overlapY = (pos.y < other.pos.y + other.size.y) && (pos.y + size.y > other.pos.y);
        bool overlapZ = (pos.z < other.pos.z + other.size.z) && (pos.z + size.z > other.pos.z);
        return overlapX && overlapY && overlapZ;
    }

    MG_DEFINE_SERIALIZABLE(pos, size);
};

NS_MG_END
