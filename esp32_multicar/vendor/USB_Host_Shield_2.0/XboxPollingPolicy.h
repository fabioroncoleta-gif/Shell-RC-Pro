#pragma once
#include <stdint.h>
namespace xbox360_polling {
// Query a paired Xbox 360 once every 1000ms to refresh its independently
// reported radio state. When no controller is paired, probe one receiver slot
// every 250ms: all 4 slots are checked once per second, without a USB burst.
// The USB polling path visits only the active HID endpoint while paired.
struct StatusScheduler {
  uint32_t lastQueryMs=0;
  uint8_t nextUnpairedSlot=0;
  static constexpr uint8_t none=0xff;
  uint8_t next(uint32_t now,uint8_t activeSlot) {
    if(activeSlot<4) {
      if(lastQueryMs==0 || uint32_t(now-lastQueryMs)>=1000u){
        lastQueryMs=now?now:1u;
        return activeSlot;
      }
      return none;
    }
    if(lastQueryMs==0 || uint32_t(now-lastQueryMs)>=250u) {
      lastQueryMs=now?now:1u;
      uint8_t s=nextUnpairedSlot;
      nextUnpairedSlot=(nextUnpairedSlot+1u)&3u;
      return s;
    }
    return none;
  }
  void reset() {
    lastQueryMs=0; nextUnpairedSlot=0;
  }
};
inline bool pollInputEndpoint(uint8_t endpointSlot, uint8_t activeSlot) {
  return activeSlot>=4 || endpointSlot==activeSlot;
}
}
