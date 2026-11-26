#ifndef SUITEOSD_SIDEBAR_ICONS_H
#define SUITEOSD_SIDEBAR_ICONS_H
// Original 24-unit line icons; no external font or asset dependency.
#include <algorithm>
#include <cstdlib>
namespace suiteosd {
template<class Pixel> void DrawSidebarIcon(int id,Pixel pixel) {
 auto line=[&](int x,int y,int X,int Y){
  int dx=std::abs(X-x),sx=x<X?1:-1,dy=-std::abs(Y-y),sy=y<Y?1:-1,err=dx+dy;
  for(;;){pixel(x,y); if(x==X&&y==Y)break; int e=2*err;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
 };
 auto rect=[&](int x,int y,int X,int Y){line(x,y,X,y);line(X,y,X,Y);line(X,Y,x,Y);line(x,Y,x,y);};
 auto circle=[&](int x,int y,int r){for(int i=-r;i<=r;++i)for(int j=-r;j<=r;++j){int d=i*i+j*j;if(d<=r*r&&d>=(r-1)*(r-1))pixel(x+i,y+j);}};
 switch(id){
 case 0: line(2,11,12,2);line(12,2,22,11);rect(5,11,19,21);rect(10,14,14,21);break;
 case 1:rect(2,4,22,18);line(8,22,16,22);line(12,18,12,22);break;
 case 2:rect(3,3,21,21);line(3,9,21,9);line(9,9,9,21);line(9,15,21,15);break;
 case 3:rect(2,5,22,21);line(2,10,22,10);line(8,5,12,10);line(16,5,20,10);line(10,13,16,16);line(16,16,10,19);line(10,19,10,13);break;
 case 4:circle(12,12,9);line(12,6,12,12);line(12,12,17,14);break;
 case 5:circle(9,9,6);line(13,13,20,20);line(9,6,9,12);line(6,9,12,9);break;
 case 6:rect(2,5,22,19);line(8,8,8,16);line(16,8,16,16);line(8,12,16,12);break;
 case 7:rect(3,3,21,21);line(6,7,18,7);line(6,12,18,12);line(6,17,15,17);break;
 case 8:rect(2,3,22,21);circle(7,8,2);line(3,18,10,11);line(10,11,14,16);line(14,16,18,12);line(18,12,22,17);break;
 case 9:rect(2,4,22,20);line(8,7,8,17);line(16,7,16,17);line(2,9,8,9);line(16,9,22,9);line(2,15,8,15);line(16,15,22,15);break;
 case 10:circle(12,12,8);circle(12,12,3);line(12,1,12,4);line(12,20,12,23);line(1,12,4,12);line(20,12,23,12);break;
 case 11:circle(9,9,7);line(14,14,22,22);break;
 case 12:line(3,5,21,5);line(3,12,21,12);line(3,19,21,19);break;
 }
}
}
#endif
