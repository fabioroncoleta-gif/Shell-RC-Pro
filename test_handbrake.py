import unittest, time, json
from pathlib import Path
from argparse import Namespace
from shell_rc_xbox360 import Run, NEUTRAL
CFG=json.loads(Path(__file__).with_name("config.json").read_text())

class FakePad:
    def __init__(self):
        self.axes={CFG["steering_axis"]:0.,CFG["lt_axis"]:-1.,CFG["rt_axis"]:-1.}
        self.buttons={}
    def get_axis(self,i):return self.axes[i]
    def get_button(self,i):return self.buttons.get(i,False)

class TestLB(unittest.TestCase):
    def ready(self,direction=1,duty=.7,steering=-.8,mode="pulse"):
        s=Run(Namespace(live=False),CFG)
        p=FakePad()
        s.pad=p;s.lt0=-1.;s.rt0=-1.;s.armed=True
        s.drive_mode=mode;s.steer=steering
        s.motor.state.direction=direction;s.motor.state.duty=duty
        p.axes[CFG["rt_axis"]]=.4 if direction==1 else -1.
        p.axes[CFG["lt_axis"]]=.4 if direction==-1 else -1.
        p.axes[CFG["steering_axis"]]=steering
        return s,p

    def test_lb_cuts_motor_but_holds_steering_and_lights(self):
        for mode in ("pulse","direct"):
            for direction in (1,-1):
                s,p=self.ready(direction=direction,mode=mode)
                s.turbo=True
                s.lights_ctrl.select("steady",time.monotonic())
                p.buttons[CFG["lb_brake_button"]]=True
                s.read_pad(.025)
                self.assertTrue(s.brake_held)
                self.assertEqual(s.brake_steering,-.8)
                self.assertEqual(s.motor.state.direction,direction)
                self.assertEqual(s.motor.state.duty,.7)
                self.assertTrue(s.turbo) # O modo fica memorizado; bit 6 nao sai durante LB.
                for _ in range(20):
                    # Mesmo se mexer no stick, direcao enviada fica congelada.
                    p.axes[CFG["steering_axis"]]=.8
                    s.read_pad(.025)
                    self.assertEqual(s.command_packet(),bytes([1,0,0,1,0,1,0,0]))

    def test_release_same_trigger_resumes_without_neutral(self):
        for mode in ("pulse","direct"):
            s,p=self.ready(duty=1.,mode=mode)
            p.axes[CFG["rt_axis"]]=1.
            p.buttons[CFG["lb_brake_button"]]=True
            s.read_pad(.025)
            self.assertEqual(s.command_packet(),bytes([1,0,0,1,0,0,0,0]))
            p.buttons[CFG["lb_brake_button"]]=False
            s.read_pad(.025)
            self.assertFalse(s.brake_release_wait_neutral)
            self.assertFalse(s.brake_held)
            # Direto = comando sustentado imediatamente; pulsos mantem duty salvo.
            if mode=="direct":
                self.assertEqual([s.command_packet()[1] for _ in range(10)],[1]*10)
            else:
                self.assertEqual(s.motor.state.direction,1)
                self.assertGreater(s.motor.state.duty,0)
                self.assertGreater(sum(s.command_packet()[1] for _ in range(20)),0)

    def test_release_reverse_resumes_reverse_no_flip(self):
        s,p=self.ready(direction=-1,duty=1.,mode="direct")
        p.axes[CFG["lt_axis"]]=1.
        p.buttons[CFG["lb_brake_button"]]=True;s.read_pad(.025)
        self.assertEqual(s.command_packet()[:3],bytes([1,0,0]))
        p.buttons[CFG["lb_brake_button"]]=False;s.read_pad(.025)
        self.assertEqual(s.motor.state.direction,-1)
        self.assertFalse(s.brake_held)
        samples=[s.command_packet() for _ in range(100)]
        self.assertTrue(all(packet[1]==0 for packet in samples))
        self.assertGreater(sum(packet[2] for packet in samples),0)

    def test_opposite_trigger_requires_neutral(self):
        s,p=self.ready(direction=1,duty=.9)
        p.buttons[CFG["lb_brake_button"]]=True;s.read_pad(.025)
        p.axes[CFG["rt_axis"]]=-1.
        p.axes[CFG["lt_axis"]]=1.
        p.buttons[CFG["lb_brake_button"]]=False;s.read_pad(.025)
        self.assertTrue(s.brake_release_wait_neutral)
        self.assertEqual(s.command_packet()[:3],bytes([1,0,0]))
        for _ in range(10):
            s.read_pad(.025)
            self.assertEqual(s.command_packet()[:3],bytes([1,0,0]))
        p.axes[CFG["lt_axis"]]=-1.
        s.read_pad(.025)
        self.assertFalse(s.brake_release_wait_neutral)

    def test_direct_mode_launch_at_small_trigger_without_off_pulses(self):
        s,p=self.ready(direction=0,duty=0,steering=0,mode="direct")
        p.axes[CFG["rt_axis"]]=-0.72 # normalizado a 0.14 (acima de 0.13)
        s.read_pad(.025)
        self.assertEqual(s.motor.state.direction,1)
        self.assertLess(s.motor.state.duty,.45)
        self.assertEqual([s.command_packet()[1] for _ in range(30)],[1]*30)
        p.axes[CFG["rt_axis"]]=-1.
        s.read_pad(.025)
        self.assertEqual(s.command_packet()[1],0)

    def test_turbo_suppressed_during_lb_and_restored_after(self):
        s,p=self.ready(direction=1,duty=1.,mode="direct",steering=0.)
        p.axes[CFG["rt_axis"]]=1.
        s.turbo=True
        p.buttons[CFG["lb_brake_button"]]=True;s.read_pad(.025)
        self.assertEqual(s.command_packet()[6],0)
        p.buttons[CFG["lb_brake_button"]]=False;s.read_pad(.025)
        self.assertEqual(s.command_packet()[6],1)

    def test_LB_disarms_pending_arming(self):
        s,p=self.ready();s.armed=False;s.arm_asked=True
        s.apply_handbrake(True)
        self.assertFalse(s.arm_asked)
        self.assertTrue(s.brake_held)

if __name__=="__main__":unittest.main()
