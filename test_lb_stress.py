"""Testes de estresse do freio LB e das protecoes de comunicacao, sem BLE real."""
import asyncio
import io
import json
import unittest
from argparse import Namespace
from contextlib import redirect_stdout
from pathlib import Path
from shell_rc_xbox360 import Run, NEUTRAL

CFG=json.loads(Path(__file__).with_name('config.json').read_text())
class FakePad:
    def __init__(self):
        self.axes={CFG['steering_axis']:-.8,CFG['lt_axis']:-1.,CFG['rt_axis']:1.}
        self.held=False
    def get_axis(self,i):
        return self.axes[i]
    def get_button(self,i):
        return self.held if i==CFG['lb_brake_button'] else False

class TestLBStress(unittest.TestCase):
    def test_1000_lb_cycles_always_safe_and_recovers(self):
        s=Run(Namespace(live=False),CFG)
        pad=FakePad()
        s.pad=pad
        s.lt0=-1.
        s.rt0=-1.
        s.armed=True
        s.motor.state.direction=1
        s.motor.state.duty=1.
        s.drive_mode='direct'
        buf=io.StringIO()
        with redirect_stdout(buf):
            for i in range(1000):
                pad.held=True
                s.read_pad(.025)
                pkt=s.command_packet()
                self.assertEqual(pkt[1:3],b'\x00\x00',i)
                self.assertEqual(pkt[3:5],b'\x01\x00',i)
                self.assertTrue(s.brake_held)
                pad.held=False
                s.read_pad(.025)
                self.assertFalse(s.brake_held)
                self.assertFalse(s.brake_release_wait_neutral)
                self.assertEqual(s.command_packet()[1],1,i)
                self.assertTrue(s.armed)
        self.assertEqual(s.reverse_logic.count,0)
        self.assertFalse(s.reverse_logic.unlocked)

    def test_polling_same_pressed_state_does_not_toggle(self):
        s=Run(Namespace(live=False),CFG)
        pad=FakePad()
        s.pad=pad;s.lt0=-1;s.rt0=-1;s.armed=True
        s.motor.state.direction=1;s.motor.state.duty=.6
        pad.held=True
        with redirect_stdout(io.StringIO()):
            for _ in range(1000):
                s.read_pad(.025)
                self.assertTrue(s.brake_held)
                self.assertEqual(s.command_packet()[1:3],b'\x00\x00')
        self.assertEqual(s.motor.state.direction,1)

    def test_watchdog_disarms_and_clears_commands(self):
        s=Run(Namespace(live=False),CFG)
        s.armed=True;s.motor.state.direction=1;s.motor.state.duty=1.
        s.last_pad_update=100.
        with redirect_stdout(io.StringIO()):
            self.assertTrue(s.check_input_watchdog(101.))
        self.assertFalse(s.armed)
        self.assertEqual(s.command_packet(),NEUTRAL)
        self.assertFalse(s.check_input_watchdog(102.))

    def test_no_watchdog_when_recent(self):
        s=Run(Namespace(live=False),CFG)
        s.armed=True;s.last_pad_update=100.
        self.assertFalse(s.check_input_watchdog(100.1))
        self.assertTrue(s.armed)

class FakeBLE:
    def __init__(self,delay=0):
        self.delay=delay
        self.writes=[]
        self.pending=0
        self.max_pending=0
    async def write_gatt_char(self,char,payload,response=False):
        self.pending+=1
        self.max_pending=max(self.max_pending,self.pending)
        try:
            await asyncio.sleep(self.delay)
            self.writes.append(bytes(payload))
        finally:
            self.pending-=1

class TestWriteTimeout(unittest.IsolatedAsyncioTestCase):
    async def test_timeout_aborts_and_counts(self):
        cfg=dict(CFG,ble_write_timeout_s=.02)
        s=Run(Namespace(live=False),cfg)
        client=FakeBLE(delay=1.)
        with self.assertRaises(asyncio.TimeoutError):
            await s.write_ble_safe(client,'char',bytes([1,1,0]),response=False)
        self.assertEqual(s.ble_write_failures,1)
        self.assertEqual(client.pending,0)
        self.assertEqual(client.writes,[])

    async def test_bounded_writes_are_sequential(self):
        s=Run(Namespace(live=False),CFG)
        client=FakeBLE(delay=.001)
        for _ in range(100):
            await s.write_ble_safe(client,'char',NEUTRAL,response=False)
        self.assertEqual(len(client.writes),100)
        self.assertEqual(client.max_pending,1)
        self.assertEqual(s.ble_write_failures,0)

if __name__=='__main__':
    unittest.main()
