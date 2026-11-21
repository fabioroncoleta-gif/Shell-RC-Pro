#include <cassert>
#include <cstdint>
#include "../src/input_core.h"
using namespace shellrc;
int main() {
  Router r;r.setMode(Mode::USB_ONLY);r.enableAutoArm(true);
  GamepadSample pad;pad.connected=true;
  r.record(Source::XBOX360_USB,pad,100);
  r.allowAutoArm(false);
  assert(!r.tick(1500).armed);
  r.allowAutoArm(true);
  r.record(Source::XBOX360_USB,pad,2000);
  assert(!r.tick(2000).armed);
  r.record(Source::XBOX360_USB,pad,2300);
  assert(!r.tick(2540).armed);
  r.record(Source::XBOX360_USB,pad,2600);
  assert(r.tick(2651).armed);
  pad.rt=650;r.record(Source::XBOX360_USB,pad,2660);
  auto active=r.tick(2660);
  assert(active.armed && active.raw_rt==650 && active.drive==650);
  r.allowAutoArm(false);
  assert(!r.tick(2661).armed);
  r.allowAutoArm(true);
  assert(!r.tick(2661).armed);
  pad.rt=0;r.record(Source::XBOX360_USB,pad,2700);
  assert(!r.tick(2700).armed);
  r.record(Source::XBOX360_USB,pad,3100);
  assert(r.tick(3400).armed);
  r.disconnected(Source::XBOX360_USB);
  assert(!r.tick(3410).armed);
  // Manual START behavior remains unchanged unless opted into autoarm.
  Router manual;manual.setMode(Mode::USB_ONLY);
  manual.record(Source::XBOX360_USB,pad,100);
  assert(!manual.tick(1000).armed);
  pad.start=true;manual.record(Source::XBOX360_USB,pad,1001);
  assert(!manual.tick(1001).armed);
  manual.record(Source::XBOX360_USB,pad,1690);
  assert(manual.tick(1800).armed);
  return 0;
}
