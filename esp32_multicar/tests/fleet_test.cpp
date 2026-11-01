#include "../src/fleet_core.h"
#include <cassert>
#include <cstdio>
int main(){using namespace shellrc;Fleet f;uint8_t a[6]={1},b[6]={2},x[6]={3};
 assert(f.observe(x,0,"Headset",-40)==-1);
 assert(f.observe(a,0,"SL-488 Challenge Evo",-60)==0);
 assert(f.choose()==0);
 assert(f.observe(a,0,"SL-488 Challenge Evo",-58)==0&&f.count==1);
 assert(f.observe(b,1,"SHELL SF90",-50)==1);
 assert(f.choose()==-1); // ambiguity never drives arbitrary car
 f.remember(a);assert(f.choose()==0);
 assert(f.select(1)&&f.choose()==1);assert(!f.select(8));
 assert(f.cycle(1)==0&&f.cycle(-1)==0);
 f.beginScan();f.observe(a,0,"SL-488",-60);assert(f.choose()==-1); // explicit missing choice never substitutes
 f.observe(b,1,"SHELL SF90",-40);assert(f.choose()==1);
 f.cars[1].rejected=true;assert(f.choose()==-1&&!f.select(1));
 f.hasManual=false;assert(f.choose()==0);
 f.beginScan();assert(f.choose()==-1);
 f.observe(a,0,"",-50);assert(f.choose()==0); // known address without name
 f.observe(a,0,"SL-name,unsafe=token",-50);assert(!strchr(f.cars[0].name,',')&&!strchr(f.cars[0].name,'='));
 for(int i=3;i<12;i++){uint8_t c[6]={(uint8_t)i};f.observe(c,0,"SL-extra",-80);}assert(f.count==8);
 puts("PASS: discovery, dedup, ambiguity, favorite, explicit selection, missing target, rejection, capacity");}
