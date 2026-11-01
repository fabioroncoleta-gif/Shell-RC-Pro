import json
import unittest
from argparse import Namespace
from pathlib import Path
from hybrid_protocol import encode, decode, compose_existing_and_pwm
from hybrid_ble import validate_approval
from shell_rc_xbox360 import Run

CFG=json.loads((Path(__file__).parent.parent/'config.json').read_text())

class Pad:
    def get_name(self): return 'Fake Xbox'
class TestHybridProtocol(unittest.TestCase):
    def test_roundtrip(self):
        for direction in (-1,0,1):
            for duty in (0,.05,.33,1.):
                raw=encode(10,direction,duty,True,True,True)
                v=decode(raw)
                self.assertEqual(len(raw),8)
                self.assertEqual(v.direction, direction if duty else 0)
                self.assertEqual(v.armed, bool(direction and duty))
                self.assertEqual(v.duty_permille, round(duty*1000) if direction else 0)
    def test_crc_rejects_bad_packets(self):
        good=bytearray(encode(2,1,.5,True))
        for i in range(8):
            bad=bytearray(good);bad[i]^=1
            with self.assertRaises(ValueError):decode(bytes(bad))
    def test_direction_validation(self):
        with self.assertRaises(ValueError):encode(0,3,.5,True)
        with self.assertRaises(ValueError):encode(0,1,1.2,True)
        with self.assertRaises(ValueError):encode(-1,1,.5,True)
    def test_failsafe_when_disarmed(self):
        raw=decode(encode(2,-1,.6,False,True))
        self.assertEqual((raw.direction,raw.duty_permille,raw.armed),(0,0,False))
    def mk(self):
        s=Run(Namespace(live=False,hybrid_live=True),CFG)
        s.pad=Pad()
        s.armed=True
        s.motor.state.direction=1
        s.motor.state.duty=.62
        s.steer=-.7
        return s
    def test_original_never_drives_rear_motor(self):
        s=self.mk();s.turbo=True
        for mode in ('pulse','direct'):
            s.drive_mode=mode
            car,pwm=compose_existing_and_pwm(s,15)
            self.assertEqual((car[1],car[2],car[6]),(0,0,0))
            self.assertEqual(car[3:5],bytes([1,0]))
            self.assertEqual(decode(pwm).duty_permille,620)
            self.assertEqual(decode(pwm).sport,mode=='direct')
    def test_lb_coasts_and_keeps_steering(self):
        s=self.mk();s.brake_held=True;s.brake_steering=-.7
        car,pwm=compose_existing_and_pwm(s,3)
        self.assertEqual(car[3:5],bytes([1,0]))
        self.assertEqual((decode(pwm).direction,decode(pwm).duty_permille),(0,0))
    def test_loss_of_pad_zeroes_command(self):
        s=self.mk();s.pad=None
        _,pwm=compose_existing_and_pwm(s,5)
        self.assertEqual(decode(pwm).duty_permille,0)
    def test_opposite_reverse_while_blocked(self):
        s=self.mk();s.brake_release_wait_neutral=True;s.motor.state.direction=-1
        _,pwm=compose_existing_and_pwm(s,5)
        self.assertEqual(decode(pwm).duty_permille,0)
    def test_missing_approval_disables_real_hardware(self):
        with self.assertRaises(RuntimeError):
            validate_approval(Path('/some/missing/approval.json'))
    def test_sample_approval_not_accepted(self):
        path=Path(__file__).with_name('INSTALLATION_APPROVAL.example.json')
        with self.assertRaises(RuntimeError):validate_approval(path)

if __name__=='__main__':unittest.main()
