"""Regressoes para a placa Shell original, sem enviar Bluetooth."""
import json
import unittest
from argparse import Namespace
from pathlib import Path
from shell_rc_xbox360 import Run,NEUTRAL

CFG=json.loads(Path(__file__).with_name("config.json").read_text())
class TestOriginalBLE(unittest.TestCase):
    def setUp(self):
        self.s=Run(Namespace(live=False,hybrid_live=False),CFG)
        self.s.armed=True
        self.s.drive_mode="direct"
        self.s.rt=1.
        self.s.motor.state.direction=1
        self.s.motor.state.duty=.02
    def test_default_direct(self):
        other=Run(Namespace(live=False),CFG)
        self.assertEqual(other.drive_mode,"direct")
    def test_direct_no_pulses(self):
        p=[self.s.command_packet() for _ in range(100)]
        self.assertEqual({(q[1],q[2]) for q in p},{(1,0)})
        self.assertTrue(all(q[6]==0 for q in p))
    def test_boost_only_after_ramp(self):
        self.s.turbo=True
        for x in (.02,.2,.4,.6,.79):
            self.s.motor.state.duty=x
            self.assertEqual(self.s.command_packet()[6],0)
        self.s.motor.state.duty=.8
        self.assertEqual(self.s.command_packet()[6],1)
        self.assertTrue(self.s.boost_output)
    def test_boost_hysteresis(self):
        self.s.turbo=True
        self.s.motor.state.duty=.82
        self.s.command_packet()
        self.s.motor.state.duty=.6
        self.assertEqual(self.s.command_packet()[6],1)
        self.s.motor.state.duty=.54
        self.assertEqual(self.s.command_packet()[6],0)
        self.s.motor.state.duty=.6
        self.assertEqual(self.s.command_packet()[6],0)
    def test_lb_preserves_direction_disables_tracion(self):
        self.s.turbo=True
        self.s.motor.state.duty=1
        self.s.steer=-.9
        self.s.brake_steering=-.9
        self.s.brake_held=True
        pkt=self.s.command_packet()
        self.assertEqual(pkt[1:7],bytes([0,0,1,0,0,0]))
        self.s.brake_held=False
        self.assertEqual(self.s.command_packet()[1:3],bytes([1,0]))
    def test_neutral_disables_boost(self):
        self.s.turbo=True
        self.s.motor.state.duty=1
        self.s.command_packet()
        self.s.stop("teste")
        self.assertFalse(self.s.boost_output)
        self.assertEqual(self.s.command_packet(),NEUTRAL)
    def test_reverse_limited_even_if_forward_mode_is_direct(self):
        self.s.motor.state.direction=-1
        self.s.motor.state.duty=.5
        self.s.rt=0
        self.s.lt=1
        samples=[self.s.command_packet() for _ in range(80)]
        self.assertEqual(sum(p[2] for p in samples),40)
        self.assertTrue(all(p[1]==0 and p[6]==0 for p in samples))

    def test_reverse_unlocked_restores_direct_mode(self):
        self.s.motor.state.direction=-1
        self.s.rt=0
        self.s.lt=1
        self.s.reverse_logic.unlocked=True
        self.s.motor.reverse_full_enabled=True
        samples=[self.s.command_packet() for _ in range(80)]
        self.assertEqual({(p[1],p[2]) for p in samples},{(0,1)})
    def test_disarmed_halts(self):
        self.s.armed=False
        self.assertEqual(self.s.command_packet(),NEUTRAL)

if __name__=="__main__": unittest.main()
