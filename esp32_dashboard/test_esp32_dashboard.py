import unittest
import time
from tempfile import TemporaryDirectory
from pathlib import Path
import esp32_dashboard as dash

class MonitorTests(unittest.TestCase):
    def setUp(self):
        self.tmp=TemporaryDirectory()
        self.original_log=dash.DASHLOG
        dash.DASHLOG=Path(self.tmp.name)/"testlog.txt"
        self.m=dash.Monitor()
        self.m.set_transport("SERIAL_USB")
    def tearDown(self):
        dash.DASHLOG=self.original_log
        self.tmp.cleanup()
    def test_xbox_ferrari_live(self):
        self.m.consume("BT_DIAG,XBOX_ADS=2,OTHER_ADS=100,RESTARTS=4,LAST_XBOX_MS=3000")
        self.m.consume("CONNECTED,XBOX_BT,Xbox Wireless Controller,slot=0")
        self.m.consume("STATUS,mode=AUTO,source=XBOX_BT,armed=1,fresh=1,host=0,bt_fresh=1")
        self.m.consume("CAR,STATE=READY,ERR=0,SCAN=5,CONN=3,TX_NEUTRAL=170")
        self.m.consume("CAR_DRIVE,ARMED=1,MODE=DIRECT,TX=170")
        self.m.consume("INPUT2,XBOX_BT,-256,700,300,130")
        s=self.m.snapshot(0)["state"]
        self.assertTrue(s["xbox_connected"])
        self.assertTrue(s["xbox_armed"])
        self.assertTrue(s["car_armed"])
        self.assertEqual((s["steer"],s["rt"],s["lt"],s["buttons"]),(-256,700,300,130))
        self.assertEqual((s["xbox_ads"],s["scan_restarts"]), (2,4))
        self.assertEqual(s["car_state"],"READY")
    def test_disconnect_resets_live_controls(self):
        self.m.consume("STATUS,mode=AUTO,source=XBOX_BT,armed=1,fresh=1,host=0,bt_fresh=1")
        self.m.consume("INPUT2,XBOX_BT,400,900,0,64")
        self.m.consume("WATCHDOG,XBOX_BT,DISARMED")
        s=self.m.snapshot(0)["state"]
        self.assertFalse(s["xbox_connected"])
        self.assertFalse(s["xbox_armed"])
        self.assertEqual((s["steer"],s["rt"],s["buttons"]),(0,0,0))
    def test_stale_car_not_reported_connected(self):
        self.m.consume("CAR,STATE=READY,ERR=0,SCAN=1,CONN=1,TX_NEUTRAL=2")
        self.m.state["car_last_seen"]=time.time()-25
        self.assertEqual(self.m.snapshot(0)["state"]["car_state"],"UNKNOWN")
    def test_stale_input_is_zero(self):
        self.m.consume("STATUS,mode=AUTO,source=XBOX_BT,armed=1,fresh=1,host=0,bt_fresh=1")
        self.m.consume("INPUT2,XBOX_BT,300,900,0,1")
        self.m.state["input_last_seen"]-=2
        s=self.m.snapshot(0)["state"]
        self.assertEqual((s["rt"],s["buttons"]),(0,0))
    def test_old_serial_log_format(self):
        self.m.consume("2026-11-01 14:59:09 CAR,STATE=READY,ERR=0,SCAN=1,CONN=2,TX_NEUTRAL=20",from_saved_log=True)
        self.assertEqual(self.m.snapshot(0)["state"]["car_connections"],2)
    def test_input_validates_range(self):
        self.m.consume("INPUT2,XBOX_BT,4000,900,0,1")
        self.assertEqual(self.m.snapshot(0)["state"]["input_age"],-1)
    def test_incremental_logs(self):
        a=self.m.snapshot(0)
        self.m.consume("BT_RECOVERY,BLUEPAD32_SCAN_RESTARTED")
        b=self.m.snapshot(a["last_id"])
        self.assertEqual(len(b["events"]),1)
        self.assertEqual(b["events"][0]["category"],"info")

if __name__ == "__main__":
    unittest.main()
