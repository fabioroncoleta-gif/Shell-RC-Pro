#include <cassert>
#include "../src/input_core.h"
#include "../src/drive_logic.h"
using namespace shellrc;
int main(){
 Router r;DriveModel motor;
 r.setMode(Mode::USB_ONLY);r.enableAutoArm(true);
 GamepadSample pad;pad.connected=true;
 r.allowAutoArm(true);
 // Valid neutral packet and subsequent neutral periodic receiver samples.
 r.record(Source::XBOX360_USB,pad,100);
 assert(!r.tick(100).armed);
 r.record(Source::XBOX360_USB,pad,350); r.tick(350);
 r.record(Source::XBOX360_USB,pad,600);r.tick(600);
 r.record(Source::XBOX360_USB,pad,800);
 auto authorized=r.tick(800);
 assert(authorized.armed);
 auto idle=motor.tick(authorized,true,800);
 assert(idle.data[1]==0 && idle.data[2]==0 && idle.armed);
 // A fresh RT report generates forward signal.
 pad.rt=1023;r.record(Source::XBOX360_USB,pad,820);
 auto driving=r.tick(820);auto forward=motor.tick(driving,true,820);
 assert(forward.armed && forward.data[1]==1 && forward.data[2]==0);
 // A valid released trigger must send a STOP immediately.
 pad.rt=0;r.record(Source::XBOX360_USB,pad,830);
 auto released=r.tick(830);auto neutral=motor.tick(released,true,830);
 assert(neutral.data[1]==0 && neutral.data[2]==0);
 // A dropped receiver cannot keep sending motion.
 pad.rt=1023;r.record(Source::XBOX360_USB,pad,1000);
 auto active=r.tick(1000);assert(motor.tick(active,true,1000).data[1]==1);
 auto stale=r.tick(1601);auto stopped=motor.tick(stale,true,1601);
 assert(!stale.fresh && stopped.data[1]==0 && stopped.data[2]==0);
 // A lost car link also forces STOP, regardless of RT.
 r.record(Source::XBOX360_USB,pad,1700);
 r.allowAutoArm(false);
 assert(motor.tick(r.tick(1700),false,1700).data[1]==0);
}
