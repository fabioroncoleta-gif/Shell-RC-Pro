#include "../src/dual_rc_runtime.h"
#include <cassert>
#include <iostream>
using namespace shellrc;

static GamepadSample input(bool start=false,uint16_t rt=0,uint16_t lt=0,int16_t steer=0) {
  GamepadSample p; p.connected=true;p.start=start;p.rt=rt;p.lt=lt;p.steer=steer;
  p.buttons=start?128u:0u;
  return p;
}
int main(){
  constexpr uint32_t BT=0x10001, USB=0x20001;
  constexpr uint64_t RED=0xAABBCC01, YELLOW=0xAABBCC02;
  {
    DualRuntime d;
    assert(d.attachController(BT,Source::BLUETOOTH,0));
    d.carAdvertised(RED); d.tick(50);
    assert(d.outputs[0].carId==RED && !d.outputs[0].carReady);
    d.submit(BT,input(true,1000),100); d.tick(100);
    assert(!d.outputs[0].motor.armed); // not GATT ready
    assert(d.carGattVerified(RED,200)); // first neutral successfully sent
    assert(d.attachController(USB,Source::XBOX360_USB,210));
    d.carAdvertised(YELLOW);
    d.tick(220);
    assert(d.outputs[1].carId==YELLOW && !d.outputs[1].carReady);
    assert(d.carGattVerified(YELLOW,230));
    // Explicit START-in-neutral separately, per controller.
    d.submit(BT,input(false),240);d.submit(USB,input(false),240);d.tick(240);
    d.submit(BT,input(true),250); d.submit(USB,input(true),250); d.tick(250);
    d.submit(BT,input(false),800);d.submit(USB,input(false),800);d.tick(800);
    assert(!d.outputs[0].motor.armed && !d.outputs[1].motor.armed);
    d.submit(BT,input(false),960);d.submit(USB,input(false),960);d.tick(960);
    assert(d.outputs[0].motor.armed && d.outputs[1].motor.armed);
    // independent throttle and steering
    d.submit(BT,input(false,1023,0,-400),1000);
    d.submit(USB,input(false,0,1023,400),1000);d.tick(1000);
    d.submit(BT,input(false,1023,0,-400),1200);
    d.submit(USB,input(false,0,1023,400),1200);d.tick(1200);
    assert(d.outputs[0].motor.armed && d.outputs[1].motor.armed);
    // Repeated READY from an adapter must not revoke a running driver's arm.
    assert(d.carGattVerified(YELLOW,1200)); d.tick(1201);
    assert(d.outputs[1].motor.armed);
    assert(d.outputs[0].route.raw_rt==1023 && d.outputs[1].route.raw_lt==1023);
    assert(d.outputs[0].route.raw_steer==-400 && d.outputs[1].route.raw_steer==400);
    // car1 drops: car2 stays armed with fresh reports
    d.carLost(RED,1210);
    d.submit(USB,input(false,1023,0,300),1220); d.tick(1220);
    assert(!d.outputs[0].motor.armed && d.outputs[1].motor.armed);
    assert(!d.outputs[0].carReady && d.outputs[1].carReady);
    // car1 repeat adverts must not rearm until verified GATT+neutral
    d.carAdvertised(RED);d.tick(1250);
    assert(!d.outputs[0].motor.armed && !d.outputs[0].carReady);
    assert(d.carGattVerified(RED,1280));d.tick(1280);
    assert(!d.outputs[0].motor.armed);
    // first controller disconnects, second stays independent
    d.controllerLost(BT,1300);
    d.submit(USB,input(false,1023),1310);d.tick(1310);
    assert(!d.outputs[0].motor.armed && d.outputs[1].motor.armed);
    // no data from second for >550ms -> fails closed
    d.tick(2000);
    assert(!d.outputs[1].motor.armed && !d.outputs[1].route.fresh);
    assert(!d.outputs[1].motor.data[1]&&!d.outputs[1].motor.data[2]);
  }
  {
    DualRuntime d;
    d.carAdvertised(RED);d.carAdvertised(YELLOW);
    assert(d.attachController(USB,Source::XBOX360_USB,10));
    assert(d.attachController(BT,Source::BLUETOOTH,11));
    assert(d.outputs[0].controllerId==0); // outputs created when ticked
    d.tick(12);
    assert(d.outputs[0].controllerId==USB && d.outputs[0].carId==RED);
    assert(d.outputs[1].controllerId==BT && d.outputs[1].carId==YELLOW);
  }
  std::cout<<"PASS: independent dual drive, START neutral, GATT gate, reverse, discovery, loss isolation, watchdog, USB-first\n";
}
