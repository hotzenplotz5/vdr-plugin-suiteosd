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
        CarouselMix(0.0f, 485.0f, a) :
        CarouselMix(485.0f, 704.0f, b);
    const float width = distance <= 1.0f ?
        CarouselMix(660.0f, 275.0f, a) :
        CarouselMix(275.0f, 166.0f, b);
    const float height = distance <= 1.0f ?
        CarouselMix(404.0f, 326.0f, a) :
        CarouselMix(326.0f, 252.0f, b);
    const float top = distance <= 1.0f ?
        CarouselMix(197.0f, 253.0f, a) :
        CarouselMix(253.0f, 291.0f, b);
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
                                   int side, float inset, float opacity)
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
                if (shift == 24) c *= fade;
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
        const float opacity = 0.24f * (1.0f - float(y) / float(result.height));
        for (int x = 0; x < result.width; ++x) {
            const uint32_t pixel =
                card.argb[size_t(card.height - 1 - y) * card.width + x];
            const uint32_t alpha = static_cast<uint32_t>(std::lround(
                float((pixel >> 24) & 255) * opacity));
            result.argb[size_t(y) * result.width + x] =
                (alpha << 24) | (pixel & 0x00FFFFFFU);
        }
    }
    return result;
}

} // namespace suiteosd

#endif // VDR_SUITEOSD_PHOTO_CAROUSEL_H
