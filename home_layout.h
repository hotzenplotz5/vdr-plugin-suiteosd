#ifndef SUITEOSD_HOME_LAYOUT_H
#define SUITEOSD_HOME_LAYOUT_H
#include <algorithm>
#include <cstdint>
namespace suiteosd {
struct PreviewRect { int x,y,w,h; };
// Fit an even-pixel video rectangle strictly inside the reserved screen slot.
// NV12/P010 providers can align down; requesting even coordinates avoids
// rejecting a valid preview merely because the OSD origin is odd.
constexpr PreviewRect AlignPreviewRect(int x,int y,int w,int h) {
    const int ax=x+(x&1), ay=y+(y&1);
    return {ax,ay,std::max(0,(w-(ax-x))&~1),std::max(0,(h-(ay-y))&~1)};
}
struct HomeLayout {
    static constexpr int columns=5, rows=3;
    static constexpr int cardWidth=140, cardHeight=210, gap=20, expandedGap=14;
    static constexpr int left=150, top=82, viewportBottom=850, rowStep=(viewportBottom-top)/rows, titleHeight=32;
    static constexpr int radius=16;
    static constexpr int posterW=cardWidth-2, posterH=cardHeight-2;
    static constexpr int right=1156, rightWidth=426;
    static constexpr int liveY=94, liveH=247;
    static constexpr int videoW=320, videoH=180, videoY=100;
    static constexpr int videoX=right+(rightWidth-videoW)/2;
    static constexpr int detailY=355, detailBottom=850;
    static constexpr int cardX(int column,bool expanded=false){return (expanded?282:left)+column*(cardWidth+(expanded?expandedGap:gap));}
    static constexpr int headingY(int row){return top+row*rowStep;}
    static constexpr int firstRow(int section,int sections,int first=1){
        const int last=std::max(1,sections-rows);
        if(section==0)return 1;
        first=std::clamp(first,1,last);
        if(section<first)return std::clamp(section,1,last);
        if(section>=first+rows)return std::clamp(section-rows+1,1,last);
        return first;
    }
    static constexpr int scrollDurationMs=240;
    static constexpr int scrollOffset(int from,int to,int elapsedMs){
        const double t=double(std::clamp(elapsedMs,0,scrollDurationMs))/scrollDurationMs;
        const double eased=t*t*(3.0-2.0*t);
        return from+int((to-from)*eased);
    }
    static constexpr int progressPercent(std::int64_t start,std::int64_t end,std::int64_t now){
        return end<=start||now<=start?0:now>=end?100:int(100.0*(now-start)/(end-start));
    }
    static constexpr int remainingMinutes(std::int64_t start,std::int64_t end,std::int64_t now){
        return start<=now&&now<end?int((end-now+59)/60):-1;
    }
};
static_assert(HomeLayout::cardWidth*3==HomeLayout::cardHeight*2);
static_assert(HomeLayout::cardX(HomeLayout::columns-1)+HomeLayout::cardWidth<HomeLayout::right);
static_assert(HomeLayout::cardX(HomeLayout::columns-1,true)+HomeLayout::cardWidth<HomeLayout::right);
static_assert(HomeLayout::headingY(HomeLayout::rows-1)+HomeLayout::titleHeight+HomeLayout::cardHeight+9<850);
static_assert(HomeLayout::headingY(HomeLayout::rows)==HomeLayout::viewportBottom);
static_assert(HomeLayout::rowStep>=HomeLayout::titleHeight+HomeLayout::cardHeight+14);
static_assert(HomeLayout::cardX(HomeLayout::columns-1,true)+HomeLayout::cardWidth+7<=HomeLayout::right-9);
static_assert(HomeLayout::gap>=18 && HomeLayout::expandedGap>=14);
static_assert(HomeLayout::videoY+HomeLayout::videoH<=HomeLayout::liveY+HomeLayout::liveH-56);
static_assert(HomeLayout::videoW*9==HomeLayout::videoH*16);
static_assert(HomeLayout::liveY+HomeLayout::liveH<HomeLayout::detailY);
}
#endif
