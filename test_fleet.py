"""Regressoes do seletor de miniaturas Shell, sem hardware BLE."""
import tempfile
import unittest
from argparse import Namespace
from pathlib import Path
from types import SimpleNamespace
from car_discovery import FleetRegistry, filter_shell_devices, choose_device
from shell_rc_xbox360 import Run,NEUTRAL

def fake(name, mac):
    return SimpleNamespace(name=name,address=mac)

class TestFleet(unittest.TestCase):
    def setUp(self):
        self.a=fake("SL-488 Challenge","AA:BB:01")
        self.b=fake("SL-SF90","AA:BB:02")
        self.x=fake("Headset","CC:DD:03")

    def test_multi_car_filters_non_shell(self):
        found=filter_shell_devices([self.b,self.x,self.a,self.a])
        self.assertEqual(len(found),2)
        self.assertEqual([x.address for x in found],["AA:BB:01","AA:BB:02"])

    def test_known_non_shell_name_is_allowed(self):
        found=filter_shell_devices([self.x],["CC:DD:03"])
        self.assertEqual(found,[self.x])

    def test_last_car_preferred(self):
        self.assertIs(choose_device([self.a,self.b],last_address=self.b.address),self.b)

    def test_explicit_selection_has_priority(self):
        self.assertIs(choose_device([self.a,self.b],manual_address=self.a.address,
                       last_address=self.b.address),self.a)

    def test_ambiguous_must_choose(self):
        self.assertIsNone(choose_device([self.a,self.b]))

    def test_save_last_selected(self):
        with tempfile.TemporaryDirectory() as tmp:
            f=Path(tmp)/"fleet.json"
            r=FleetRegistry(f)
            r.remember_connected(self.a)
            r.remember_connected(self.b)
            v=FleetRegistry(f)
            self.assertEqual(v.last_address,self.b.address)
            self.assertEqual(len(v.cars),2)

    def test_switch_disarms_and_resets_reverse(self):
        r=Run(Namespace(live=False),{})
        r.cars=[self.a,self.b]
        r.armed=True
        r.reverse_logic.unlocked=True
        r.motor.reverse_full_enabled=True
        r.select_car(1)
        self.assertFalse(r.armed)
        self.assertFalse(r.reverse_logic.unlocked)
        self.assertFalse(r.motor.reverse_full_enabled)
        self.assertTrue(r.switch_requested)
        self.assertEqual(r.selected_car_address,self.b.address)
        self.assertEqual(r.command_packet(),NEUTRAL)

    def test_no_selection_beyond_list(self):
        r=Run(Namespace(live=False),{})
        r.cars=[self.a]
        r.select_car(5)
        self.assertIsNone(r.selected_car_address)

if __name__=="__main__":
    unittest.main()
