import json,unittest
from pathlib import Path

class TestRealBoardGate(unittest.TestCase):
    def test_confirmed_rear_motor_wires(self):
        p=json.loads(Path(__file__).with_name('hardware_profile_36732_BT.json').read_text())
        self.assertEqual(set(p['confirmed_traction_motor']['wires']),{'yellow','red'})
        self.assertEqual(p['confirmed_traction_motor']['status'],'CONFIRMED_PHYSICAL_WIRE_PAIR')
    def test_m2_motor_pad_labels_from_photo(self):
        p=json.loads(Path(__file__).with_name('hardware_profile_36732_BT.json').read_text())
        motor=p['confirmed_traction_motor']
        self.assertEqual(motor['pcb_connector_label'],'M2')
        self.assertEqual({x['pcb_silkscreen']:x['wire'] for x in motor['pcb_pad_wire_mapping']},
                         {'M2-':'yellow','M2+':'red'})
        self.assertFalse(p['hardware_pwm_enabled'])
    def test_owner_confirmed_traction_driver(self):
        p=json.loads(Path(__file__).with_name('hardware_profile_36732_BT.json').read_text())
        m=p['confirmed_traction_motor']
        self.assertEqual(m['pcb_driver_component'],'U6')
        self.assertEqual(m['driver_association_status'],'CONFIRMED_BY_OWNER_NO_PIN_MAPPING')
        self.assertFalse(p['motor_driver_pinout_verified'])
    def test_hardware_not_enabled_without_driver_identification(self):
        p=json.loads(Path(__file__).with_name('hardware_profile_36732_BT.json').read_text())
        self.assertFalse(p['motor_driver_pinout_verified'])
        self.assertFalse(p['hardware_pwm_enabled'])
        self.assertFalse(p['dynamic_electrical_brake_enabled'])
        self.assertFalse(p['firmware_enabled'])

if __name__=='__main__':unittest.main()
