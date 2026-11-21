#include <cassert>
#include <iostream>
#include "../src/drive_logic.h"
using namespace shellrc;
int main(){
 for(int direction: {1,-1})for(int trigger: {140,220,512,900,1023}){
  DriveModel d;d.resetAll(1000);RoutedState r{};r.fresh=true;r.armed=true;
  r.raw_rt=direction>0?trigger:0;r.raw_lt=direction<0?trigger:0;r.raw_steer=300;
  for(unsigned n=0;n<500;n++){
   r.buttons=n%40==0?32:0;auto o=d.tick(r,true,1000+n*10);
   assert(o.direct);assert(o.reverseFull);
   assert(o.data[direction>0?1:2]==1);assert(o.data[direction>0?2:1]==0);
   assert(o.duty==1.f);
  }
  r.brake=true;
  for(unsigned n=0;n<100;n++){
   auto o=d.tick(r,true,6100+n*10);assert(o.handbrake);assert(o.data[1]==0&&o.data[2]==0);assert(o.data[4]==1);
   r.raw_steer=-300;
  }
  r.brake=false;auto o=d.tick(r,true,7200);assert(o.data[direction>0?1:2]==1);
  r.fresh=false;o=d.tick(r,true,7210);assert(!o.armed&&o.data[1]==0&&o.data[2]==0);
  r.fresh=true;o=d.tick(r,false,7220);assert(!o.armed&&o.data[1]==0&&o.data[2]==0);
 }
 std::cout<<"PASS: continuous forward/reverse at five trigger levels, repeated RB, LB neutral/steer, resume and disconnect\n";
}
