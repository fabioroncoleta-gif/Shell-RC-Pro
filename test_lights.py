import json, unittest, math
from pathlib import Path
from argparse import Namespace
from light_effects import LightEffects
from motor_profile import MotorController
from shell_rc_xbox360 import Run
C=json.loads(Path(__file__).with_name("config.json").read_text())

class LightTest(unittest.TestCase):
    def test_modes_exclusive_and_toggle_off(self):
        light=LightEffects(C)
        self.assertEqual(light.mode,"off")
        self.assertEqual(light.select("steady",1),"steady")
        self.assertEqual(light.select("strobe",2),"strobe")
        self.assertEqual(light.select("fade",3),"fade")
        self.assertEqual(light.select("fade",4),"off")
        self.assertEqual(light.select("steady",5),"steady")
        self.assertEqual(light.select("steady",6),"off")

    def test_strobe_frequency(self):
        light=LightEffects(C); light.select("strobe",100)
        # 3 Hz, 'on' no primeiro 40% de cada ciclo.
        self.assertTrue(light.output(100.0))
        self.assertFalse(light.output(100.20))
        self.assertTrue(light.output(100+1/3))
        self.assertFalse(light.output(100+1/3+0.18))

    def test_fade_breathing_curve(self):
        light=LightEffects(C); light.select("fade",100)
        p=C["fade_period_s"]
        self.assertAlmostEqual(light.brightness(100),0,places=5)
        self.assertAlmostEqual(light.brightness(100+p/4),.5,places=5)
        self.assertAlmostEqual(light.brightness(100+p/2),1,places=5)
        self.assertAlmostEqual(light.brightness(100+p),0,places=5)
        samples=[light.output(100+i*.05) for i in range(64)]
        self.assertTrue(any(samples))
        self.assertTrue(any(not s for s in samples))

    def test_lights_motor_bits_independent(self):
        m=MotorController(C)
        m.state.duty=1;m.state.direction=1
        on=m.packet(0,True);off=m.packet(0,False)
        self.assertEqual(on[:5],off[:5])
        self.assertEqual(on[5],1)
        self.assertEqual(off[5],0)

    def test_buttons_on_run(self):
        s=Run(Namespace(live=False),C)
        s.set_light_mode("steady")
        self.assertEqual(s.lights_ctrl.mode,"steady")
        # Botoes diferentes devem alternar sem atraso.
        s.set_light_mode("fade")
        self.assertEqual(s.lights_ctrl.mode,"fade")
        # Mesmo botao repetido em menos de 300 ms e ignorado.
        s.set_light_mode("fade")
        self.assertEqual(s.lights_ctrl.mode,"fade")
        s.last_button_time.clear()
        s.set_light_mode("strobe")
        self.assertEqual(s.lights_ctrl.mode,"strobe")

if __name__=="__main__":
    unittest.main()
