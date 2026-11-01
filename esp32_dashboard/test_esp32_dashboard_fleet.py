import unittest
import time
from tempfile import TemporaryDirectory
from pathlib import Path
import esp32_dashboard as dash
class FleetPanelTests(unittest.TestCase):
 def setUp(self):
  self.tmp=TemporaryDirectory();self.old=dash.DASHLOG;dash.DASHLOG=Path(self.tmp.name)/'log';self.m=dash.Monitor();self.m.set_transport('SERIAL_USB')
 def tearDown(self):dash.DASHLOG=self.old;self.tmp.cleanup()
 def test_discovery_selection_and_switch(self):
  self.m.consume('FLEET_BEGIN,COUNT=2')
  self.m.consume('FLEET,INDEX=1,NAME=SL-488,ADDR=AA:BB:CC:DD:EE:FF,RSSI=-50,SEEN=1,REJECTED=0')
  self.m.consume('FLEET,INDEX=2,NAME=SL-SF90,ADDR=11:22:33:44:55:66,RSSI=-60,SEEN=1,REJECTED=1')
  self.m.consume('FLEET_SELECTED,INDEX=1,NAME=SL-488,ADDR=AA:BB:CC:DD:EE:FF')
  s=self.m.snapshot(0)['state'];self.assertEqual(len(s['fleet']),2);self.assertEqual(s['selected_car'],1);self.assertTrue(s['fleet'][1]['rejected'])
  self.m.state.update(car_armed=True,xbox_armed=True)
  self.m.consume('FLEET_SWITCH,DISARMED,START_REQUIRED')
  self.assertFalse(self.m.state['car_armed']);self.assertFalse(self.m.state['xbox_armed'])
 def test_only_fleet_commands(self):
  for cmd in ['CAR 1','CAR 8','SCAN','CARS','NEXT','PREV']:self.assertTrue(self.m.queue_command(cmd)[0])
  for cmd in ['ARM','CAR 9','CAR 1\nMODE USB','MODE USB','RT 1000','CAR 0']:self.assertFalse(self.m.queue_command(cmd)[0])
 def test_shared_or_offline_usb_cannot_write(self):
  for transport in ['LOG_COMPARTILHADO','RECONECTANDO','SEM_PERMISSAO']:
   self.m.set_transport(transport);self.assertFalse(self.m.queue_command('CAR 1')[0])
 def test_dpad_bits_preserved(self):
  self.m.consume('INPUT2,XBOX_BT,0,0,0,512');self.assertEqual(self.m.state['buttons'],512)
 def test_queue_bounded(self):
  for _ in range(16):self.assertTrue(self.m.queue_command('CARS')[0])
  self.assertFalse(self.m.queue_command('CARS')[0])
if __name__=='__main__':unittest.main()
