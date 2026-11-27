/*
 * Perspective cover-flow presentation of image_next ARGB thumbnails.
 * Pure CPU pixel transformation, not a second image decoder or player.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef VDR_SUITEOSD_PHOTO_CAROUSEL_H
#define VDR_SUITEOSD_PHOTO_CAROUSEL_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace suiteosd {

struct CarouselSlot {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    float opacity = 1.0f;
    float inset = 0.0f; // outer edge is foreshortened; inner edge stays tall
    int side = 0;        // -1: left, +1: right, 0: centre
};

inline float CarouselMix(float a, float b, float t)
{
    return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
}

// Layout is expressed in SuiteOSD's 1600 x 900 virtual coordinate system.
// Left photos are taller at their RIGHT edge; right photos are taller at
// their LEFT edge. This inward facing perspective is intentional.
inline CarouselSlot CarouselGeometry(float relative)
{
    const float distance = std::min(2.4f, std::fabs(relative));
    const float a = std::min(distance, 1.0f);
    const float b = std::clamp(distance - 1.0f, 0.0f, 1.0f);
    const float offset = distance <= 1.0f ?
        CarouselMix(0.0f, 438.0f, a) :
        CarouselMix(438.0f, 677.0f, b);
    const float width = distance <= 1.0f ?
        CarouselMix(612.0f, 252.0f, a) :
        CarouselMix(252.0f, 188.0f, b);
    const float height = distance <= 1.0f ?
        CarouselMix(420.0f, 328.0f, a) :
        CarouselMix(328.0f, 236.0f, b);
    const float top = distance <= 1.0f ?
        CarouselMix(178.0f, 238.0f, a) :
        CarouselMix(238.0f, 284.0f, b);
    const float opacity = distance <= 1.0f ?
        CarouselMix(1.0f, 0.62f, a) :
        CarouselMix(0.62f, 0.39f, b);
    const float inset = distance <= 1.0f ?
        CarouselMix(0.0f, 0.14f, a) :
        CarouselMix(0.14f, 0.21f, b);
    CarouselSlot slot;
    slot.width = std::max(1, static_cast<int>(std::lround(width)));
    slot.height = std::max(1, static_cast<int>(std::lround(height)));
    slot.x = static_cast<int>(std::lround(800.0f +
        (relative < 0.0f ? -offset : offset) - width / 2.0f));
    slot.y = static_cast<int>(std::lround(top));
    slot.opacity = opacity;
    slot.inset = inset;
    slot.side = relative < -0.0001f ? -1 : relative > 0.0001f ? 1 : 0;
    return slot;
}

struct CarouselBitmap {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> argb;
    bool valid() const { return width > 0 && height > 0 &&
        argb.size() == static_cast<size_t>(width) * height; }
};

// Perspective warp with cover-cropping and bilinear interpolation.
// The top/bottom of the OUTER vertical edge are inset, creating the
// inward-facing card perspective approved for the SuiteOSD carousel.
inline CarouselBitmap WarpCarousel(const uint32_t *src, int srcWidth,
                                   int srcHeight, int destWidth, int destHeight,
                                   int side, float inset, float opacity,
                                   float radius = 0, float rim = 0,
                                   uint32_t rimColor = 0xFF53E5FFU)
{
    CarouselBitmap result;
    if (!src || srcWidth < 1 || srcHeight < 1 ||
        destWidth < 1 || destHeight < 1 || destWidth > 2048 || destHeight > 1200)
        return result;
    result.width = destWidth;
    result.height = destHeight;
    result.argb.assign(static_cast<size_t>(destWidth) * destHeight, 0);
    const float srcAspect = float(srcWidth) / float(srcHeight);
    const float destAspect = float(destWidth) / float(destHeight);
    const float cropX = srcAspect > destAspect ? destAspect / srcAspect : 1.0f;
    const float cropY = srcAspect < destAspect ? srcAspect / destAspect : 1.0f;
    const float fade = std::clamp(opacity, 0.0f, 1.0f);
    radius = std::clamp(radius, 0.0f, 0.5f * std::min(destWidth, destHeight));

    for (int x = 0; x < destWidth; ++x) {
        const float u = (float(x) + 0.5f) / float(destWidth);
        const float outer = side < 0 ? 1.0f - u : (side > 0 ? u : 0.0f);
        const float verticalInset = std::max(0.0f, inset) * outer;
        const float usable = 1.0f - 2.0f * verticalInset;
        if (usable <= 0.0f) continue;
        const float srcX = ((1.0f - cropX) / 2.0f + u * cropX) *
                           float(srcWidth - 1);
        const int x0 = std::clamp(int(srcX), 0, srcWidth - 1);
        const int x1 = std::min(srcWidth - 1, x0 + 1);
        const float fx = srcX - float(x0);

        for (int y = 0; y < destHeight; ++y) {
            const float v = (float(y) + 0.5f) / float(destHeight);
            if (v < verticalInset || v > 1.0f - verticalInset) continue;
            const float stretch = (v - verticalInset) / usable;
            // Round the unwarped card, then apply the same perspective to
            // the image and its rim. Keep the old rectangular path for callers
            // that request no radius.
            float coverage = 1.0f, edge = 0.0f;
            if (radius > 0 || rim > 0) {
                const float px = std::min(u, 1.0f-u) * destWidth;
                const float py = std::min(stretch, 1.0f-stretch) * destHeight;
                const float dx = radius-px, dy = radius-py;
                const float d = (dx > 0 && dy > 0 ? std::sqrt(dx*dx+dy*dy) :
                                 std::max(dx,dy)) - radius;
                coverage = std::clamp(0.5f-d, 0.0f, 1.0f);
                if (coverage == 0) continue;
                edge = std::clamp(d+rim+0.5f, 0.0f, 1.0f);
            }
            const float srcY = ((1.0f - cropY) / 2.0f + stretch * cropY) *
                               float(srcHeight - 1);
            const int y0 = std::clamp(int(srcY), 0, srcHeight - 1);
            const int y1 = std::min(srcHeight - 1, y0 + 1);
            const float fy = srcY - float(y0);
            const uint32_t corners[4] = {
                src[size_t(y0) * srcWidth + x0],
                src[size_t(y0) * srcWidth + x1],
                src[size_t(y1) * srcWidth + x0],
                src[size_t(y1) * srcWidth + x1]
            };
            uint32_t pixel = 0;
            for (int shift = 0; shift <= 24; shift += 8) {
                const float p0 = CarouselMix(float((corners[0] >> shift) & 255),
                                             float((corners[1] >> shift) & 255), fx);
                const float p1 = CarouselMix(float((corners[2] >> shift) & 255),
                                             float((corners[3] >> shift) & 255), fx);
                float c = CarouselMix(p0, p1, fy);
                if (shift == 24) c = CarouselMix(c, 255.0f, edge) * fade * coverage;
                else c = CarouselMix(c, float((rimColor >> shift) & 255), edge);
                pixel |= static_cast<uint32_t>(std::lround(
                    std::clamp(c, 0.0f, 255.0f))) << shift;
            }
            result.argb[size_t(y) * destWidth + x] = pixel;
        }
    }
    return result;
}

// Fade a vertically mirrored band. The reflection shares the trapezoid's
// already-warped pixels and never starts an independent image decode.
inline CarouselBitmap ReflectCarousel(const CarouselBitmap &card, int depth = 72)
{
    CarouselBitmap result;
    if (!card.valid() || depth <= 0) return result;
    result.width = card.width;
    result.height = std::min(depth, card.height);
    result.argb.resize(size_t(result.width) * result.height, 0);
    for (int y = 0; y < result.height; ++y) {
        const float t = float(y) / float(result.height);
        const float opacity = 0.30f * (1.0f-t) * (1.0f-t);
        for (int x = 0; x < result.width; ++x) {
            // Preserve the warped silhouette and rounded alpha mask.
            const int sy = card.height - 1 - std::min(card.height-1, y);
            const uint32_t pixel = card.argb[size_t(sy) * card.width + x];
            const uint32_t alpha = static_cast<uint32_t>(std::lround(
                float((pixel >> 24) & 255) * opacity));
            result.argb[size_t(y) * result.width + x] =
                (alpha << 24) | (pixel & 0x00FFFFFFU);
        }
    }
    return result;
}

// ARGB chrome patches are always composited onto an opaque canvas by the
// OSD caller. No semi-transparent pixels are copied to the video plane.
inline uint32_t PhotoRgbMix(uint32_t a, uint32_t b, float t) {
    uint32_t out = 0xFF000000U;
    for (int shift : {0,8,16})
        out |= uint32_t(CarouselMix(float((a>>shift)&255),float((b>>shift)&255),t)+0.5f)<<shift;
    return out;
}

inline float PhotoRoundDistance(float x,float y,float w,float h,float radius) {
    radius=std::clamp(radius,0.0f,0.5f*std::min(w,h));
    const float dx=std::abs(x-w*0.5f)-w*0.5f+radius;
    const float dy=std::abs(y-h*0.5f)-h*0.5f+radius;
    const float ox=std::max(0.0f,dx),oy=std::max(0.0f,dy);
    return std::sqrt(ox*ox+oy*oy)+std::min(0.0f,std::max(dx,dy))-radius;
}

inline CarouselBitmap PhotoPanel(int w,int h,float radius,uint32_t color,
                                 uint32_t rim=0,float border=0,int halo=0) {
    CarouselBitmap out;
    if(w<1||h<1||halo<0) return out;
    out.width=w+2*halo; out.height=h+2*halo;
    out.argb.resize(size_t(out.width)*out.height);
    for(int y=0;y<out.height;++y)for(int x=0;x<out.width;++x){
        const float d=PhotoRoundDistance(x+0.5f-halo,y+0.5f-halo,w,h,radius);
        if(d>0.5f){
            if(halo && d<halo){
                const float fade=1.0f-d/halo;
                out.argb[size_t(y)*out.width+x]=(uint32_t(105*fade*fade)<<24)|(rim&0xFFFFFFU);
            }
            continue;
        }
        const float coverage=std::clamp(0.5f-d,0.0f,1.0f);
        const float edge=border>0?std::clamp(d+border+0.5f,0.0f,1.0f):0;
        const uint32_t rgb=PhotoRgbMix(color,rim,edge);
        const float alpha=CarouselMix(float(color>>24),float(rim>>24),edge)*coverage;
        out.argb[size_t(y)*out.width+x]=(uint32_t(alpha+0.5f)<<24)|(rgb&0xFFFFFFU);
    }
    return out;
}

inline CarouselBitmap PhotoBackdrop(int w,int h) {
    CarouselBitmap out;
    if(w<1||h<1) return out;
    out.width=w; out.height=h;
    out.argb.resize(size_t(w)*h);
    std::vector<float> horizontal(w);
    for(int x=0;x<w;++x){
        const float u=(x+0.5f)/w;
        const float d=(u-0.5f)*2;
        horizontal[x]=std::max(0.0f,1-d*d);
    }
    for(int y=0;y<h;++y){
        const float v=(y+0.5f)/h;
        const float upper=std::max(0.0f,1-std::abs(v-0.30f)*3);
        const float floor=std::max(0.0f,1-std::abs(v-0.74f)*5);
        for(int x=0;x<w;++x){
            auto c=PhotoRgbMix(0xFF050D20U,0xFF102B51U,0.50f*horizontal[x]*upper);
            out.argb[size_t(y)*w+x]=PhotoRgbMix(c,0xFF07345AU,0.70f*horizontal[x]*floor);
        }
    }
    return out;
}

} // namespace suiteosd

#endif // VDR_SUITEOSD_PHOTO_CAROUSEL_H
