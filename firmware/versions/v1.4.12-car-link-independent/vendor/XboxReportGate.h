#pragma once
#include <stdint.h>
// Xbox 360 receiver traffic: 2-byte radio attach/detach notifications are
// essential for pairing. Full 18-byte reports update actual button state.
// Never dispatch a NAK or replay the previous buffer on a failed transfer.
namespace xbox360_report_gate {
inline bool shouldHandle(uint8_t transferResult, const uint8_t* data, uint16_t length) {
    if (transferResult != 0 || data == nullptr || length < 2) return false;
    if (data[0] == 0x08) return true; // attach/detach; often only 2 bytes
    if (data[1] == 0x00) return length >= 5; // status
    if (data[1] == 0x01) return length >= 18; // input: two triggers + buttons + sticks
    return false;
}
}
