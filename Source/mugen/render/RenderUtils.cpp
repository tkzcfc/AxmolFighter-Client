#include "RenderUtils.h"

NS_MG_BEGIN

#ifdef RUNTIME_IN_AXMOL

namespace RenderUtils
{

static void setSpriteQuad(ax::V3F_C4B_T2F_Quad* quad,
                          const ax::Size& origSize,
                          const int x,
                          const int y,
                          float x_factor,
                          float y_factor)
{
    float offset_x = origSize.width * x;
    float offset_y = origSize.height * y;

    quad->bl.vertices.set(ax::Vec3(offset_x, offset_y, 0));
    quad->br.vertices.set(ax::Vec3(offset_x + (origSize.width * x_factor), offset_y, 0));
    quad->tl.vertices.set(ax::Vec3(offset_x, offset_y + (origSize.height * y_factor), 0));
    quad->tr.vertices.set(ax::Vec3(offset_x + (origSize.width * x_factor), offset_y + (origSize.height * y_factor), 0));

    if (x_factor != 1.0f || y_factor != 1.0f)
    {
        float x_size = (quad->br.texCoords.u - quad->bl.texCoords.u) * x_factor;
        float y_size = (quad->tl.texCoords.v - quad->bl.texCoords.v) * y_factor;

        quad->br.texCoords = ax::Tex2F(quad->bl.texCoords.u + x_size, quad->bl.texCoords.v);
        quad->tl.texCoords = ax::Tex2F(quad->tl.texCoords.u, quad->bl.texCoords.v + y_size);
        quad->tr.texCoords = ax::Tex2F(quad->bl.texCoords.u + x_size, quad->bl.texCoords.v + y_size);
    }
}

// https://github.com/cocos2d/creator_to_cocos2dx/blob/master/creator_project/packages/creator-luacpp-support/reader/CreatorReader.cpp#L1480
void tileSprite(ax::Sprite* sprite)
{
    const auto new_s        = sprite->getContentSize();
    const auto frame        = sprite->getSpriteFrame();
    const auto orig_s_pixel = frame->getOriginalSizeInPixels();
    const auto orig_rect    = frame->getRectInPixels();

    if (orig_s_pixel.fuzzyEquals(ax::Vec2::ZERO, 0.0001f))
    {
        return;
    }

    // cheat: let the sprite calculate the original Quad for us.
    sprite->setContentSize(orig_s_pixel);
    ax::V3F_C4B_T2F_Quad origQuad = sprite->getQuad();

    // restore the size
    sprite->setContentSize(new_s);

    const float f_x = new_s.width / orig_rect.size.width;
    const float f_y = new_s.height / orig_rect.size.height;
    const int n_x   = std::ceil(f_x);
    const int n_y   = std::ceil(f_y);

    const int totalQuads = n_x * n_y;

    // use new instead of malloc, since Polygon info will release them using delete
    ax::V3F_C4B_T2F_Quad* quads = new (std::nothrow) ax::V3F_C4B_T2F_Quad[totalQuads];
    unsigned short* indices     = new (std::nothrow) unsigned short[totalQuads * 6];

    // populate the vertices
    for (int y = 0; y < n_y; ++y)
    {
        for (int x = 0; x < n_x; ++x)
        {
            quads[y * n_x + x] = origQuad;
            float x_factor     = (orig_rect.size.width * (x + 1) <= new_s.width) ? 1 : f_x - (long)f_x;
            float y_factor     = (orig_rect.size.height * (y + 1) <= new_s.height) ? 1 : f_y - (long)f_y;
            // MG_LOG_I("x={}, y={}", x_factor, y_factor);
            setSpriteQuad(&quads[y * n_x + x], orig_rect.size, x, y, x_factor, y_factor);
        }
    }

    // populate the indices
    for (int i = 0; i < totalQuads; i++)
    {
        indices[i * 6 + 0] = (unsigned short)(i * 4 + 0);
        indices[i * 6 + 1] = (unsigned short)(i * 4 + 1);
        indices[i * 6 + 2] = (unsigned short)(i * 4 + 2);
        indices[i * 6 + 3] = (unsigned short)(i * 4 + 3);
        indices[i * 6 + 4] = (unsigned short)(i * 4 + 2);
        indices[i * 6 + 5] = (unsigned short)(i * 4 + 1);
    }

    ax::PolygonInfo poly;
    poly.triangles.vertCount  = 4 * totalQuads;
    poly.triangles.indexCount = 6 * totalQuads;
    poly.triangles.verts      = (ax::V3F_C4B_T2F*)quads;
    poly.triangles.indices    = indices;
    sprite->setPolygonInfo(poly);
}

void setupSpriteType(ax::Sprite* sprite, SpriteType type)
{
    switch (type)
    {
    case SpriteType::kSpriteType_Simple:
        sprite->setCenterRectNormalized(ax::Rect(0, 0, 1, 1));
        break;
    case SpriteType::kSpriteType_Tiled:
        tileSprite(sprite);
        break;
    case SpriteType::kSpriteType_Sliced:
    case SpriteType::kSpriteType_Filled:
        break;
    }
}

}  // namespace RenderUtils

#endif  // RUNTIME_IN_AXMOL

NS_MG_END
