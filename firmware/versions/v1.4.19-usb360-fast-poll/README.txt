v1.4.19 — USB360 FAST POLL — OFFICIAL STABLE, 2026-11-21
Prior stable baseline v1.4.18 committed + tagged in GitHub; kept as rollback.
Change isolated to vendor Xbox 360 receiver USB polling and request scheduling.
While one gamepad paired, poll 1 interrupt IN endpoint rather than 4; radio status check uses 1 query / second to active slot rather than 4; when no paired controller scan all four slots one at a time 250 ms apart.
No changes to Bluetooth controller code, Ferrari BLE car connections, motor direct mode, safety watchdog, startup arming or car assignment.
Native virtual 30-second polling scheduler test PASS, 18-second held throttle and radio failsafe PASS, USB receiver report gate PASS. Arduino compilation PASS.
Board flashed and SHA verified on 2026-11-20. While USB receiver was enumerated without a paired controller, observed stable USB_MAX_US ~1900-2100us versus prior version ~3700-3900us, excluding ~927ms startup enumeration recovery (present in both versions). Physical validation confirmed by owner on 2026-11-21: Xbox360 response improved and dual Xbox/cars work. Test with wheels suspended before normal use.
