#ifndef SUITEOSD_ARTWORK_DECODE_H
#define SUITEOSD_ARTWORK_DECODE_H

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <cstddef>
#include <string>
#include <vector>
#include <utility>

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

// Enlarge only the explicitly requested on-screen detail artwork. The
// normal thumbnail decoder still never enlarges its source images.
inline void ExpandArtwork(Artwork &art, int maxWidth, int maxHeight)
{
    if (!art.valid() || maxWidth < 1 || maxHeight < 1 ||
        maxWidth > 1024 || maxHeight > 1024) return;
    const double scale = std::min(double(maxWidth)/art.width,double(maxHeight)/art.height);
    if (scale <= 1.0) return;
    Artwork next;
    next.width = std::clamp(int(art.width*scale),1,maxWidth);
    next.height = std::clamp(int(art.height*scale),1,maxHeight);
    next.argb.resize(size_t(next.width)*next.height);
    for(int y=0;y<next.height;++y) for(int x=0;x<next.width;++x) {
        const double sx = double(x)*(art.width-1)/std::max(1,next.width-1);
        const double sy = double(y)*(art.height-1)/std::max(1,next.height-1);
        const int x0=int(sx), y0=int(sy), x1=std::min(x0+1,art.width-1), y1=std::min(y0+1,art.height-1);
        const double dx=sx-x0, dy=sy-y0;
        uint32_t out=0;
        for(unsigned shift : {0u,8u,16u,24u}) {
            auto c=[&](int xx,int yy){return (art.argb[size_t(yy)*art.width+xx]>>shift)&255u;};
            const double top=c(x0,y0)*(1-dx)+c(x1,y0)*dx;
            const double bottom=c(x0,y1)*(1-dx)+c(x1,y1)*dx;
            out |= uint32_t(top*(1-dy)+bottom*dy+0.5)<<shift;
        }
        next.argb[size_t(y)*next.width+x]=out;
    }
    art=std::move(next);
}

inline void FadeArtwork(Artwork &art,uint32_t matte)
{
    if(!art.valid()) return;
    for(int y=0;y<art.height;++y) for(int x=0;x<art.width;++x) {
        const double fx=std::clamp(double(x)/std::max(1,art.width-1)*1.25-0.15,0.0,1.0);
        const double fy=std::clamp(double(art.height-1-y)/std::max(1,art.height-1)*3.5,0.0,1.0);
        auto &pixel=art.argb[size_t(y)*art.width+x];
        const unsigned a=unsigned(255*fx*fy);
        uint32_t out=0xFF000000u;
        for(unsigned shift : {0u,8u,16u}) {
            const unsigned fg=(pixel>>shift)&255u,bg=(matte>>shift)&255u;
            out |= ((fg*a+bg*(255-a)+127)/255)<<shift;
        }
        pixel=out;
    }
}

// Center-crop to the card bounds without stretching. The dark, opaque
// lower gradient is part of the cached poster, not the live-video plane.
inline Artwork CoverHomePoster(Artwork source,int w,int h,int radius,
                                uint32_t matte=0xFF091B30u)
{
    if(!source.valid()||w<1||h<1||w>1024||h>1024)return {};
    OpaqueArtwork(source,matte);
    Artwork out{w,h,std::vector<uint32_t>(size_t(w)*h)};
    const double scale=std::max(double(w)/source.width,double(h)/source.height);
    const double x0=(source.width-w/scale)/2.0;
    const double y0=(source.height-h/scale)/2.0;
    radius=std::clamp(radius,0,std::min(w,h)/2);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){
        const int dx=std::max(0,radius-std::min(x,w-1-x));
        const int dy=std::max(0,radius-std::min(y,h-1-y));
        auto &pixel=out.argb[size_t(y)*w+x];
        if(dx*dx+dy*dy>radius*radius){pixel=matte;continue;}
        const double sx=std::clamp(x0+(x+.5)/scale-.5,0.0,double(source.width-1));
        const double sy=std::clamp(y0+(y+.5)/scale-.5,0.0,double(source.height-1));
        const int ix=int(sx),iy=int(sy),jx=std::min(ix+1,source.width-1),jy=std::min(iy+1,source.height-1);
        const double fx=sx-ix,fy=sy-iy;
        const double fade=std::clamp((.91-double(y)/std::max(1,h-1))/.33,0.0,1.0);
        pixel=0xFF000000u;
        for(unsigned shift:{0u,8u,16u}){
            auto c=[&](int xx,int yy){return (source.argb[size_t(yy)*source.width+xx]>>shift)&255u;};
            const double v=(c(ix,iy)*(1-fx)+c(jx,iy)*fx)*(1-fy)+(c(ix,jy)*(1-fx)+c(jx,jy)*fx)*fy;
            const double bg=(matte>>shift)&255u;
            pixel|=uint32_t(v*fade+bg*(1-fade)+.5)<<shift;
        }
    }
    return out;
}

// Pure image decoder, independent of VDR. Decode only the small on-screen image.
// Supports PNG/JPEG/SVG, preserves aspect ratio, no network or shell invocations.
Artwork LoadArtwork(const std::string &path, int maxWidth, int maxHeight);
Artwork LoadArtworkBytes(const std::string &bytes,int maxWidth,int maxHeight);

} // namespace suiteosd

#endif
