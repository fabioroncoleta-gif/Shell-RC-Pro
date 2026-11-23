#pragma once
#include <stdint.h>
namespace shellrc {
// Xbox360 USB receiver reports are often event-driven. Short packet silence
// is not a radio disconnect. Bound every hold by the timestamp of the last
// real USB report, never by the software heartbeat sent to the Router.
static constexpr uint32_t kUsb360MaxReportAgeMs=900;
static constexpr uint32_t kUsb360SoftwareHeartbeatMs=50;
inline bool usb360CanReplay(bool radioPaired,bool validSample,uint32_t reportAgeMs,uint32_t heartbeatAgeMs){
  return radioPaired && validSample &&
         reportAgeMs<=kUsb360MaxReportAgeMs &&
         heartbeatAgeMs>=kUsb360SoftwareHeartbeatMs;
}
inline bool usb360ReportExpired(uint32_t reportAgeMs){
  return reportAgeMs>kUsb360MaxReportAgeMs;
}
}
