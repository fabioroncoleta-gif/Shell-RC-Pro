import json,unittest
from argparse import Namespace
from pathlib import Path
from shell_rc_xbox360 import Run,NEUTRAL,trig,CONFIG_DEFAULT
from motor_profile import MotorController,curve

CFG=dict(CONFIG_DEFAULT)
CONFIG_PATH=Path(__file__).with_name("config.json")
if CONFIG_PATH.exists():
    CFG.update(json.loads(CONFIG_PATH.read_text()))

class TestMotorDrive(unittest.TestCase):
    def test_joystick_trigger(self):
        self.assertEqual(trig(-1.,-1.),0)
        self.assertAlmostEqual(trig(0.,-1.),.5)
        self.assertEqual(trig(1.,-1.),1)

    def test_monotonic_forward_and_reverse_curves(self):
        for reverse in (False,True):
            values=[curve(x/1000,CFG,reverse=reverse) for x in range(1001)]
            self.assertTrue(all(b>=a for a,b in zip(values,values[1:])))
            self.assertEqual(values[0],0)
            self.assertEqual(values[-1],1)

    def test_no_drive_when_not_armed(self):
        m=MotorController(CFG)
        self.assertEqual(m.step(1,0,.025,1,armed=False),(0.,0))
        self.assertEqual(m.packet(0,False,armed=False),NEUTRAL)

    def test_max_forward_is_continuous_after_ramp(self):
        m=MotorController(CFG)
        for i in range(200):
            m.step(1,0,.025,1+i*.025,armed=True)
        self.assertEqual(m.state.duty,1.)
        self.assertEqual(m.state.direction,1)
        for _ in range(50):
            self.assertEqual(m.packet(0),bytes([1,1,0,0,0,0,0,0]))

    def test_reverse_limited_half_duty_by_default(self):
        m=MotorController(CFG)
        for i in range(250):
            m.step(0,1,.025,1+i*.025,armed=True)
        self.assertAlmostEqual(m.state.duty,.5,places=3)
        self.assertEqual(m.state.direction,-1)
        self.assertLessEqual(abs(sum(m.packet(0)[2] for _ in range(100))-50),1)

    def test_full_reverse_after_explicit_unlock(self):
        m=MotorController(CFG)
        m.reverse_full_enabled=True
        for i in range(250):
            m.step(0,1,.025,1+i*.025,armed=True)
        self.assertEqual(m.state.duty,1.)
        self.assertEqual(m.state.direction,-1)
        for _ in range(50):
            self.assertEqual(m.packet(0),bytes([1,0,1,0,0,0,0,0]))

    def test_curve_jerk_limited(self):
        m=MotorController(CFG)
        before=0.
        for i in range(180):
            duty,_=m.step(1,0,.025,1+i*.025,armed=True)
            self.assertGreaterEqual(duty,before-1e-8)
            self.assertLess(duty-before,.035)
            before=duty

    def test_release_issues_neutral_immediately(self):
        m=MotorController(CFG)
        for i in range(70):
            m.step(1,0,.025,1+i*.025,armed=True)
        m.step(0,0,.025,3,armed=True)
        self.assertEqual(m.packet(0),NEUTRAL)

    def test_reverse_interlock(self):
        m=MotorController(CFG)
        for i in range(40):m.step(1,0,.025,1+i*.025)
        duty,direction=m.step(0,1,.025,2.01)
        self.assertEqual((duty,direction),(0,0))
        self.assertEqual(m.packet(0),NEUTRAL)
        for i in range(10):
            duty,direction=m.step(0,1,.025,2.02+i*.025)
            self.assertEqual((duty,direction),(0,0))
        duty,direction=m.step(0,1,.025,2.52)
        self.assertEqual(direction,-1)

    def test_lights_independent_of_motor(self):
        m=MotorController(CFG)
        self.assertEqual(m.packet(0,lights=True),bytes([1,0,0,0,0,1,0,0]))
        m.step(1,0,.025,1)
        m.state.duty=1.
        self.assertEqual(m.packet(0,lights=True),bytes([1,1,0,0,0,1,0,0]))

    def test_duty_distribution(self):
        m=MotorController(CFG)
        m.state.duty=.35;m.state.direction=-1
        n=sum(m.packet(0)[2] for _ in range(200))
        self.assertLessEqual(abs(n-70),1)

    def test_no_spontaneous_restart(self):
        m=MotorController(CFG)
        m.step(1,0,.025,1)
        m.reset(2)
        for i in range(50):
            m.step(0,0,.025,2+i*.025)
            self.assertEqual(m.packet(0),NEUTRAL)

    def test_stop_clears_state(self):
        state=Run(Namespace(live=False),CFG)
        state.armed=True
        state.motor.step(1,0,.025,1)
        state.stop("teste")
        self.assertFalse(state.armed)
        self.assertEqual(state.motor.state.duty,0.)

    def test_turbo_b_bit(self):
        m=MotorController(CFG)
        m.state.duty=1.;m.state.direction=1
        self.assertEqual(m.packet(0,False,True,"pulse",True),bytes([1,1,0,0,0,0,1,0]))
        self.assertEqual(m.packet(0,True,True,"pulse",True),bytes([1,1,0,0,0,1,1,0]))
        self.assertEqual(m.packet(0,False,True,"pulse",False),bytes([1,1,0,0,0,0,0,0]))
        self.assertEqual(m.packet(0,False,False,"pulse",True),NEUTRAL)
    def test_b_toggles_turbo_only(self):
        s=Run(Namespace(live=False),CFG)
        self.assertEqual(s.drive_mode,CFG.get("default_drive_mode","pulse"))
        s.toggle_turbo()
        self.assertTrue(s.turbo)
        self.assertEqual(s.drive_mode,CFG.get("default_drive_mode","pulse"))
        s.toggle_turbo()  # debounce
        self.assertTrue(s.turbo)
        s.last_button_time.clear()
        s.toggle_turbo()
        self.assertFalse(s.turbo)
        s.toggle_drive_mode()  # only keyboard D
        self.assertEqual(s.drive_mode,"pulse" if CFG.get("default_drive_mode","pulse")=="direct" else "direct")
        s.toggle_turbo()
        self.assertFalse(s.turbo)
        s.stop("teste")
        self.assertFalse(s.turbo)
if __name__=="__main__":unittest.main()
