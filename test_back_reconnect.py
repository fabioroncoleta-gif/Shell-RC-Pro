"""BACK/Select: testes unitarios de estado e BLE inteiramente simulado."""
import asyncio
import io
import sys
import time
import types
import unittest
import tempfile
from pathlib import Path
from car_discovery import FleetRegistry
from argparse import Namespace
from contextlib import redirect_stdout
from unittest.mock import patch

from shell_rc_xbox360 import Run, NEUTRAL

CARA=types.SimpleNamespace(name="SL-488", address="02:00:00:00:00:01")
CARB=types.SimpleNamespace(name="SL-F80", address="02:00:00:00:00:02")
CFG={"interval":.008,"input_watchdog_s":20,"back_retry_delay_ms":30,
     "back_disconnect_timeout_ms":250, "back_connect_timeout_ms":600,
     "back_find_timeout_ms":150,"back_neutral_timeout_ms":150,
     "ble_write_timeout_s":.4,"preferred_car_address":CARA.address,
     "back_debounce_ms":350,"default_drive_mode":"direct"}

class TestBackState(unittest.TestCase):
    def test_back_stops_car_and_only_selects_same_device(self):
        s=Run(Namespace(live=True),CFG)
        s.active_car_address=CARA.address
        s.link=True
        s.armed=True
        s.motor.state.direction=1
        s.motor.state.duty=1.
        with redirect_stdout(io.StringIO()):
            self.assertTrue(s.request_fast_reconnect(now=10))
        self.assertFalse(s.armed)
        self.assertFalse(s.link)
        self.assertEqual(s.command_packet(),NEUTRAL)
        self.assertEqual(s.reconnect_target_address,CARA.address)
        self.assertEqual(s.selected_car_address,CARA.address)
        self.assertTrue(s.reconnect_event.is_set())
        self.assertTrue(s.reconnect_pending)

    def test_back_bounce_is_ignored(self):
        s=Run(Namespace(live=True),CFG)
        with redirect_stdout(io.StringIO()):
            self.assertTrue(s.request_fast_reconnect(now=20))
            self.assertFalse(s.request_fast_reconnect(now=20.1))
            self.assertTrue(s.request_fast_reconnect(now=20.4))
        self.assertEqual(s.reconnect_generation,2)

class FakeCharacteristic:
    properties=["write-without-response"]
class FakeServices:
    def get_characteristic(self,uuid):
        return FakeCharacteristic() if uuid.startswith("0000fff1-") else None
class FakeBleClient:
    all=[]
    def __init__(self,target,timeout=10):
        self.target=target
        self.is_connected=False
        self.services=FakeServices()
        self.writes=[]
        self.started=time.monotonic()
        self.disconnected=False
        FakeBleClient.all.append(self)
    async def connect(self):
        await asyncio.sleep(.015)
        self.is_connected=True
    async def disconnect(self):
        await asyncio.sleep(.012)
        self.disconnected=True
        self.is_connected=False
    async def write_gatt_char(self,char,pkt,response=False):
        await asyncio.sleep(.002)
        self.writes.append(bytes(pkt))
class FakeScanner:
    scans=0
    finds=0
    @staticmethod
    async def discover(timeout=4):
        FakeScanner.scans+=1
        await asyncio.sleep(.003)
        return [CARA,CARB]
    @staticmethod
    async def find_device_by_address(address,timeout=2):
        FakeScanner.finds+=1
        return CARA if address==CARA.address else None

async def wait_until(predicate,timeout=.8):
    start=time.monotonic()
    while not predicate():
        if time.monotonic()-start>timeout:
            raise AssertionError("condition not reached")
        await asyncio.sleep(.004)

class TestBackReconnection(unittest.IsolatedAsyncioTestCase):
    async def test_reconnects_same_car_without_scan_and_no_auto_arm(self):
        FakeBleClient.all.clear()
        FakeScanner.scans=0
        FakeScanner.finds=0
        s=Run(Namespace(live=True,address=None),CFG)
        tmp=tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        s.fleet=FleetRegistry(Path(tmp.name)/"fleet.json")  # mock nunca altera o registro real
        s.pad=object()
        fake_module=types.SimpleNamespace(BleakClient=FakeBleClient,BleakScanner=FakeScanner)
        with patch.dict(sys.modules,{"bleak":fake_module}):
            with redirect_stdout(io.StringIO()):
                task=asyncio.create_task(s.ble_loop())
                try:
                    await wait_until(lambda:s.link and len(FakeBleClient.all)==1)
                    first=FakeBleClient.all[0]
                    s.armed=True
                    s.motor.state.direction=1
                    s.motor.state.duty=1
                    s.rt=1
                    await wait_until(lambda:any(p[1]==1 for p in first.writes))
                    self.assertTrue(s.request_fast_reconnect())
                    self.assertFalse(s.armed)
                    await wait_until(lambda:s.link and len(FakeBleClient.all)>=2)
                    second=FakeBleClient.all[1]
                    self.assertEqual(second.target.address,CARA.address)
                    self.assertTrue(first.disconnected)
                    self.assertEqual(FakeScanner.scans,1)
                    self.assertEqual(second.writes[0],NEUTRAL)
                    self.assertFalse(s.armed)
                    await asyncio.sleep(.025)
                    self.assertTrue(all(packet[1:3]==b"\x00\x00" for packet in second.writes))
                    self.assertIsInstance(s.last_reconnect_ms,int)
                    self.assertGreaterEqual(s.last_reconnect_ms,0)
                finally:
                    s.running=False
                    await asyncio.wait_for(task,timeout=1)

    async def test_back_interrupts_stuck_ble_packet_without_waiting_full_timeout(self):
        class SlowBLE:
            pending=0
            async def write_gatt_char(self,*a,**kw):
                self.pending+=1
                try:
                    await asyncio.sleep(2)
                finally:
                    self.pending-=1
        s=Run(Namespace(live=True),CFG)
        s.link=True
        s.active_car_address=CARA.address
        s.armed=True
        client=SlowBLE()
        with redirect_stdout(io.StringIO()):
            job=asyncio.create_task(s.write_drive_or_reset(client,FakeCharacteristic(),b'\x01\x01\x00',False))
            await asyncio.sleep(.025)
            began=time.monotonic()
            s.request_fast_reconnect()
            result=await asyncio.wait_for(job,timeout=.25)
        self.assertFalse(result)
        self.assertLess(time.monotonic()-began,.25)
        self.assertEqual(client.pending,0)
        self.assertFalse(s.armed)

if __name__=="__main__":
    unittest.main()
