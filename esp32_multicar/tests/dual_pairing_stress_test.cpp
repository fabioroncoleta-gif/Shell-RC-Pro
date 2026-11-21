#include "../src/dual_pairing.h"
#include <cassert>
#include <cstdint>
#include <iostream>
using shellrc::DualPairing;
int main(){
  constexpr uint32_t controllers[]={0x10001,0x20001,0x10002,0x20002};
  constexpr uint64_t cars[]={0xAA01,0xAA02,0xAA03,0xAA04,0xAA05};
  uint32_t seed=20261107;
  auto rnd=[&]()->uint32_t { seed ^= seed<<13;seed ^= seed>>17;seed ^= seed<<5;return seed;};
  constexpr int runs=10000;
  for(int i=0;i<runs;i++){
    DualPairing d;
    for(int step=0;step<35;step++){
      int action=rnd()%8, ctl=rnd()%4, car=rnd()%5;
      switch(action){
        case 0: d.controllerConnected(controllers[ctl]);break;
        case 1: d.controllerDisconnected(controllers[ctl]);break;
        case 2: d.carDiscovered(cars[car]);break;
        case 3: d.carDisconnected(cars[car]);break;
        case 4: d.arm(controllers[ctl]);break;
        case 5: d.disarm(controllers[ctl]);break;
        case 6: d.carDiscovered(cars[car]);break; // repeat advertiser
        case 7: d.verifiedCarReady(cars[car]);break; // simulated GATT verified
      }
      const auto&a=d.slots[0],&b=d.slots[1];
      if(a.controllerId&&b.controllerId)assert(a.controllerId!=b.controllerId);
      if(a.carId&&b.carId)assert(a.carId!=b.carId);
      for(const auto&s:d.slots){
        if(!s.controllerId)assert(!s.carId&&!s.armed);
        if(s.carId)assert(s.controllerId);
        if(s.armed)assert(s.carOnline&&s.controllerId&&s.carId);
      }
    }
  }
  std::cout<<"PASS: "<<runs<<" randomized event sequences, 350000 state changes; no duplicate controller/car, no armed-offline, no ownerless car\n";
}
