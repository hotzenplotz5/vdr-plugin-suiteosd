#ifndef SUITEOSD_ARTWORK_DECODE_H
#define SUITEOSD_ARTWORK_DECODE_H

#include <cstdint>
#include <initializer_list>
#include <cstddef>
#include <string>
#include <vector>

namespace suiteosd {

struct Artwork {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> argb;
    bool valid() const { return width > 0 && height > 0 && argb.size() == size_t(width) * size_t(height); }
};

// Composite straight-alpha logo pixels onto a quiet, opaque matte.  A VDR
// DrawImage may replace destination pixels rather than blend over the card.
inline void OpaqueArtwork(Artwork &art, uint32_t matte)
{
    for (auto &pixel : art.argb) {
        const uint32_t a = pixel >> 24;
        uint32_t out = 0xFF000000u;
        for (unsigned shift : {0u, 8u, 16u}) {
            const uint32_t fg = (pixel >> shift) & 255u;
            const uint32_t bg = (matte >> shift) & 255u;
            out |= ((fg * a + bg * (255u - a) + 127u) / 255u) << shift;
        }
        pixel = out;
    }
}

// Pure image decoder, independent of VDR. Decode only the small on-screen image.
// Supports PNG/JPEG/SVG, preserves aspect ratio, no network or shell invocations.
Artwork LoadArtwork(const std::string &path, int maxWidth, int maxHeight);

} // namespace suiteosd

#endif
