#pragma once
// NEXO DUAL RC - host-buildable orchestrator for two independent cars.
// Hardware adapters will deliver validated HID reports + BLE GATT-READY events.
// Does not control GPIO/BLE/SPI: virtual integration only.
#include <stdint.h>
#include "dual_pairing.h"
#include "input_core.h"
#include "drive_logic.h"

namespace shellrc {
struct DualOutput {
  uint32_t controllerId=0;
  uint64_t carId=0;
  bool carReady=false;
  RoutedState route{};
  CarPacket motor{}; // Neutral when anything is invalid.
};

class DualRuntime {
 public:
  DualPairing pair;
  DualOutput outputs[2];
  bool attachController(uint32_t id,Source src,uint32_t now) {
    if(src==Source::NONE)return false;
    int i=pair.controllerConnected(id);
    if(i<0)return false;
    if(ids_[i]!=id) {
      ids_[i]=id;
      kinds_[i]=src;
      routes_[i]=Router();
      drives_[i]=DriveModel();
      drives_[i].resetAll(now);
    }
    return true;
  }
  void controllerLost(uint32_t id,uint32_t now) {
    int slot=slotOf(id);
    if(slot<0)return;
    routes_[slot].disconnected(kinds_[slot]);
    routes_[slot].reset();
    drives_[slot].resetAll(now);
    pair.controllerDisconnected(id);
    ids_[slot]=0;
    kinds_[slot]=Source::NONE;
    outputs[slot]={};
  }
  int carAdvertised(uint64_t macId){return pair.carDiscovered(macId);}
  bool carGattVerified(uint64_t macId,uint32_t now){
    bool wasReady=false;
    for(const auto& slot:pair.slots)
      if(slot.carId==macId && slot.carOnline) wasReady=true;
    if(!pair.verifiedCarReady(macId))return false;
    // A repeated READY status is not a reconnect and must not cancel a race.
    if(!wasReady) for(int i=0;i<2;++i) {
      if(pair.slots[i].carId==macId) {
        routes_[i].reset();
        drives_[i].resetAll(now);
      }
    }
    return true;
  }
  void carLost(uint64_t macId,uint32_t now){
    pair.carDisconnected(macId);
    for(int i=0;i<2;++i) {
      if(pair.slots[i].carId==macId) {
        routes_[i].reset();
        drives_[i].resetAll(now);
      }
    }
  }
  bool submit(uint32_t id,const GamepadSample& report,uint32_t now){
    int i=slotOf(id);
    if(i<0 || !report.connected)return false;
    routes_[i].record(kinds_[i],report,now);
    return true;
  }
  void tick(uint32_t now) {
    for(int i=0;i<2;i++) {
      const auto& slot=pair.slots[i];
      DualOutput& out=outputs[i];
      out.controllerId=slot.controllerId;
      out.carId=slot.carId;
      out.carReady=slot.carOnline;
      out.route=routes_[i].tick(now);
      // Do not interpret arm as permission until GATT and neutral write passed.
      bool valid=slot.controllerId!=0 && slot.carId!=0 &&
                 slot.carOnline && out.route.fresh && out.route.armed;
      if(valid) pair.arm(slot.controllerId);
      else pair.disarm(slot.controllerId);
      out.motor=drives_[i].tick(out.route,valid,now);
    }
  }
 private:
  uint32_t ids_[2]={};
  Source kinds_[2]={Source::NONE,Source::NONE};
  Router routes_[2];
  DriveModel drives_[2];
  int slotOf(uint32_t id)const {
    if(!id)return -1;
    for(int i=0;i<2;i++)if(ids_[i]==id)return i;
    return -1;
  }
};
}
