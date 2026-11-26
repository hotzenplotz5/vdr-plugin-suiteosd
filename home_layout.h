#ifndef SUITEOSD_HOME_LAYOUT_H
#define SUITEOSD_HOME_LAYOUT_H
#include <algorithm>
#include <cstdint>
namespace suiteosd {
struct HomeLayout {
    static constexpr int columns=6, rows=3;
    static constexpr int cardWidth=140, cardHeight=210, gap=28;
    static constexpr int left=157, top=83, rowStep=246, titleHeight=30;
    static constexpr int radius=12;
    static constexpr int right=1184, rightWidth=372;
    static constexpr int videoX=1197, videoY=105, videoW=338, videoH=190;
    static constexpr int detailY=354, detailBottom=820;
    static constexpr int cardX(int column){return left+column*(cardWidth+gap);}
    static constexpr int headingY(int row){return top+row*rowStep;}
    static constexpr int firstRow(int section,int sections){
        return std::clamp(section-rows+1,1,std::max(1,sections-rows));
    }
    static constexpr int remainingMinutes(std::int64_t start,std::int64_t end,std::int64_t now){
        return start<=now&&now<end?int((end-now+59)/60):-1;
    }
};
static_assert(HomeLayout::cardWidth*3==HomeLayout::cardHeight*2);
static_assert(HomeLayout::cardX(HomeLayout::columns-1)+HomeLayout::cardWidth<HomeLayout::right);
static_assert(HomeLayout::headingY(2)+HomeLayout::titleHeight+HomeLayout::cardHeight<836);
static_assert(HomeLayout::videoY+HomeLayout::videoH<HomeLayout::detailY);
}
#endif
