#include "../artwork_decode.h"
#include "../cinematic_home.h"
#include "../home_layout.h"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <jpeglib.h>
#include <png.h>
#include <string>
#include <fstream>
#include <iterator>

int main()
{
    using suiteosd::HomeLayout;
    const auto bg=suiteosd::CinematicBackground(800,450,272);
    assert(bg.valid()&&bg.argb[0]!=bg.argb[400]);
    for(auto px:bg.argb)assert((px>>24)==255);
    const int cx=HomeLayout::cardX(0,true)/2,cy=(HomeLayout::top+HomeLayout::titleHeight)/2;
    auto normal=suiteosd::CinematicFrame(bg,cx,cy,66,99,8,suiteosd::CinemaStyle::Poster,.5);
    auto focused=suiteosd::CinematicFrame(bg,cx,cy,66,99,8,suiteosd::CinemaStyle::Focus,.5);
    assert(normal.x==focused.x&&normal.y==focused.y);
    assert(normal.image.width==focused.image.width&&normal.image.height==focused.image.height);
    assert(normal.image.argb!=focused.image.argb);
    const auto sample=focused.image.argb[size_t(cy+20-focused.y)*focused.image.width+cx-focused.x];
    assert((sample&255u)>200u);
    suiteosd::Artwork fixture{64,97,std::vector<uint32_t>(64*97,0xFFFF0000u)};
    suiteosd::CinematicArtwork(focused,fixture,cx+1,cy+1,cx,cy,66,99,8,2);
    assert(focused.image.argb[size_t(cy+30-focused.y)*focused.image.width+cx+30-focused.x]==0xFFFF0000u);
    for(auto px:focused.image.argb)assert((px>>24)==255);
    const auto glyph=suiteosd::CinematicSidebarGlyph(0,30);
    assert(glyph.valid()&&std::any_of(glyph.argb.begin(),glyph.argb.end(),[](uint32_t p){return (p>>24)>0&&(p>>24)<255;}));
    const auto glyphPatch=suiteosd::CinematicGlyphPatch(bg,nullptr,20,60,30,0);
    assert(glyphPatch.image.valid());
    for(auto px:glyphPatch.image.argb)assert((px>>24)==255);
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
    auto readBytes=[](const std::string &path){std::ifstream f(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
    auto pngBytes=suiteosd::LoadArtworkBytes(readBytes(pngPath),2,2);
    assert(pngBytes.valid()&&pngBytes.argb==png.argb);
    assert(suiteosd::LoadArtworkBytes(readBytes(pngPath),1,1).width==1);
    assert(!suiteosd::LoadArtworkBytes("invalid image bytes",2,2).valid());
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
    suiteosd::Artwork large{1,1,{0xFFFFFFFFu}};
    suiteosd::ExpandArtwork(large,4,4);
    assert(large.valid() && large.width==4 && large.height==4 && large.argb[5]==0xFFFFFFFFu);
    suiteosd::FadeArtwork(large,0xFF090B0Fu);
    assert(large.argb[0]==0xFF090B0Fu && large.argb[3]==0xFFFFFFFFu && large.argb[15]==0xFF090B0Fu);
    suiteosd::Artwork wide{6,2,std::vector<uint32_t>(12,0xFFFF0000u)};
    wide.argb[2]=wide.argb[3]=0xFF00FF00u;
    auto poster=suiteosd::CoverHomePoster(wide,4,6,1);
    assert(poster.valid()&&poster.width==4&&poster.height==6);
    assert(poster.argb.front()==0xFF091B30u&&poster.argb.back()==0xFF091B30u);
    assert(poster.argb[2]==0xFF00FF00u); // center crop, not stretched wide source
    assert(poster.argb[5*4+2]==0xFF091B30u); // opaque text gradient
    assert(!suiteosd::CoverHomePoster({},4,6,1).valid());
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
    auto jpegBytes=suiteosd::LoadArtworkBytes(readBytes(jpegPath),2,2);
    assert(jpegBytes.valid()&&jpegBytes.width==2&&jpegBytes.height==2);
    assert((jpegBytes.argb[0]>>24)==255);
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
