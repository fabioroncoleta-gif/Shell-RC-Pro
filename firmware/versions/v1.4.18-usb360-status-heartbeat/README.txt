V1.4.18 USB360 STATUS HEARTBEAT — 2026-11-20
Arduino compile PASS. ESP32 flash PASS with verified SHA.
Baseline v1.4.17 saved in firmware/versions/v1.4.17-usb360-bounded-hold; v1.4.16 independently saved.
Only Xbox360 reception handling changed; Bluetooth gamepad, Ferrari BLE link, second-car link and drive logic retained unchanged.
Cause: Xbox360 wireless HID reports are event-driven, so artificial 220ms then 900ms loss-of-HID watchdog zeroed a held trigger while receiver was still paired.
Fix: send 50ms software heartbeat with cached last *real* HID state while 0x08 receiver radio responses keep arriving; check receiver radio status every 1000ms; terminate held state after >2300ms without radio response, immediately on explicit radio disconnect/USB host loss.
Validation: native tests for 18s held throttle, neutral stop, missing heartbeat, USB disconnect PASS.
Live 2026-11-20: USB receiver task=90, receiver=1, controllers=01; HID packet counter remained 1 for >39s while receiver status count increased 7 to 42 and IDLE_NEUTRAL=0 throughout. No artificial HID timeout; cars 1 and 2 both READY and sent neutral packets. On 2026-11-20 the owner subsequently confirmed the two Xbox controllers and cars are working, including resolution of Xbox360 signal cuts. This is the preferred stable version.
