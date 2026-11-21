#include <cassert>
#include <iostream>
#include "../src/drive_logic.h"
using namespace shellrc;
int main(){
 DriveModel d;
 RoutedState r;
 r.fresh=true; r.armed=false; r.raw_rt=1023; r.buttons=0;
 auto p=d.tick(r,false,1000);
 assert(p.data[1]==0 && p.data[2]==0 && !p.armed);
 // Even with carReady true, disarmed must not move.
 p=d.tick(r,true,1100);
 assert(p.data[1]==0 && p.data[2]==0);
 r.armed=true; r.raw_steer=300; r.raw_rt=700; r.raw_lt=0;
 p=d.tick(r,true,1150);
 assert(p.armed && p.data[1]==1 && p.data[2]==0 && p.data[4]==1);
 // LB stops traction and keeps original steer.
 r.brake=true;
 p=d.tick(r,true,1200);
 assert(p.handbrake && p.data[1]==0 && p.data[4]==1);
 // releasing brake with same direction should resume
 r.brake=false;
 p=d.tick(r,true,1250);
 assert(p.data[1]==1 && p.data[4]==1);
 // changing gear under LB is blocked
 r.brake=true; p=d.tick(r,true,1300);
 r.brake=false; r.raw_rt=0; r.raw_lt=900; p=d.tick(r,true,1350);
 assert(p.data[2]==0);
 p=d.tick(r,true,1500);assert(p.data[2]==0);
 r.raw_lt=0; p=d.tick(r,true,1550);
 // coast
 assert(p.data[1]==0 && p.data[2]==0);
 // lights A toggle and turbo B
 r.raw_rt=0; r.buttons=1; p=d.tick(r,true,1800);
 assert(p.data[5]==1);
 r.buttons=0;p=d.tick(r,true,1850);
 r.buttons=1;p=d.tick(r,true,1900);
 assert(p.data[5]==0);
 r.buttons=2; p=d.tick(r,true,1950);
 assert(p.turbo);
 r.buttons=0;p=d.tick(r,true,2000);
 r.buttons=32;p=d.tick(r,true,2050);
 assert(p.direct); // RB can no longer enable pulse mode.
 // invalid/stale input kills traction
 r.fresh=false;r.raw_rt=1023; p=d.tick(r,true,2100);
 assert(!p.armed&&p.data[1]==0&&p.data[2]==0);
 // source/connection loss always stops even with armed stale state
 r.fresh=true;r.armed=true;p=d.tick(r,false,2300);
 assert(p.data[1]==0&&p.data[2]==0);
 std::cout<<"PASS: neutral until START, forward, steer, LB hold, gear interlock, lights, turbo, RB, watchdog, disconnect\n";
}
