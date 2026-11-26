#ifndef SUITEOSD_HOME_LAYOUT_H
#define SUITEOSD_HOME_LAYOUT_H
#include <algorithm>
#include <cstdint>
namespace suiteosd {
struct HomeLayout {
    static constexpr int columns=6, rows=3;
    static constexpr int cardWidth=132, cardHeight=198, gap=36;
    static constexpr int left=150, top=82, rowStep=252, titleHeight=38;
    static constexpr int radius=16;
    static constexpr int posterW=cardWidth-2, posterH=cardHeight-2;
    static constexpr int right=1156, rightWidth=426;
    static constexpr int liveY=94, liveH=217;
    static constexpr int videoX=1185, videoY=96, videoW=368, videoH=207;
    static constexpr int detailY=327, detailBottom=850;
    static constexpr int cardX(int column,bool expanded=false){return (expanded?282:left)+column*(cardWidth+(expanded?14:gap));}
    static constexpr int headingY(int row){return top+row*rowStep;}
    static constexpr int firstRow(int section,int sections){
        return std::clamp(section-rows+1,1,std::max(1,sections-rows));
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
static_assert(HomeLayout::headingY(2)+HomeLayout::titleHeight+HomeLayout::cardHeight<836);
static_assert(HomeLayout::videoY+HomeLayout::videoH<HomeLayout::detailY);
}
#endif
