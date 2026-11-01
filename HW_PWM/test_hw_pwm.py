import unittest
from pwm_motor_core import PwmMotorController,COAST,BRAKE

class TestBenchController(unittest.TestCase):
    def test_off_disarmed(self):
        m=PwmMotorController()
        self.assertEqual(m.tick(1.,False,False,.01,0),COAST)

    def test_low_and_high_throttle_monotonic(self):
        for throttle in (.20,.45,1.):
            m=PwmMotorController()
            a=[m.tick(throttle,False,True,.01,i*.01).duty for i in range(120)]
            self.assertGreater(a[-1],0)
            self.assertTrue(all(b+1.e-6>=a for a,b in zip(a,a[1:])))
        self.assertGreater(PwmMotorController()._target(1.0),PwmMotorController()._target(.25))

    def test_forward_and_reverse_outputs(self):
        for throttle,expected in ((.8,("PWM","0")),(-.8,("0","PWM"))):
            m=PwmMotorController()
            out=m.tick(throttle,False,True,.01,0)
            self.assertEqual((out.in1,out.in2),expected)

    def test_electrical_brake_is_not_reverse(self):
        m=PwmMotorController()
        for i in range(40):m.tick(1.,False,True,.01,i*.01)
        out=m.tick(1.,True,True,.01,.4)
        self.assertEqual(out,BRAKE)
        self.assertEqual((out.in1,out.in2),("1","1"))
        for i in range(10):
            self.assertEqual(m.tick(1.,True,True,.01,.41+.01*i),BRAKE)

    def test_resume_after_brake_ramps_pwm(self):
        m=PwmMotorController()
        for i in range(100):m.tick(.9,False,True,.01,i*.01)
        self.assertEqual(m.tick(.9,True,True,.01,1.0),BRAKE)
        out=m.tick(.9,False,True,.01,1.01)
        self.assertEqual(out,COAST) # tempo minimo de recuperacao
        trace=[m.tick(.9,False,True,.01,1.02+i*.01) for i in range(80)]
        first=next(x for x in trace if x.mode=="FORWARD_PWM")
        self.assertLess(first.duty,.2) # sem retomada em 100% abrupta
        self.assertGreater(trace[-1].duty,.3)

    def test_inversion_coasts_before_reversing(self):
        m=PwmMotorController()
        for i in range(80):m.tick(1.,False,True,.01,i*.01)
        out=m.tick(-1.,False,True,.01,.81)
        self.assertEqual(out,COAST)
        for i in range(28):
            self.assertEqual(m.tick(-1.,False,True,.01,.82+i*.01),COAST)
        out=m.tick(-1.,False,True,.01,1.20)
        self.assertEqual(out.mode,"REVERSE_PWM")

    def test_zero_never_actuates(self):
        m=PwmMotorController()
        for i in range(100):m.tick(.7,False,True,.01,i*.01)
        self.assertEqual(m.tick(0.,False,True,.01,1.),COAST)

    def test_deadzone_and_clamp(self):
        m=PwmMotorController()
        self.assertEqual(m.tick(.03,False,True,.01,0),COAST)
        self.assertLessEqual(m._target(99),1.)

    def test_brake_does_not_drive_when_disarmed(self):
        m=PwmMotorController()
        self.assertEqual(m.tick(.9,True,False,.01,0),COAST)

if __name__=="__main__":unittest.main()
