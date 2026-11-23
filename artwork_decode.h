#ifndef SUITEOSD_ARTWORK_DECODE_H
#define SUITEOSD_ARTWORK_DECODE_H

#include <cstdint>
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

// Pure image decoder, independent of VDR. Decode only the small on-screen image.
// Supports PNG/JPEG, preserves aspect ratio, no network or shell invocations.
Artwork LoadArtwork(const std::string &path, int maxWidth, int maxHeight);

} // namespace suiteosd

#endif
