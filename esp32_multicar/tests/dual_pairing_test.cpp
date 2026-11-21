#include "../src/dual_pairing.h"
#include <cassert>
#include <iostream>
using shellrc::DualPairing;
int main(){
  constexpr uint32_t BT=0x10001, USB=0x20001, BT2=0x10002;
  constexpr uint64_t A=0xAABBCC01, B=0xAABBCC02, C=0xAABBCC03;
  {
    DualPairing d;
    assert(d.controllerConnected(BT)==0);
    assert(d.carDiscovered(A)==0);
    assert(d.lookup(BT)->carId==A);
    assert(!d.arm(BT)); // advertisement alone cannot arm motor.
    assert(d.controllerConnected(USB)==1);
    assert(d.carDiscovered(B)==1);
    assert(d.lookup(USB)->carId==B);
    assert(!d.arm(USB));
    assert(!d.verifiedCarReady(C)); // cannot verify unknown car
    assert(d.verifiedCarReady(A));
    assert(d.verifiedCarReady(B));
    assert(d.arm(BT) && d.arm(USB));
    assert(d.carDiscovered(B)==1); // repeated advertisement keeps state.
    assert(d.lookup(USB)->armed);
    d.carDisconnected(A);
    assert(!d.lookup(BT)->carOnline && !d.lookup(BT)->armed);
    assert(d.lookup(USB)->carOnline && d.lookup(USB)->armed);
    assert(d.carDiscovered(C)==2);
    assert(d.lookup(BT)->carId==A); // no mid-race car reassignment
    assert(!d.arm(BT));
    d.carDiscovered(A);d.verifiedCarReady(A);
    assert(d.lookup(BT)->carOnline && !d.lookup(BT)->armed);
    d.controllerDisconnected(USB);
    assert(d.lookup(USB)==nullptr);
    assert(d.lookup(BT)->carId==A);
    assert(d.controllerConnected(BT2)==1);
    assert(d.lookup(BT2)->carId==B);
    assert(d.lookup(BT2)->carOnline); // existing ready car reused safely
    assert(!d.lookup(BT2)->armed);
  }
  {
    DualPairing d; // USB-first priority, never permanent BT priority
    d.carDiscovered(A);d.carDiscovered(B);
    assert(d.controllerConnected(USB)==0);
    assert(d.lookup(USB)->carId==A);
    assert(d.controllerConnected(BT)==1);
    assert(d.lookup(BT)->carId==B);
    assert(d.controllerConnected(BT2)==-1);
    assert(d.lookup(USB)->carId!=d.lookup(BT)->carId);
    d.controllerDisconnected(USB);
    assert(d.lookup(BT)->carId==B);
    assert(d.controllerConnected(BT2)==0);
    assert(d.lookup(BT2)->carId==A);
  }
  {
    DualPairing d;
    d.controllerConnected(BT);d.controllerConnected(USB);
    assert(d.lookup(BT)->carId==0 && d.lookup(USB)->carId==0);
    assert(!d.arm(BT));
    d.carDiscovered(A);
    assert(d.lookup(BT)->carId==A && !d.lookup(BT)->carOnline);
    assert(d.lookup(USB)->carId==0);
    d.carDiscovered(B);
    assert(d.lookup(USB)->carId==B);
  }
  std::cout<<"PASS: first-come assignment, unknown car, GATT gating, no takeover, dropout isolation, late connection, USB-first/BT-first\n";
}
