#include "../photo_carousel.h"
#include <cassert>
#include <cstdint>
#include <vector>

int main()
{
    using namespace suiteosd;
    const auto middle = CarouselGeometry(0.0f);
    const auto left = CarouselGeometry(-1.0f);
    const auto right = CarouselGeometry(1.0f);
    const auto leftOuter = CarouselGeometry(-2.0f);
    const auto rightOuter = CarouselGeometry(2.0f);
    assert(middle.side == 0 && left.side == -1 && right.side == 1);
    assert(middle.width > left.width && left.width > leftOuter.width);
    assert(middle.opacity > left.opacity && left.opacity > leftOuter.opacity);
    assert(left.x < middle.x && right.x > middle.x);
    assert(leftOuter.x < left.x && rightOuter.x > right.x);
    // Approaching the centre makes the same photo larger, less tilted
    // and brighter. The opposite path recedes/fades symmetrically.
    const auto arriving = CarouselGeometry(-0.4f);
    assert(arriving.width > left.width);
    assert(arriving.inset < left.inset);
    assert(arriving.opacity > left.opacity);
    assert(CarouselGeometry(-0.75f).width == CarouselGeometry(0.75f).width);

    std::vector<uint32_t> src(32 * 24, 0xFFAA8844U);
    // Left: right edge remains full height, left edge is inset.
    auto l = WarpCarousel(src.data(), 32, 24, 80, 60, -1, 0.25f, 0.65f);
    assert(l.valid());
    assert((l.argb[0] >> 24) == 0);                // left outer top corner
    assert((l.argb[79] >> 24) > 0);               // left card inner top corner
    assert((l.argb[59 * 80] >> 24) == 0);        // left outer bottom corner
    assert((l.argb[59 * 80 + 79] >> 24) > 0);   // left inner bottom corner

    // Right: left edge remains full height, right edge is inset.
    auto r = WarpCarousel(src.data(), 32, 24, 80, 60, 1, 0.25f, 0.65f);
    assert(r.valid());
    assert((r.argb[0] >> 24) > 0);
    assert((r.argb[79] >> 24) == 0);
    assert((r.argb[59 * 80] >> 24) > 0);
    assert((r.argb[59 * 80 + 79] >> 24) == 0);

    auto centered = WarpCarousel(src.data(), 32, 24, 80, 60, 0, 0, 1.0f);
    assert(centered.valid() && centered.argb[0] == 0xFFAA8844U);
    assert((centered.argb[25 * 80 + 20] >> 24) == 255);
    assert(((l.argb[30 * 80 + 40] >> 24) & 255) < 255);
    const auto reflection = ReflectCarousel(centered, 20);
    assert(reflection.valid() && reflection.height == 20);
    assert((reflection.argb[0] >> 24) > (reflection.argb[19 * 80] >> 24));
    assert(!WarpCarousel(nullptr, 20, 10, 80, 60, 0, 0, 1.0f).valid());
    return 0;
}
