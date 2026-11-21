#pragma once
#include <stdint.h>
namespace shellrc {
// Match Bluetooth's software heartbeat model: an unchanged Xbox360 HID
// state remains valid while the receiver's *independent* radio-status
// messages prove that the controller is still paired and replying.
// Do not confuse event-driven button reports with radio disconnection.
static constexpr uint32_t kUsb360RadioStatusDeadlineMs=2300;
static constexpr uint32_t kUsb360SoftwareHeartbeatMs=50;
inline bool usb360RadioAlive(bool hostReady,bool radioPaired,
                            uint32_t lastRadioStatusMs,uint32_t statusAgeMs){
  return hostReady && radioPaired && lastRadioStatusMs!=0 &&
         statusAgeMs<=kUsb360RadioStatusDeadlineMs;
}
inline bool usb360CanReplay(bool radioAlive,bool validSample,uint32_t heartbeatAgeMs){
  return radioAlive && validSample &&
         heartbeatAgeMs>=kUsb360SoftwareHeartbeatMs;
}
}
