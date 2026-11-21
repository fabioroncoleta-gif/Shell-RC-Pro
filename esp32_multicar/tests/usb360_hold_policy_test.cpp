#include <cassert>
#include "../src/usb360_hold_policy.h"
#include "../src/input_core.h"
#include "../src/drive_logic.h"
using namespace shellrc;
int main(){
  assert(usb360RadioAlive(true,true,10,2300));
  assert(!usb360RadioAlive(true,true,10,2301));
  assert(!usb360RadioAlive(true,true,0,0));
  assert(!usb360RadioAlive(true,false,10,100));
  assert(!usb360RadioAlive(false,true,10,100));
  assert(usb360CanReplay(true,true,50));
  assert(!usb360CanReplay(true,true,49));
  assert(!usb360CanReplay(false,true,50));
  Router r;r.setMode(Mode::USB_ONLY);r.enableAutoArm(true);r.allowAutoArm(true);
  DriveModel drive;GamepadSample in;in.connected=true;
  r.record(Source::XBOX360_USB,in,1);r.tick(1);
  for(unsigned t=51;t<701;t+=50){r.record(Source::XBOX360_USB,in,t);r.tick(t);}
  assert(r.tick(701).armed);
  in.rt=800;r.record(Source::XBOX360_USB,in,750);
  assert(drive.tick(r.tick(750),true,750).data[1]==1);
  // One actual USB input event; periodic independent radio status persists.
  unsigned lastStatus=760;
  for(unsigned t=800;t<=18800;t+=50){
    if(t-lastStatus>=1000)lastStatus=t; // 1s independent radio response
    bool alive=usb360RadioAlive(true,true,lastStatus,t-lastStatus);
    assert(alive);
    if(usb360CanReplay(alive,true,50))r.record(Source::XBOX360_USB,in,t);
    auto state=r.tick(t);
    assert(state.fresh && state.armed && state.raw_rt==800);
    assert(drive.tick(state,true,t).data[1]==1);
  }
  // Receiver stops answering status requests, even though cached radio bit
  // still says paired: must stop after 2300ms.
  const unsigned staleAt=lastStatus+2301;
  assert(!usb360RadioAlive(true,true,lastStatus,2301));
  GamepadSample zero;zero.connected=true;
  r.record(Source::XBOX360_USB,zero,staleAt);
  auto stopped=r.tick(staleAt);
  assert(stopped.raw_rt==0 && drive.tick(stopped,true,staleAt).data[1]==0);
  r.disconnected(Source::XBOX360_USB);
  assert(!r.tick(staleAt+1).fresh);
}
