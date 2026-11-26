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
        // Wide blue illumination and a concentrated cyan curve in the
        // header only. Do not run a bright diagonal through the posters.
        if(v<71) {
            const double top=-35+.28*(u-590)+.00014*(u-590)*(u-590);
            const double fade=CinemaGaussian(u-690,215);
            c=CinemaMix(c,0xFF0068C8u,.85*fade*CinemaGaussian(v-top,37));
            c=CinemaMix(c,0xFF00B8EFu,.83*CinemaGaussian(u-650,155)*CinemaGaussian(v+9,26));
            c=CinemaMix(c,0xFF30F0FFu,.85*fade*CinemaGaussian(v-top,4));
        }
        if(v>=71&&u<sideRight) {
            c=CinemaMix(c,0xFF031225u,.55);
            // Lower ribbon stays in the sidebar, like the approved reference.
            const double d=v-(696-.34*u+.00022*u*u);
            const double lower=v-(692-.48*u+.00155*u*u);
            c=CinemaMix(c,0xFF005ABAu,.84*CinemaGaussian(d,64));
            c=CinemaMix(c,0xFF008AF2u,.82*CinemaGaussian(d,23));
            c=CinemaMix(c,0xFF00F0FFu,.97*CinemaGaussian(d,5)*CinemaGaussian(u-95,155));
            const double foreground=std::clamp((lower+1.5)/3.0,0.0,1.0);
            c=CinemaMix(c,CinemaMix(0xFF063D78u,0xFF031328u,std::clamp(lower/140,0.0,1.0)),
                foreground*.95);
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
                                 double radius,CinemaStyle style,double scale,int marginLogical=9) {
    if(!bg.valid()||w<=0||h<=0)return {};
    const bool focus=style==CinemaStyle::Focus;
    const bool sidebar=style==CinemaStyle::Sidebar;
    const bool pill=style==CinemaStyle::Pill;
    const int margin=std::max(2,int(std::ceil(marginLogical*scale)));
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
        // Fade to the exact background at the patch boundary. Adjacent
        // opaque patches therefore never leave a hard rectangular edge.
        const double edge=std::clamp(double(std::min({i,j,pw-1-i,ph-1-j}))/std::max(1.0,2.5*scale),0.0,1.0);
        c=CinemaMix(c,0xFF00040Bu,edge*.58*CinemaGaussian(std::max(0.0,sh),3.6*scale));
        if(focus||sidebar)c=CinemaMix(c,0xFF00C9FFu,edge*(focus?.85:.45)*CinemaGaussian(std::max(0.0,d),3.4*scale));
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

// Antialiased native line glyphs in the existing sidebar's exact order.
inline Artwork CinematicSidebarGlyph(int id,int size) {
    struct Segment {double x,y,X,Y;};
    struct Ring {double x,y,r;};
    std::vector<Segment> lines;
    std::vector<Ring> rings;
    auto line=[&](double x,double y,double X,double Y){lines.push_back({x,y,X,Y});};
    auto rect=[&](double x,double y,double X,double Y){line(x,y,X,y);line(X,y,X,Y);line(X,Y,x,Y);line(x,Y,x,y);};
    auto circle=[&](double x,double y,double r){rings.push_back({x,y,r});};
    auto list=[&](){for(int y:{5,12,19}){line(3,y,4,y);line(8,y,21,y);}};
    switch(id) {
    case 0: line(2,11,12,2);line(12,2,22,11);line(5,9,5,21);line(19,9,19,21);line(5,21,19,21);rect(10,14,14,21);break;
    case 1: rect(2,3,22,17);line(12,17,12,22);line(7,22,17,22);break;
    case 2: list();break;
    case 3: circle(12,12,10);circle(12,12,3);break;
    case 4: circle(12,12,9);line(12,6,12,12);line(12,12,17,15);break;
    case 5: circle(9,9,6);line(13,13,21,21);line(6,9,12,9);line(9,6,9,12);break;
    case 6: rect(2,5,22,19);line(8,8,8,16);line(16,8,16,16);line(8,12,16,12);break;
    case 7: rect(3,3,21,21);line(6,7,18,7);line(6,12,18,12);line(6,17,15,17);break;
    case 8: rect(2,3,22,21);circle(7,8,2);line(3,18,10,11);line(10,11,14,16);line(14,16,18,12);line(18,12,22,17);break;
    case 9: rect(2,3,22,21);line(7,3,7,21);line(17,3,17,21);for(int y:{7,12,17}){line(2,y,7,y);line(17,y,22,y);}break;
    case 10: circle(12,12,7.5);circle(12,12,3);
        for(int i=0;i<8;++i){double a=i*3.141592653589793/4;line(12+8*std::cos(a),12+8*std::sin(a),12+10.5*std::cos(a),12+10.5*std::sin(a));}break;
    case 11: circle(9,9,7);line(14,14,22,22);break;
    default: list();break;
    }
    if(size<=0)return {};
    Artwork icon{size,size,std::vector<uint32_t>(size_t(size)*size)};
    for(int y=0;y<size;++y)for(int x=0;x<size;++x){
        const double u=(x+.5)*24/size,v=(y+.5)*24/size;
        double d=100;
        for(const auto &l:lines){
            const double dx=l.X-l.x,dy=l.Y-l.y;
            const double len=dx*dx+dy*dy;
            const double t=len?std::clamp(((u-l.x)*dx+(v-l.y)*dy)/len,0.0,1.0):0;
            d=std::min(d,std::hypot(u-l.x-t*dx,v-l.y-t*dy));
        }
        for(const auto &r:rings)d=std::min(d,std::abs(std::hypot(u-r.x,v-r.y)-r.r));
        const unsigned a=unsigned(255*std::clamp(.5+(.78-d)*size/24,0.0,1.0)+.5);
        icon.argb[size_t(y)*size+x]=(a<<24)|0x00F3FAFFu;
    }
    return icon;
}
inline CinemaPatch CinematicGlyphPatch(const Artwork &bg,const CinemaPatch *parent,
                                      int x,int y,int size,int id) {
    if(!bg.valid()||size<=0||x<0||y<0||x+size>bg.width||y+size>bg.height)return {};
    auto icon=CinematicSidebarGlyph(id,size);
    CinemaPatch out{x,y,std::move(icon)};
    for(int j=0;j<size;++j)for(int i=0;i<size;++i){
        const int gx=x+i,gy=y+j;
        uint32_t under=bg.argb[size_t(gy)*bg.width+gx];
        if(parent&&parent->image.valid()&&gx>=parent->x&&gy>=parent->y&&
           gx-parent->x<parent->image.width&&gy-parent->y<parent->image.height)
            under=parent->image.argb[size_t(gy-parent->y)*parent->image.width+gx-parent->x];
        auto &src=out.image.argb[size_t(j)*size+i];
        src=CinemaMix(under,src,(src>>24)/255.0);
    }
    return out;
}
}
#endif
