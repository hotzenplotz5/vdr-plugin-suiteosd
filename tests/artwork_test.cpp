#include "../artwork_decode.h"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <jpeglib.h>
#include <png.h>
#include <string>

int main()
{
    const std::string pngPath = "suiteosd-artwork-test.png";
    const std::string jpegPath = "suiteosd-artwork-test.jpg";
    const std::string invalidPath = "suiteosd-artwork-broken.png";
    const unsigned char rgba[16] = {
        255,0,0,255, 0,255,0,255,
        0,0,255,255, 255,255,255,128
    };
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = 2;
    image.height = 2;
    image.format = PNG_FORMAT_RGBA;
    assert(png_image_write_to_file(&image, pngPath.c_str(), 0, rgba, 0, nullptr));
    auto png = suiteosd::LoadArtwork(pngPath, 2, 2);
    assert(png.valid() && png.width == 2 && png.height == 2);
    assert(png.argb[0] == 0xFFFF0000u);
    assert(png.argb[3] == 0x80FFFFFFu);
    auto opaque = png;
    suiteosd::OpaqueArtwork(opaque, 0xFF102030u);
    assert(opaque.argb[0] == 0xFFFF0000u);
    assert(opaque.argb[3] == 0xFF889098u);
    suiteosd::Artwork transparent{1, 1, {0x00010203u}};
    suiteosd::OpaqueArtwork(transparent, 0xFF102030u);
    assert(transparent.argb[0] == 0xFF102030u);
    auto smaller = suiteosd::LoadArtwork(pngPath, 1, 1);
    assert(smaller.valid() && smaller.width == 1 && smaller.height == 1);

    FILE *f = fopen(jpegPath.c_str(), "wb");
    assert(f);
    jpeg_compress_struct cinfo{};
    jpeg_error_mgr err{};
    cinfo.err = jpeg_std_error(&err);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, f);
    cinfo.image_width = 2;
    cinfo.image_height = 2;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 95, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    unsigned char pixels[12] = {
        255,0,0, 255,0,0,
        255,0,0, 255,0,0
    };
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row = pixels + size_t(cinfo.next_scanline) * 6;
        jpeg_write_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(f);

    auto jpeg = suiteosd::LoadArtwork(jpegPath, 2, 2);
    assert(jpeg.valid() && jpeg.width == 2 && jpeg.height == 2);
    assert((jpeg.argb[0] & 0xFF000000u) == 0xFF000000u);
    assert(!suiteosd::LoadArtwork("/nonexistent/suiteosd-artwork.jpg", 3, 3).valid());
    assert(!suiteosd::LoadArtwork(pngPath, 2000, 2000).valid());

    f = fopen(invalidPath.c_str(), "wb");
    assert(f);
    const unsigned char malformedJpeg[] = {0xff, 0xd8, 0x00, 0x00};
    fwrite(malformedJpeg, 1, sizeof(malformedJpeg), f);
    fclose(f);
    assert(!suiteosd::LoadArtwork(invalidPath, 10, 10).valid());

    const std::string svgPath = "suiteosd-artwork-test.svg";
    f = fopen(svgPath.c_str(), "wb");
    assert(f);
    fputs(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 20 10"><rect width="10" height="10" fill="#ff0000" fill-opacity="0.5"/></svg>)", f);
    fclose(f);
    auto svg = suiteosd::LoadArtwork(svgPath, 20, 20);
    assert(svg.valid() && svg.width == 20 && svg.height == 10);
    assert((svg.argb[0] & 0x00FFFFFFu) == 0x00FF0000u);
    assert((svg.argb[0] >> 24) >= 127 && (svg.argb[0] >> 24) <= 128);
    assert(svg.argb[19] == 0);
    f = fopen(svgPath.c_str(), "wb");
    assert(f);
    fputs("not an SVG", f);
    fclose(f);
    assert(!suiteosd::LoadArtwork(svgPath, 20, 20).valid());
    remove(svgPath.c_str());
    remove(pngPath.c_str());
    remove(jpegPath.c_str());
    remove(invalidPath.c_str());
    return 0;
}
