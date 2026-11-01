#include "../src/input_core.h"
#include "../src/held_input_policy.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace shellrc;
 assert(canHoldInput(true,true,400,100));
 assert(!canHoldInput(false,true,400,100));
 assert(!canHoldInput(true,false,400,100));
 assert(!canHoldInput(true,true,0,100));
 assert(!canHoldInput(true,true,720,100));
 assert(!canHoldInput(true,true,400,251));
 Router r; GamepadSample s; s.connected=true;s.start=true;
 r.record(Source::BLUETOOTH,s,100);r.tick(100);
 for(unsigned t=150;t<=800;t+=50){r.record(Source::BLUETOOTH,s,t);r.tick(t);}
 assert(r.tick(800).armed); // held START must not toggle repeatedly
 s.start=false;s.rt=700;s.buttons=16;s.lb=true;
 for(unsigned t=850;t<=4000;t+=50){
  if(canHoldInput(true,true,400,100))r.record(Source::BLUETOOTH,s,t);
  auto o=r.tick(t); assert(o.armed && o.raw_rt==700 && o.brake && o.buttons==16);
 }
 assert(!canHoldInput(false,true,400,0));
 assert(!r.tick(4551).armed && !r.tick(4551).fresh);
 puts("PASS: held RT/LB/START for 3s; stale heartbeat, unconfirmed supervision and link loss fail closed");
}
