#include <cassert>
#include <iostream>
#include "../src/input_core.h"
using namespace shellrc;
int main(){
  Router r;
  GamepadSample bt;bt.connected=true;
  auto x=r.tick(0);assert(x.source==Source::NONE&&!x.armed);
  r.record(Source::BLUETOOTH,bt,10); x=r.tick(10);assert(x.source==Source::BLUETOOTH&&!x.armed);
  bt.start=true;r.record(Source::BLUETOOTH,bt,60);x=r.tick(60);assert(!x.armed);
  bt.start=false;r.record(Source::BLUETOOTH,bt,740);x=r.tick(740);assert(x.armed);
  bt.rt=700;bt.steer=200;r.record(Source::BLUETOOTH,bt,760);x=r.tick(760);assert(x.drive==700&&x.steer==200);
  bt.lb=true;r.record(Source::BLUETOOTH,bt,780);x=r.tick(780);assert(x.drive==0&&x.steer==200);
  bt.lb=false;bt.rt=700;bt.lt=800;r.record(Source::BLUETOOTH,bt,800);x=r.tick(800);assert(x.drive==0);
  r.disconnected(Source::BLUETOOTH);x=r.tick(810);assert(x.source==Source::NONE&&!x.armed);
  GamepadSample usb;usb.connected=true;usb.rt=900;
  r.record(Source::XBOX360_USB,usb,900);x=r.tick(900);assert(x.source==Source::XBOX360_USB&&!x.armed);
  usb.rt=0;usb.start=true;r.record(Source::XBOX360_USB,usb,950);r.tick(950);
  usb.start=false;r.record(Source::XBOX360_USB,usb,1610);x=r.tick(1610);assert(x.armed);
  r.record(Source::BLUETOOTH,bt,1620);x=r.tick(1620);assert(x.source==Source::XBOX360_USB && x.armed);
  x=r.tick(2250);assert(!x.armed&&!x.fresh);
  r.setMode(Mode::BLUETOOTH_ONLY);r.record(Source::BLUETOOTH,GamepadSample{},2400);
  x=r.tick(2400);assert(!x.armed);
  r.setMode(Mode::USB_ONLY);r.record(Source::XBOX360_USB,usb,2500);
  x=r.tick(2500);assert(!x.armed);
  std::cout<<"PASS: AUTO source, Xbox BT, Xbox360 USB, START, LB, neutral, disconnect, watchdog, modes\n";
  return 0;
}
