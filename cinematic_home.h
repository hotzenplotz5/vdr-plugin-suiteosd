#ifndef SUITEOSD_CINEMATIC_HOME_H
#define SUITEOSD_CINEMATIC_HOME_H

#include "artwork_decode.h"
#include <cmath>

namespace suiteosd {
// Shared native ARGB raster primitives. All chrome is precomposited onto
// the opaque Home background: no accidental holes into the video plane.
inline uint32_t CinemaMix(uint32_t a,uint32_t b,double t) {
    t=std::clamp(t,0.0,1.0);
    uint32_t c=0xFF000000u;
    for(unsigned s:{0u,8u,16u})
        c|=uint32_t(((a>>s)&255u)*(1-t)+((b>>s)&255u)*t+.5)<<s;
    return c;
}
inline double CinemaGaussian(double d,double sigma) {
    const double v=d/sigma;
    return std::exp(-.5*v*v);
}
inline double CinemaRoundDistance(double x,double y,double w,double h,double r) {
    r=std::clamp(r,0.0,std::min(w,h)*.5);
    const double dx=std::abs(x-w*.5)-w*.5+r;
    const double dy=std::abs(y-h*.5)-h*.5+r;
    return std::hypot(std::max(dx,0.0),std::max(dy,0.0))+std::min(std::max(dx,dy),0.0)-r;
}
inline Artwork CinematicBackground(int w,int h,int sideRight) {
    if(w<=0||h<=0)return {};
    Artwork out{w,h,std::vector<uint32_t>(size_t(w)*h)};
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        const double u=(x+.5)*1600/w,v=(y+.5)*900/h;
        uint32_t c=0xFF031020u;
        c=CinemaMix(c,0xFF082C54u,.55*CinemaGaussian(u-670,530)*CinemaGaussian(v-90,320));
        c=CinemaMix(c,0xFF003D7Cu,.48*CinemaGaussian(u-90,380)*CinemaGaussian(v-710,340));
        // Header ribbon: a broad, curved blue light with a fine cyan edge.
        const double top=-45+.00045*(u-420)*(u-420);
        const double topFade=CinemaGaussian(u-650,290);
        c=CinemaMix(c,0xFF007BCEu,.6*topFade*CinemaGaussian(v-top,35));
        c=CinemaMix(c,0xFF00DBFFu,.7*topFade*CinemaGaussian(v-top-6,3.5));
        if(v>=71&&u<sideRight) {
            c=CinemaMix(c,0xFF031225u,.55);
            // Lower ribbon stays in the sidebar, like the approved reference.
            const double d=v-(675-.24*u+.00022*u*u);
            const double wave= v-(700-.58*u+.0012*u*u);
            c=CinemaMix(c,0xFF0065BDu,.78*CinemaGaussian(d,38));
            c=CinemaMix(c,0xFF00D9FFu,.86*CinemaGaussian(d,5));
            c=CinemaMix(c,0xFF003F94u,.68*CinemaGaussian(wave,19));
        }
        if(v>=71&&u>=sideRight)c=CinemaMix(c,0xFF000B19u,.20);
        if(v>=71&&u>1154)c=CinemaMix(c,0xFF020D1Bu,.32);
        if(v<71)c=CinemaMix(c,0xFF061C39u,.19);
        if(std::abs(v-71)<.8)c=CinemaMix(c,0xFF20618Fu,.65);
        if(v>=71&&(std::abs(u-sideRight)<.8||std::abs(u-1154)<.8))
            c=CinemaMix(c,0xFF1483AFu,.55);
        // Original play-mark, rasterized with the same antialiased edges.
        const double lx=u-46,ly=v-16;
        const double stem=CinemaRoundDistance(lx,ly,10,39,5);
        const double arrow=std::min(
            std::hypot(std::max(0.0,std::abs(lx-20)-8),ly-(9+.48*(lx-12))),
            std::hypot(std::max(0.0,std::abs(lx-20)-8),ly-(30-.48*(lx-12))));
        const double mark=std::max(std::clamp(.5-stem,0.0,1.0),std::clamp(4.5-arrow,0.0,1.0));
        c=CinemaMix(c,CinemaMix(0xFF008EECu,0xFF00F1F7u,1-ly/39),mark);
        out.argb[size_t(y)*w+x]=c;
    }
    return out;
}
enum class CinemaStyle { Poster, Focus, Panel, Sidebar, Pill };
struct CinemaPatch { int x=0,y=0; Artwork image; };
// Frame shadow and halo are continuous signed-distance fields, rather
// than concentric opaque rectangles. The output is cached by geometry.
inline CinemaPatch CinematicFrame(const Artwork &bg,int x,int y,int w,int h,
                                 double radius,CinemaStyle style,double scale) {
    if(!bg.valid()||w<=0||h<=0)return {};
    const bool focus=style==CinemaStyle::Focus;
    const bool sidebar=style==CinemaStyle::Sidebar;
    const bool pill=style==CinemaStyle::Pill;
    const int margin=std::max(2,int(std::ceil(9*scale)));
    CinemaPatch out;
    out.x=std::max(0,x-margin);out.y=std::max(0,y-margin);
    const int pw=std::min(bg.width,x+w+margin)-out.x;
    const int ph=std::min(bg.height,y+h+margin)-out.y;
    if(pw<=0||ph<=0)return {};
    out.image={pw,ph,std::vector<uint32_t>(size_t(pw)*ph)};
    const double border=(focus?3.5:sidebar?1.8:1.2)*scale;
    for(int j=0;j<ph;++j)for(int i=0;i<pw;++i) {
        const int gx=out.x+i,gy=out.y+j;
        const double lx=gx+.5-x,ly=gy+.5-y;
        const double d=CinemaRoundDistance(lx,ly,w,h,radius);
        const double sh=CinemaRoundDistance(lx,ly-3*scale,w,h,radius);
        uint32_t c=bg.argb[size_t(gy)*bg.width+gx];
        c=CinemaMix(c,0xFF00040Bu,.68*CinemaGaussian(std::max(0.0,sh),4*scale));
        if(focus||sidebar)c=CinemaMix(c,0xFF00C9FFu,(focus?.83:.45)*CinemaGaussian(std::max(0.0,d),3.7*scale));
        const double t=std::clamp(ly/h,0.0,1.0);
        const uint32_t rim=focus?0xFF20ECFFu:sidebar?0xFF00BDEAu:
            CinemaMix(0xFF859BB0u,0xFF36536Eu,t);
        const uint32_t inside=sidebar?CinemaMix(0xFF0889ADu,0xFF053452u,t):
            pill?0xFF102C41u:style==CinemaStyle::Panel?CinemaMix(0xFF102B48u,0xFF041426u,t):
            0xFF061626u;
        c=CinemaMix(c,rim,std::clamp(.5-d,0.0,1.0));
        c=CinemaMix(c,inside,std::clamp(.5-d-border,0.0,1.0));
        out.image.argb[size_t(j)*pw+i]=c;
    }
    return out;
}
// Place artwork inside a frame, preserving its antialiased border.
// Art is already decoded/cropped/faded by the existing artwork cache.
inline void CinematicArtwork(CinemaPatch &patch,const Artwork &art,int x,int y,
                            int frameX,int frameY,int frameW,int frameH,
                            double radius,double inset) {
    if(!patch.image.valid()||!art.valid())return;
    for(int j=0;j<art.height;++j)for(int i=0;i<art.width;++i) {
        const int gx=x+i,gy=y+j,px=gx-patch.x,py=gy-patch.y;
        if(px<0||py<0||px>=patch.image.width||py>=patch.image.height)continue;
        const double d=CinemaRoundDistance(gx+.5-frameX,gy+.5-frameY,frameW,frameH,radius);
        const uint32_t src=art.argb[size_t(j)*art.width+i];
        auto &dst=patch.image.argb[size_t(py)*patch.image.width+px];
        dst=CinemaMix(dst,src,std::clamp(.5-d-inset,0.0,1.0)*(src>>24)/255.0);
    }
}
inline void CinematicLogo(Artwork &poster,const Artwork &logo,int pad) {
    if(!poster.valid()||!logo.valid()||logo.width+pad*2>poster.width||logo.height+pad*2>poster.height)return;
    const int left=poster.width-logo.width-pad;
    for(int y=0;y<logo.height;++y)for(int x=0;x<logo.width;++x) {
        const auto src=logo.argb[size_t(y)*logo.width+x];
        auto &dst=poster.argb[size_t(y+pad)*poster.width+left+x];
        const double a=(src>>24)/255.0;
        dst=CinemaMix(CinemaMix(dst,0xFF031120u,a*.28),src,a);
    }
}
}
#endif
