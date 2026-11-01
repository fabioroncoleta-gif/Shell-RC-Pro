#pragma once
#include <stdint.h>
namespace shellrc {
inline bool canHoldInput(bool hci,bool ready,uint16_t supervisionMs,uint32_t heartbeatAge) {
    return hci && ready && supervisionMs>0 && supervisionMs<=400 && heartbeatAge<=250;
}
}
