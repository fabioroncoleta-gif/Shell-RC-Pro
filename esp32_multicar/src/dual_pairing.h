#pragma once
// NEXO DUAL RC: pure C++ assignment state machine; never commands a motor.
// Controller registration order sets player slot 0 then 1.
// The first two *discovered* cars reserve those slots in discovery order.
// Car READY is separate: requires successful BLE GATT FFF1 + initial neutral
// write from the future hardware adapter, NOT merely an advertising packet.
#include <stdint.h>

namespace shellrc {
struct DualSlot {
  uint32_t controllerId=0;
  uint64_t carId=0;
  bool carOnline=false; // Set only by verifiedCarReady() after BLE validation.
  bool armed=false;
};

class DualPairing {
 public:
  static constexpr int kControllers=2;
  static constexpr int kCars=8;
  struct SeenCar {uint64_t id=0;bool seen=false;bool online=false;};
  DualSlot slots[kControllers];
  SeenCar fleet[kCars];
  int fleetCount=0;

  int controllerConnected(uint32_t id) {
    if (!id) return -1;
    for (int i=0;i<kControllers;++i)
      if (slots[i].controllerId==id) return i;
    for (int i=0;i<kControllers;++i)
      if (!slots[i].controllerId) {
        slots[i]={};
        slots[i].controllerId=id;
        assignWaiting();
        return i;
      }
    return -1;
  }
  int carDiscovered(uint64_t id) {
    if (!id) return -1;
    int i=findCar(id);
    if (i<0) {
      if (fleetCount>=kCars) return -1;
      i=fleetCount++;
      fleet[i].id=id;
      fleet[i].seen=false;
      fleet[i].online=false;
    }
    fleet[i].seen=true;
    // Repeated advertising never grants READY or rearms.
    assignWaiting();
    return i;
  }
  bool verifiedCarReady(uint64_t id) {
    int i=findCar(id);
    if (i<0 || !fleet[i].seen) return false;
    fleet[i].online=true;
    for(auto& slot:slots) if(slot.carId==id) {
      if(!slot.carOnline) slot.armed=false;
      slot.carOnline=true;
    }
    assignWaiting();
    return true;
  }
  void controllerDisconnected(uint32_t id) {
    for(auto& slot:slots) if (id && slot.controllerId==id) {
      slot={}; // exclusive reservation released
      break;
    }
    assignWaiting();
  }
  void carDisconnected(uint64_t id) {
    int j=findCar(id);
    if(j>=0){fleet[j].online=false;fleet[j].seen=false;}
    for(auto& slot:slots) if(slot.carId==id) {
      slot.carOnline=false;
      slot.armed=false;
    }
    // Retain reservation of the same car. No surprise switch during dropout.
  }
  bool arm(uint32_t id) {
    for(auto& slot:slots) if(id && slot.controllerId==id) {
      if(slot.carId && slot.carOnline) {slot.armed=true;return true;}
    }
    return false;
  }
  void disarm(uint32_t id) {
    for(auto& slot:slots) if(id && slot.controllerId==id)slot.armed=false;
  }
  const DualSlot* lookup(uint32_t id) const {
    for(const auto& slot:slots)if(id && slot.controllerId==id)return &slot;
    return nullptr;
  }
 private:
  int findCar(uint64_t id) const {
    for(int i=0;i<fleetCount;i++)if(fleet[i].id==id)return i;
    return -1;
  }
  bool assigned(uint64_t id) const {
    for(const auto& slot:slots)if(slot.carId==id)return true;
    return false;
  }
  void assignWaiting() {
    for(auto& slot:slots) {
      if(!slot.controllerId||slot.carId)continue;
      for(int j=0;j<fleetCount;j++) {
        if(fleet[j].seen && !assigned(fleet[j].id)) {
          slot.carId=fleet[j].id;
          slot.carOnline=fleet[j].online;
          slot.armed=false; // Every new assignment must be manually armed.
          break;
        }
      }
    }
  }
};
}
