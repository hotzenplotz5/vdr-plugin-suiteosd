#include "artwork_decode.h"

#include <algorithm>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <jpeglib.h>
#include <png.h>
#include <vector>

namespace suiteosd {
namespace {
constexpr size_t kMaxDecodeBytes = 32U * 1024U * 1024U;
constexpr int kMaxThumbnailSide = 1024;

struct RawImage {
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char *pixels = nullptr;
};

struct JpegError {
    jpeg_error_mgr base;
    jmp_buf jump;
};
void JpegFail(j_common_ptr info)
{
    auto *error = reinterpret_cast<JpegError *>(info->err);
    longjmp(error->jump, 1);
}

bool LoadJpeg(const std::string &path, int maxWidth, int maxHeight, RawImage &result)
{
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) return false;

    jpeg_decompress_struct decoder{};
    JpegError error{};
    decoder.err = jpeg_std_error(&error.base);
    error.base.error_exit = JpegFail;
    unsigned char *buffer = nullptr;
    bool created = false;
    if (setjmp(error.jump)) {
        if (created) jpeg_destroy_decompress(&decoder);
        free(buffer);
        fclose(fp);
        return false;
    }

    jpeg_create_decompress(&decoder);
    created = true;
    jpeg_stdio_src(&decoder, fp);
    if (jpeg_read_header(&decoder, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&decoder);
        fclose(fp);
        return false;
    }
    // Decode at 1/8, 1/4, 1/2, or full resolution as necessary.
    // Avoid allocating full-size movie poster bitmaps.
    decoder.scale_num = 1;
    decoder.scale_denom = 1;
    while (decoder.scale_denom < 8 &&
           (decoder.image_width / decoder.scale_denom > unsigned(maxWidth * 2) ||
            decoder.image_height / decoder.scale_denom > unsigned(maxHeight * 2)))
        decoder.scale_denom *= 2;
    decoder.out_color_space = JCS_RGB;
    jpeg_start_decompress(&decoder);

    const size_t width = decoder.output_width;
    const size_t height = decoder.output_height;
    const size_t channels = decoder.output_components;
    if (!width || !height || channels != 3 ||
        width > kMaxDecodeBytes / channels ||
        height > kMaxDecodeBytes / (width * channels)) {
        jpeg_destroy_decompress(&decoder);
        fclose(fp);
        return false;
    }
    const size_t bytes = width * height * channels;
    buffer = static_cast<unsigned char *>(malloc(bytes));
    if (!buffer) {
        jpeg_destroy_decompress(&decoder);
        fclose(fp);
        return false;
    }
    while (decoder.output_scanline < decoder.output_height) {
        JSAMPROW line = buffer + size_t(decoder.output_scanline) * width * channels;
        jpeg_read_scanlines(&decoder, &line, 1);
    }
    jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    fclose(fp);
    result.width = int(width);
    result.height = int(height);
    result.channels = int(channels);
    result.pixels = buffer;
    return true;
}

bool LoadPng(const std::string &path, RawImage &result)
{
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&image, path.c_str()))
        return false;

    const size_t width = image.width;
    const size_t height = image.height;
    if (!width || !height || width > kMaxDecodeBytes / 4 ||
        height > kMaxDecodeBytes / (width * 4)) {
        png_image_free(&image);
        return false;
    }
    image.format = PNG_FORMAT_RGBA;
    unsigned char *bytes = static_cast<unsigned char *>(malloc(width * height * 4));
    if (!bytes) {
        png_image_free(&image);
        return false;
    }
    if (!png_image_finish_read(&image, nullptr, bytes, 0, nullptr)) {
        free(bytes);
        png_image_free(&image);
        return false;
    }
    png_image_free(&image);
    result.width = int(width);
    result.height = int(height);
    result.channels = 4;
    result.pixels = bytes;
    return true;
}

Artwork ScaleToFit(const RawImage &source, int maxWidth, int maxHeight)
{
    Artwork result;
    if (!source.pixels || !source.width || !source.height || !source.channels)
        return result;

    const double scale = std::min({1.0, double(maxWidth) / source.width,
                                    double(maxHeight) / source.height});
    result.width = std::max(1, int(source.width * scale));
    result.height = std::max(1, int(source.height * scale));
    result.argb.resize(size_t(result.width) * size_t(result.height));
    for (int y = 0; y < result.height; ++y) {
        const int sy = std::min(source.height - 1, int(double(y) * source.height / result.height));
        for (int x = 0; x < result.width; ++x) {
            const int sx = std::min(source.width - 1, int(double(x) * source.width / result.width));
            const unsigned char *p = source.pixels + (size_t(sy) * source.width + sx) * source.channels;
            uint32_t a = source.channels == 4 ? p[3] : 255U;
            result.argb[size_t(y) * result.width + x] =
                (a << 24) | (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | uint32_t(p[2]);
        }
    }
    return result;
}
} // namespace

Artwork LoadArtwork(const std::string &path, int maxWidth, int maxHeight)
{
    if (path.empty() || maxWidth < 1 || maxHeight < 1 ||
        maxWidth > kMaxThumbnailSide || maxHeight > kMaxThumbnailSide)
        return {};
    // Identify file content by signature, not file extension.
    unsigned char magic[8] = {};
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return {};
    const size_t n = fread(magic, 1, sizeof(magic), f);
    fclose(f);

    RawImage raw;
    bool ok = false;
    if (n >= 8 && png_sig_cmp(magic, 0, 8) == 0)
        ok = LoadPng(path, raw);
    else if (n >= 2 && magic[0] == 0xff && magic[1] == 0xd8)
        ok = LoadJpeg(path, maxWidth, maxHeight, raw);
    if (!ok) return {};

    Artwork result = ScaleToFit(raw, maxWidth, maxHeight);
    free(raw.pixels);
    return result;
}

} // namespace suiteosd
