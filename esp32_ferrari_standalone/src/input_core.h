#pragma once
// Pure C++ logic: compile/test on Linux without ESP32 hardware.
#include <stdint.h>

namespace shellrc {

enum class Source : uint8_t { NONE = 0, BLUETOOTH = 1, XBOX360_USB = 2 };
enum class Mode : uint8_t { AUTO = 0, BLUETOOTH_ONLY = 1, USB_ONLY = 2 };

struct GamepadSample {
    bool connected = false;
    int16_t steer = 0;       // [-512,+512]
    uint16_t rt = 0;        // [0,1023]
    uint16_t lt = 0;        // [0,1023]
    bool start = false;
    bool back = false;
    bool lb = false;
    bool b = false;
    uint16_t buttons = 0;  // bit0:A bit1:B bit2:X bit3:Y bit4:LB bit5:RB bit6:BACK bit7:START
};

struct RoutedState {
    Source source = Source::NONE;
    bool armed = false;
    bool fresh = false;
    bool brake = false;
    int16_t steer = 0;
    int16_t drive = 0;       // signed [-1023,+1023]
    bool turbo_requested = false;
    // Full raw input before arming; serial bridge PC owns arming/interlocks.
    int16_t raw_steer = 0;
    uint16_t raw_rt = 0;
    uint16_t raw_lt = 0;
    uint16_t buttons = 0;
};

// Receiver packets must keep arriving or motor output always goes to zero.
// This class DOES NOT drive GPIOs or transmit to RC vehicles.
class Router {
public:
    static constexpr uint32_t kWatchdogMs = 550;
    static constexpr uint32_t kNeutralArmMs = 650;
    void setMode(Mode m) {
        if (mode_ == m) return;
        mode_ = m;
        reset();
        selected_ = Source::NONE;
    }
    Mode mode() const { return mode_; }
    Source selected() const { return selected_; }

    void record(Source s, const GamepadSample& in, uint32_t now) {
        if (s == Source::BLUETOOTH) {
            bluetooth_ = in; timeBt_ = now; hasBt_ = true;
        } else if (s == Source::XBOX360_USB) {
            usb_ = in; timeUsb_ = now; hasUsb_ = true;
        }
    }
    void disconnected(Source s) {
        if (s == Source::BLUETOOTH) {
            bluetooth_.connected = false; hasBt_ = false;
        } else if (s == Source::XBOX360_USB) {
            usb_.connected = false; hasUsb_ = false;
        }
        if (selected_ == s) { selected_ = Source::NONE; reset(); }
    }
    void reset() {
        armed_ = false;
        armPending_ = false;
        lastStart_ = false;
        lastBack_ = false;
        neutralSince_ = 0;
    }
    RoutedState tick(uint32_t now) {
        Source requested = Source::NONE;
        bool btOk = fresh(bluetooth_, hasBt_, timeBt_, now);
        bool usbOk = fresh(usb_, hasUsb_, timeUsb_, now);
        if (mode_ == Mode::BLUETOOTH_ONLY) {
            if (btOk) requested = Source::BLUETOOTH;
        } else if (mode_ == Mode::USB_ONLY) {
            if (usbOk) requested = Source::XBOX360_USB;
        } else {
            // Priority rule: keep selected source while available.
            // Switch to another controller only after disarming.
            if (selected_ == Source::BLUETOOTH && btOk) requested = selected_;
            else if (selected_ == Source::XBOX360_USB && usbOk) requested = selected_;
            else if (btOk) requested = Source::BLUETOOTH;
            else if (usbOk) requested = Source::XBOX360_USB;
        }
        RoutedState out;
        if (requested != selected_) {
            reset();
            selected_ = requested;
        }
        out.source = requested;
        if (requested == Source::NONE) return out;
        const GamepadSample& in = requested == Source::BLUETOOTH ? bluetooth_ : usb_;
        out.fresh = true;
        out.raw_steer = in.steer;
        out.raw_rt = in.rt;
        out.raw_lt = in.lt;
        out.buttons = in.buttons;
        bool neutral = in.rt < 45 && in.lt < 45 && absSteer(in.steer) < 90;
        bool startEdge = in.start && !lastStart_;
        bool backEdge = in.back && !lastBack_;
        lastStart_ = in.start;
        lastBack_ = in.back;
        if (backEdge) resetArming();
        else if (startEdge) {
            if (armed_) resetArming();
            else if (neutral) {
                armPending_ = true;
                neutralSince_ = now;
            }
        }
        if (armPending_) {
            if (!neutral) armPending_ = false;
            else if (uint32_t(now - neutralSince_) >= kNeutralArmMs) {
                armed_ = true;
                armPending_ = false;
            }
        }
        out.armed = armed_;
        if (!armed_) return out;
        out.steer = deadzone(in.steer, 55);
        out.brake = in.lb;
        out.turbo_requested = in.b && !out.brake;
        if (out.brake || (in.rt > 45 && in.lt > 45)) {
            out.drive = 0;
        } else if (in.rt > 45) out.drive = clampThrottle(in.rt);
        else if (in.lt > 45) out.drive = -clampThrottle(in.lt);
        return out;
    }
private:
    static int absSteer(int16_t n) { return n >= 0 ? n : -n; }
    static int16_t deadzone(int16_t n, int v) { return absSteer(n) < v ? 0 : n; }
    static int16_t clampThrottle(uint16_t x) { return x > 1023 ? 1023 : x; }
    bool fresh(const GamepadSample& i, bool has, uint32_t when, uint32_t now) const {
        return has && i.connected && uint32_t(now - when) <= kWatchdogMs;
    }
    void resetArming() { armed_ = false; armPending_ = false; }
    Mode mode_ = Mode::AUTO;
    Source selected_ = Source::NONE;
    GamepadSample bluetooth_, usb_;
    uint32_t timeBt_ = 0, timeUsb_ = 0, neutralSince_ = 0;
    bool hasBt_ = false, hasUsb_ = false;
    bool armed_ = false, armPending_ = false, lastStart_ = false, lastBack_ = false;
};

}  // namespace shellrc
