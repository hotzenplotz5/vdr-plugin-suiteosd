#include "../navigation.h"
#include "../home_cache.h"
#include <cassert>
#include <string>

int main()
{
    using namespace suiteosd;
    static_assert(kHomePosterWorkers==4);
    HomeLru<int,int> cache;
    cache.emplace(1,10);cache.emplace(2,20);cache.emplace(3,30);
    const auto *retained=&cache.find(1)->second;
    cache.evictOldest();assert(cache.size()==2&&cache.find(2)==cache.end());
    assert(&cache.find(1)->second==retained&&*retained==10);
    cache.insertOrAssign(1,11);assert(&cache.find(1)->second==retained&&*retained==11);
    cache.eraseIf([](int k,int){return k==3;});assert(cache.size()==1);
    cache.clear();assert(cache.size()==0);
    for(auto size:{PreviewRect{0,0,1280,720},PreviewRect{0,0,1920,1080},PreviewRect{0,0,3840,2160}}){
        const auto full=FullscreenOsdRect(size.w,size.h,{96,54,1728,972});
        assert(full.x==0&&full.y==0&&full.w==size.w&&full.h==size.h);
        assert(InclusiveOsdEnd(1599,1600,full.w)==full.w-1);
        assert(InclusiveOsdEnd(899,900,full.h)==full.h-1);
        for(bool expanded:{false,true}){
            const auto video=AlignPreviewRect(HomeLayout::videoX(expanded)*full.w/1600,HomeLayout::videoY*full.h/900,
                HomeLayout::videoW*full.w/1600,HomeLayout::videoH*full.h/900);
            assert(video.x>=0&&video.y>=0&&video.x+video.w<=full.w&&video.y+video.h<=full.h);
        }
    }
    const auto fallback=FullscreenOsdRect(0,0,{96,54,1728,972});
    assert(fallback.x==96&&fallback.y==54&&fallback.w==1728&&fallback.h==972);
    Navigation n;
    assert(n.section == 0 && !n.sideFocused);
    n.card[0]=0;
    n.Move(Direction::Up, 3, 6);
    assert(n.sideFocused); // collapsed icon rail expands all labels when focused
    n.Move(Direction::Down, 1, 5);
    assert(n.sideIndex == 1);
    n.Move(Direction::Right, 1, 5);
    assert(!n.sideFocused); // exit sidebar without activating
    n.Move(Direction::Down, 9, 4);
    assert(n.section == 1);
    for (int i = 0; i < 7; ++i)
        n.Move(Direction::Right, 9, 4);
    assert(n.card[1] == 7 && n.firstCard[1] == 4);
    n.Move(Direction::Down, 3, 4);
    assert(n.section == 2);
    assert(n.firstCard[1] == 4);
    n.Move(Direction::Down, 6, 4);
    assert(n.section == Navigation::kNew && n.firstVisibleSection == 1);
    assert(n.Move(Direction::Select, 6, 4) == Activation::Recording);
    n.Move(Direction::Down,6,4);
    assert(n.section==Navigation::kContinue && n.firstVisibleSection==2 && n.Move(Direction::Select,6,4)==Activation::HomeCard);
    n.Move(Direction::Left, 6, 4);
    assert(n.sideFocused);
    n.Move(Direction::Up, 6, 4);
    assert(n.sideIndex == 0);
    // Every icon remains focusable; the last entry is global search.
    n.Move(Direction::Down, 6, 4);
    for (int i = 0; i < 20; ++i) n.Move(Direction::Down, 6, 4);
    assert(n.sideIndex == Navigation::kSidebarItems - 1);
    assert(n.sideIndex == Navigation::kClassicMenuItem);
    n.Move(Direction::Up, 6, 4);
    assert(n.sideIndex == Navigation::kSearchItem);
    for (int i = 0; i < 20; ++i) n.Move(Direction::Up, 6, 4);
    assert(n.sideIndex == 0);
    assert(n.Move(Direction::Select, 1, 4) == Activation::None);
    assert(n.section == 0 && !n.sideFocused);
    assert(n.Move(Direction::Back, 1, 4) == Activation::Close);

    Navigation posterRails;
    posterRails.Move(Direction::Down, 12, Navigation::kHomeVisibleCards);
    for(int i=0;i<6;++i)posterRails.Move(Direction::Right,12,Navigation::kHomeVisibleCards);
    assert(posterRails.card[1]==6 && posterRails.firstCard[1]==1);
    static_assert(HomeLayout::columns==6 && HomeLayout::rows==3);
    static_assert(HomeLayout::cardWidth==136 && HomeLayout::cardHeight==204);
    static_assert(HomeLayout::cardWidth*3==HomeLayout::cardHeight*2);
    for(bool expanded:{false,true}){
        for(int i=1;i<HomeLayout::visibleColumns(expanded);++i)
            assert(HomeLayout::cardX(i,expanded)-HomeLayout::cardX(i-1,expanded)-HomeLayout::cardWidth==(expanded?14:20));
        assert(HomeLayout::cardX(HomeLayout::visibleColumns(expanded)-1,expanded)+HomeLayout::cardWidth+7<=HomeLayout::right-9);
    }
    posterRails.Move(Direction::Down,12,Navigation::kHomeVisibleCards);
    posterRails.Move(Direction::Down,12,Navigation::kHomeVisibleCards);
    assert(posterRails.section==Navigation::kNew && posterRails.firstVisibleSection==1);
    posterRails.Move(Direction::Down,12,Navigation::kHomeVisibleCards);
    assert(posterRails.section==Navigation::kContinue && posterRails.firstVisibleSection==2);
    posterRails.Move(Direction::Up,12,Navigation::kHomeVisibleCards);
    assert(posterRails.firstVisibleSection==2); // focus stays inside the viewport
    posterRails.Move(Direction::Up,12,Navigation::kHomeVisibleCards);
    assert(posterRails.firstVisibleSection==2);
    posterRails.Move(Direction::Up,12,Navigation::kHomeVisibleCards);
    assert(posterRails.firstVisibleSection==1);
    assert(posterRails.card[1]==6 && posterRails.firstCard[1]==1);
    static_assert(HomeLayout::videoY+HomeLayout::videoH<=HomeLayout::liveY+HomeLayout::liveH-32);
    static_assert(HomeLayout::videoW*9==HomeLayout::videoH*16);
    // An odd OSD origin must not cause the output provider's chroma
    // alignment to reject the whole live preview.
    for(int x:{1209,1210})for(int y:{99,100})for(int w:{319,320})for(int h:{179,180}){
        const auto r=AlignPreviewRect(x,y,w,h);
        assert((r.x|r.y|r.w|r.h)%2==0);
        assert(r.x>=x&&r.y>=y&&r.x+r.w<=x+w&&r.y+r.h<=y+h);
        assert(r.w>=w-2&&r.h>=h-2);
    }
    assert(HomeLayout::firstRow(0,10)==0);
    assert(HomeLayout::firstRow(2,10)==0);
    assert(HomeLayout::firstRow(3,10)==1);
    assert(HomeLayout::firstRow(4,10)==2);
    assert(HomeLayout::firstRow(9,10)==7);
    assert(HomeLayout::firstRow(7,10,7)==7);
    assert(HomeLayout::firstRow(6,10,7)==6);
    assert(HomeLayout::firstRow(0,10,7)==0);
    assert(HomeLayout::headingY(3)==HomeLayout::viewportBottom);
    for(int from:{0,256}){
        const int to=256-from;
        assert(HomeLayout::scrollOffset(from,to,0)==from);
        assert(HomeLayout::scrollOffset(from,to,120)==128);
        assert(HomeLayout::scrollOffset(from,to,240)==to);
        assert(HomeLayout::scrollOffset(from,to,999)==to);
        int previous=from;
        for(int ms=1;ms<=240;++ms){
            int current=HomeLayout::scrollOffset(from,to,ms);
            assert(from<to?current>=previous:current<=previous);
            previous=current;
        }
    }
    Navigation all;
    for(int section=1;section<Navigation::kSections;++section){
        all.Move(Direction::Down,12,5);
        assert(all.section==section);
        assert(section>=all.firstVisibleSection&&section<all.firstVisibleSection+HomeLayout::rows);
    }
    assert(all.firstVisibleSection==7);
    for(int section=8;section>=1;--section){
        all.Move(Direction::Up,12,5);
        assert(all.section==section);
        assert(section>=all.firstVisibleSection&&section<all.firstVisibleSection+HomeLayout::rows);
    }
    assert(all.firstVisibleSection==1);
    assert(HomeLayout::remainingMinutes(100,5500,2800)==45);
    assert(HomeLayout::remainingMinutes(100,5500,5500)==-1);
    assert(HomeLayout::remainingMinutes(100,5500,99)==-1);
    assert(HomeLayout::progressPercent(100,5500,2800)==50);
    assert(HomeLayout::progressPercent(100,5500,5500)==100);
    assert(HomeLayout::progressPercent(100,5500,99)==0);
    assert(HomeLayout::progressPercent(0,0,99)==0);

    for(bool expanded:{false,true}){
        assert(HomeLayout::heroX(expanded)-HomeLayout::cardX(0,expanded)-HomeLayout::heroGap==HomeLayout::neighborW(expanded));
        assert(HomeLayout::heroRight-HomeLayout::nextX(expanded)==HomeLayout::neighborW(expanded));
        assert(HomeLayout::videoX(expanded)+HomeLayout::videoW<HomeLayout::nextX(expanded));
    }
    Navigation hero;
    assert(hero.card[0]==0&&hero.firstVisibleSection==0);
    hero.Move(Direction::Left,123,6);assert(hero.card[0]==122&&!hero.sideFocused);
    hero.Move(Direction::Right,123,6);assert(hero.card[0]==0);
    for(int i=0;i<250;++i)hero.Move(Direction::Right,123,6);
    assert(hero.card[0]==4&&!hero.sideFocused&&hero.firstCard[0]==0);
    for(int i=0;i<250;++i)hero.Move(Direction::Left,123,6);
    assert(hero.card[0]==0&&!hero.sideFocused);
    hero.Move(Direction::Right,0,6);assert(hero.card[0]==0);
    hero.Move(Direction::Left,1,6);assert(hero.card[0]==0);
    hero.Move(Direction::Right,123,6);
    hero.Move(Direction::Down,12,6);hero.Move(Direction::Up,123,6);
    assert(hero.card[0]==1);
    assert(hero.Move(Direction::Select,123,6)==Activation::Hero);
    hero.Move(Direction::Up,123,6);assert(hero.sideFocused);
    hero.Move(Direction::Right,123,6);assert(!hero.sideFocused&&hero.card[0]==1);
    assert(HomeLayout::channelIndex(0,-1,123)==122);
    assert(HomeLayout::channelIndex(122,1,123)==0);
    Navigation direct;
    direct.card[0]=0;
    direct.Move(Direction::Up, 3, 6);
    direct.Move(Direction::Down, 1, 4);
    direct.Move(Direction::Down, 1, 4);
    direct.Move(Direction::Down, 1, 4);
    assert(direct.sideIndex == 3);
    assert(direct.Move(Direction::Select, 6, 4) == Activation::Module);
    assert(direct.sideIndex == 3); // recordings native VDR menu
    assert(!direct.sideFocused);
    assert(direct.section == 0); // Suite Home stays unchanged

    // Every non-Home icon calls its own native menu/plug-in adapter,
    // not another location of the Home carousel.
    for (int index = 1; index < Navigation::kSidebarItems; ++index) {
        Navigation menu;
        menu.card[0]=0;
        menu.Move(Direction::Up, 3, 6);
        for (int j = 0; j < index; ++j)
            menu.Move(Direction::Down, 1, 4);
        assert(menu.sideIndex == index);
        assert(menu.Move(Direction::Select, 1, 4) == Activation::Module);
    }
    suiteosd::HomeLru<int,std::string> weighted;
    weighted.emplace(1,std::string("abc"));weighted.emplace(2,std::string("defgh"));
    const auto weight=[](const std::string &v){return v.size();};
    assert(weighted.totalWeight(weight)==8);
    weighted.find(1);weighted.evictOldest();assert(weighted.totalWeight(weight)==3);
    weighted.clear();assert(weighted.totalWeight(weight)==0);
    return 0;
}
