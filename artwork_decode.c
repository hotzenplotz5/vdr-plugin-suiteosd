#include "artwork_decode.h"

#include <algorithm>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <jpeglib.h>
#include <png.h>
#include <cairo.h>
#include <librsvg/rsvg.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cmath>
#include <memory>
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

// Logos are rendered directly at the bounded display size. A memory-backed
// handle has no base URL, so external local files are not resolved.
Artwork LoadSvg(const std::string &path, int maxWidth, int maxHeight)
{
    constexpr size_t kMaxSvgBytes = 2U * 1024U * 1024U;
    std::unique_ptr<FILE, decltype(&fclose)> file(fopen(path.c_str(), "rb"), fclose);
    if (!file) return {};
    std::vector<unsigned char> bytes(kMaxSvgBytes + 1);
    const size_t size = fread(bytes.data(), 1, bytes.size(), file.get());
    if (size == 0 || size > kMaxSvgBytes || ferror(file.get())) return {};
    GError *error = nullptr;
    auto releaseHandle = [](RsvgHandle *p) { if (p) g_object_unref(p); };
    std::unique_ptr<RsvgHandle, decltype(releaseHandle)> handle(
        rsvg_handle_new_from_data(bytes.data(), size, &error), releaseHandle);
    if (error) g_error_free(error);
    if (!handle) return {};

    double w = 0, h = 0;
    if (!rsvg_handle_get_intrinsic_size_in_pixels(handle.get(), &w, &h)) {
        gboolean hasViewBox = FALSE;
        RsvgRectangle box{};
        rsvg_handle_get_intrinsic_dimensions(handle.get(), nullptr, nullptr,
                                             nullptr, nullptr, &hasViewBox, &box);
        w = hasViewBox ? box.width : maxWidth;
        h = hasViewBox ? box.height : maxHeight;
    }
    if (!std::isfinite(w) || !std::isfinite(h) || w <= 0 || h <= 0)
        return {};
    const double scale = std::min(double(maxWidth) / w, double(maxHeight) / h);
    const int width = std::clamp(int(w * scale), 1, maxWidth);
    const int height = std::clamp(int(h * scale), 1, maxHeight);
    std::unique_ptr<cairo_surface_t, decltype(&cairo_surface_destroy)> surface(
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height),
        cairo_surface_destroy);
    if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS) return {};
    std::unique_ptr<cairo_t, decltype(&cairo_destroy)> context(
        cairo_create(surface.get()), cairo_destroy);
    if (cairo_status(context.get()) != CAIRO_STATUS_SUCCESS) return {};
    RsvgRectangle viewport{0, 0, double(width), double(height)};
    error = nullptr;
    const gboolean ok = rsvg_handle_render_document(handle.get(), context.get(),
                                                    &viewport, &error);
    if (error) g_error_free(error);
    if (!ok || cairo_status(context.get()) != CAIRO_STATUS_SUCCESS) return {};
    cairo_surface_flush(surface.get());
    const unsigned char *data = cairo_image_surface_get_data(surface.get());
    const int stride = cairo_image_surface_get_stride(surface.get());
    if (!data || stride < width * 4) return {};

    Artwork result;
    result.width = width;
    result.height = height;
    result.argb.resize(size_t(width) * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t p;
            std::memcpy(&p, data + size_t(y) * stride + 4 * x, sizeof(p));
            const uint32_t a = p >> 24;
            // Cairo stores premultiplied ARGB; VDR's cImage needs straight ARGB.
            auto straight = [a](uint32_t v) -> uint32_t {
                return a ? std::min(255U, (v * 255U + a / 2U) / a) : 0U;
            };
            result.argb[size_t(y) * width + x] =
                (a << 24) | (straight((p >> 16) & 255U) << 16) |
                (straight((p >> 8) & 255U) << 8) | straight(p & 255U);
        }
    }
    return result;
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
    if (!ok) {
        if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".svg") == 0)
            return LoadSvg(path, maxWidth, maxHeight);
        return {};
    }

    Artwork result = ScaleToFit(raw, maxWidth, maxHeight);
    free(raw.pixels);
    return result;
}

// GdkPixbuf is already part of the librsvg image stack. Accept only the
// Public-v1 binary formats and keep the decoded result thumbnail-sized.
Artwork LoadArtworkBytes(const std::string &bytes,int maxWidth,int maxHeight)
{
    if(bytes.size()<12||bytes.size()>kMaxDecodeBytes||maxWidth<1||maxHeight<1||
       maxWidth>kMaxThumbnailSide||maxHeight>kMaxThumbnailSide)return {};
    const auto *p=reinterpret_cast<const unsigned char*>(bytes.data());
    const char *type=png_sig_cmp(p,0,8)==0?"png":
        p[0]==0xff&&p[1]==0xd8?"jpeg":
        bytes.compare(0,4,"RIFF")==0&&bytes.compare(8,4,"WEBP")==0?"webp":nullptr;
    if(!type)return {};
    GError *error=nullptr;
    auto *loader=gdk_pixbuf_loader_new_with_type(type,&error);
    if(error){g_error_free(error);error=nullptr;}
    if(!loader)return {};
    struct Size {int w,h;bool valid=true;} size{maxWidth,maxHeight};
    g_signal_connect(loader,"size-prepared",G_CALLBACK(+[](GdkPixbufLoader *l,int w,int h,gpointer u){
        auto &s=*static_cast<Size*>(u);
        if(w<=0||h<=0){s.valid=false;return;}
        const double scale=std::min({1.0,double(s.w)/w,double(s.h)/h});
        gdk_pixbuf_loader_set_size(l,std::max(1,int(w*scale)),std::max(1,int(h*scale)));
    }),&size);
    const bool written=gdk_pixbuf_loader_write(loader,p,bytes.size(),&error);
    if(error){g_error_free(error);error=nullptr;}
    const bool closed=gdk_pixbuf_loader_close(loader,&error);
    if(error)g_error_free(error);
    Artwork result;
    if(written&&closed&&size.valid)if(auto *pix=gdk_pixbuf_loader_get_pixbuf(loader)){
        const int w=gdk_pixbuf_get_width(pix),h=gdk_pixbuf_get_height(pix);
        const int channels=gdk_pixbuf_get_n_channels(pix),stride=gdk_pixbuf_get_rowstride(pix);
        if(w>0&&h>0&&w<=maxWidth&&h<=maxHeight&&(channels==3||channels==4)){
            result={w,h,std::vector<uint32_t>(size_t(w)*h)};
            const auto *pixels=gdk_pixbuf_read_pixels(pix);
            for(int y=0;y<h;++y)for(int x=0;x<w;++x){
                const auto *q=pixels+y*stride+x*channels;
                result.argb[size_t(y)*w+x]=(uint32_t(channels==4?q[3]:255)<<24)|
                    (uint32_t(q[0])<<16)|(uint32_t(q[1])<<8)|q[2];
            }
        }
    }
    g_object_unref(loader);
    return result;
}

} // namespace suiteosd
