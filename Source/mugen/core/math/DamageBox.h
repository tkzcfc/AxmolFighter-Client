#pragma once

#include "IntVec3.h"
#include "mugen/core/reflect/Reflect.h"

NS_MG_BEGIN

// 以逻辑整数坐标表示的轴对齐三维碰撞盒。
struct DamageBox
{
    // 相对动画基点的 X、Y、Z 最小角坐标。
    IntVec3 pos;

    // X 为宽度、Y 为深度、Z 为高度的非负尺寸。
    IntVec3 size;

#ifdef RUNTIME_IN_AXMOL
    void drawDebug(ax::DrawNode* drawNode,
                   const ax::Color4F& borderColor = ax::Color4F::RED,
                   const ax::Color4F& fillColor   = ax::Color4F(1.0f, 0.0f, 0.0f, 0.15f)) const
    {
        if (drawNode == nullptr)
            return;

        // 斜二测与编辑器一致：深度轴 pos.y 投影到右上 45° 并乘 0.5。
        // 坐标是动画局部坐标。世界框要先减去实体纵深，绝对纵深会把框推离角色。
        const float oblique = 0.5f;

        float fx = static_cast<float>(pos.x);
        float fz = static_cast<float>(pos.z);
        float sx = static_cast<float>(size.x);
        float sy = static_cast<float>(size.y);
        float sz = static_cast<float>(size.z);

        const float y0 = static_cast<float>(pos.y);
        const float y1 = y0 + sy;
        float dox0 = y0 * oblique;
        float doz0 = y0 * oblique;
        float dox1 = y1 * oblique;
        float doz1 = y1 * oblique;

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

    // 判断两个半开区间 AABB 在三个轴上是否均有正长度交集。
    bool overlaps(const DamageBox& other) const
    {
        // 三维 AABB 碰撞要求 x、y、z 三轴均有重叠。
        bool overlapX = (pos.x < other.pos.x + other.size.x) && (pos.x + size.x > other.pos.x);
        bool overlapY = (pos.y < other.pos.y + other.size.y) && (pos.y + size.y > other.pos.y);
        bool overlapZ = (pos.z < other.pos.z + other.size.z) && (pos.z + size.z > other.pos.z);
        return overlapX && overlapY && overlapZ;
    }

    MG_REFLECT(DamageBox, pos, size)
};

NS_MG_END
