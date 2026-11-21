#include <cassert>
#include <cstdint>
#include "../vendor/USB_Host_Shield_2.0/XboxReportGate.h"
using xbox360_report_gate::shouldHandle;
int main() {
    uint8_t connect[2]={0x08,0x80};
    uint8_t disconnect[2]={0x08,0x00};
    uint8_t headset[2]={0x08,0x40};
    uint8_t input[18]={0};input[1]=0x01;
    uint8_t status[5]={0};status[1]=0x00;
    assert(shouldHandle(0,connect,2));
    assert(shouldHandle(0,disconnect,2));
    assert(shouldHandle(0,headset,2));
    assert(shouldHandle(0,input,18));
    assert(shouldHandle(0,status,5));
    assert(!shouldHandle(0,input,17));
    assert(!shouldHandle(0,status,4));
    assert(!shouldHandle(4,connect,2)); // NAK never replays prior input
    assert(!shouldHandle(13,input,18)); // bus error never accepted
    assert(!shouldHandle(0,connect,1));
    assert(!shouldHandle(0,nullptr,18));
    return 0;
}
