/*
 * Shell RC Pro - ESP32 Dual Xbox GamepadSample Gateway (v0.1)
 * MCU: ESP32-D0WD-V3, 4 MB flash, CP2102 UART.
 *
 * INPUT 1: Xbox Wireless Bluetooth (Bluepad32, BT Classic / BLE).
 * INPUT 2: Xbox 360 wireless receiver USB, via EXTERNAL MAX3421E host.
 *
 * USB HOST SHIELD (NOT currently attached):
 * ESP32 GPIO18 SCK, GPIO19 MISO, GPIO23 MOSI, GPIO5 CS, GPIO17 INT,
 * GND common; USB Host shield 5V/VBUS from correct external supply.
 *
 * HARD SAFETY: NO motor GPIO, NO BLE RC-car commands. Serial telemetry only.
 * Pair BT gamepad with Xbox sync button. For Xbox360 add USB host hardware.
 */
#include <Arduino.h>
#include <SPI.h>
#include <Bluepad32.h>
#include <Usb.h>
#include <usbhub.h>
#include <XBOXRECV.h>
#include "src/input_core.h"
#include "src/ferrari_ble_link.h"
#include "src/drive_logic.h"

using shellrc::GamepadSample;
using shellrc::RoutedState;
using shellrc::Router;
using shellrc::Source;
using shellrc::Mode;

static Router router;
static ControllerPtr gamepads[BP32_MAX_GAMEPADS] = {};
static USB usb;
static XBOXRECV xbox360(&usb);
static bool usbHostReady = false;
static uint32_t lastUsbPoll = 0;
static uint32_t lastPrint = 0;
static uint32_t lastHealth = 0;
static uint32_t lastBtData = 0;
static bool lastBtConnected = false;
static GamepadSample lastBtSample = {};
static uint32_t lastBtKeepAlive = 0;
static char serialCommand[40];
static size_t serialLen = 0;
static Source previousSource = Source::NONE;
static bool previousArmed = false;
static const uint32_t kOutputEveryMs = 50;
static uint32_t lastCarActionMs=0;
static uint32_t lastCarStatusMs=0;
static ferrarible::State lastCarState=ferrarible::State::IDLE;
static bool lastCarLink=false;
static uint32_t lastGamepadRecoveryMs=0;
static uint32_t lastGamepadLinkMs=0;
static uint32_t lastDiagMs=0;
static shellrc::DriveModel drive;
static uint8_t lastButtonsBridge=0;
static bool lastReportedArm=false;

static const char* sourceName(Source s) {
  if (s == Source::BLUETOOTH) return "XBOX_BT";
  if (s == Source::XBOX360_USB) return "XBOX360_USB";
  return "NONE";
}

static const char* modeName(Mode m) {
  if (m == Mode::BLUETOOTH_ONLY) return "BT";
  if (m == Mode::USB_ONLY) return "USB";
  return "AUTO";
}

static void onConnectedController(ControllerPtr ctl) {
  // Class can still be NONE at connection time; wait for a typed HID report.
  ControllerProperties prop = ctl->getProperties();
  bool pendingXbox = int(ctl->getClass()) == 0 && ctl->getModel() == 32 &&
      prop.vendor_id == 0x045e && prop.product_id == 0x0b13;
  if (!ctl->isGamepad() && !pendingXbox) {
    Serial.printf("IGNORED,NON_GAMEPAD_BT,MODEL=%s,CLASS=%d\n",
        ctl->getModelName().c_str(),int(ctl->getClass()));
    ctl->disconnect();
    return;
  }
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (gamepads[i] == nullptr) {
      gamepads[i] = ctl;
      ferrarible::xboxAwaitingReport.store(true);
      // No synthetic sample, READY or motor arming before valid gamepad data.
      Serial.printf("LINKED,XBOX_BT,%s,slot=%d,CLASS=%d,WAITING_VALID_REPORT\n",
          ctl->getModelName().c_str(),i,int(ctl->getClass()));
      ctl->setPlayerLEDs(1 << i);
      return;
    }
  }
  Serial.println("IGNORED,NO_GAMEPAD_SLOT");
}

static void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (gamepads[i] == ctl) gamepads[i] = nullptr;
  }
  router.disconnected(Source::BLUETOOTH);
  lastBtConnected = false;
  ferrarible::xboxAwaitingReport.store(false);
  ferrarible::xboxGamepadReady.store(false);
  lastBtSample = {};
  lastBtKeepAlive = 0;
  Serial.println("DISCONNECTED,XBOX_BT,DISARMED");
}

static int16_t clipSteer(int32_t x) {
  if (x < -512) return -512;
  if (x > 512) return 512;
  return static_cast<int16_t>(x);
}
static uint16_t clipTrigger(int32_t x) {
  if (x < 0) return 0;
  if (x > 1023) return 1023;
  return static_cast<uint16_t>(x);
}

static void handleBluetooth(uint32_t now) {
  bool updated = BP32.update();
  bool seenController = false;
  if (updated) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
      ControllerPtr ctl = gamepads[i];
      if (!ctl || !ctl->isConnected() || !ctl->isGamepad() || !ctl->hasData())
        continue;
      if (!lastBtConnected) {
        ferrarible::xboxGamepadReady.store(true);
        ferrarible::xboxAwaitingReport.store(false);
        Serial.printf("CONNECTED,XBOX_BT,%s,slot=%d,VALID_REPORT=1\n",
            ctl->getModelName().c_str(),i);
      }
      // Exactly one Bluetooth controller is selected, to prevent two people
      // from driving the same robot. Slot 0 has first-connection priority.
      GamepadSample in;
      in.connected = true;
      in.steer = clipSteer(ctl->axisX());
      in.rt = clipTrigger(ctl->throttle());
      in.lt = clipTrigger(ctl->brake());
      in.lb = ctl->l1();
      in.b = ctl->b();
      in.start = ctl->miscStart();
      in.back = ctl->miscSelect();
      in.buttons =
          (ctl->a() ? 1u : 0u) |
          (ctl->b() ? 2u : 0u) |
          (ctl->x() ? 4u : 0u) |
          (ctl->y() ? 8u : 0u) |
          (ctl->l1() ? 16u : 0u) |
          (ctl->r1() ? 32u : 0u) |
          (in.back ? 64u : 0u) |
          (in.start ? 128u : 0u);
      router.record(Source::BLUETOOTH,in,now);
      lastBtSample = in;
      lastBtData = now;
      seenController = true;
      break;
    }
  }
  if (seenController) lastBtConnected=true;
  // Xbox sends changes, not a fixed stream. Preserve held controls only
  // while BLE's confirmed 400ms supervision and BTstack heartbeat are alive.
  if (lastBtConnected && !seenController &&
      uint32_t(now-lastBtKeepAlive)>=50u &&
      ferrarible::supervisedXboxLive(now)) {
    bool linked=false;
    for(int i=0;i<BP32_MAX_GAMEPADS;i++)
      if(gamepads[i] && gamepads[i]->isConnected() && gamepads[i]->isGamepad()) linked=true;
    if(linked && lastBtSample.connected) {
      router.record(Source::BLUETOOTH,lastBtSample,now);
      lastBtKeepAlive=now;
      lastBtData=now;
    }
  }
  if (lastBtConnected && uint32_t(now-lastBtData)>Router::kWatchdogMs) {
    router.disconnected(Source::BLUETOOTH);
    lastBtConnected = false;
    lastBtSample = {};
    Serial.println("WATCHDOG,XBOX_BT,DISARMED");
  }
}

static void handleXbox360(uint32_t now) {
  if (!usbHostReady) return;  // still scan for Bluetooth when shield absent
  usb.Task();
  if (uint32_t(now-lastUsbPoll)<20) return;
  lastUsbPoll=now;
  if (!xbox360.XboxReceiverConnected) {
    router.disconnected(Source::XBOX360_USB);
    return;
  }
  int first = -1;
  for (int i=0;i<4;i++) {
    if (xbox360.Xbox360Connected[i]) {first=i;break;}
  }
  if (first < 0) {
    router.disconnected(Source::XBOX360_USB);
    return;
  }
  GamepadSample in;
  in.connected=true;
  // USB Host Shield library: analog hat in signed [-32768,32767].
  in.steer=clipSteer(xbox360.getAnalogHat(LeftHatX,first)/64);
  // Triggers are 8-bit [0,255]; convert to range [0,1023].
  in.lt=clipTrigger(int32_t(xbox360.getButtonPress(L2,first))*4);
  in.rt=clipTrigger(int32_t(xbox360.getButtonPress(R2,first))*4);
  in.lb=xbox360.getButtonPress(L1,first)>0;
  in.b=xbox360.getButtonPress(B,first)>0; // inspect library button aliases
  in.start=xbox360.getButtonPress(START,first)>0;
  in.back=xbox360.getButtonPress(BACK,first)>0;
  in.buttons =
      (xbox360.getButtonPress(A,first) ? 1u : 0u) |
      (in.b ? 2u : 0u) |
      (xbox360.getButtonPress(X,first) ? 4u : 0u) |
      (xbox360.getButtonPress(Y,first) ? 8u : 0u) |
      (in.lb ? 16u : 0u) |
      (xbox360.getButtonPress(R1,first) ? 32u : 0u) |
      (in.back ? 64u : 0u) |
      (in.start ? 128u : 0u);
  router.record(Source::XBOX360_USB,in,now);
}

static void printHelp() {
  Serial.println("HELP,Shell-RC-Pro-ESP32-DUAL-XBOX");
  Serial.println("COMMANDS: STATUS | MODE AUTO | MODE BT | MODE USB | DISARM | HELP");
  Serial.println("SAFETY,NO_CAR_COMMANDS_NO_MOTOR_GPIO");
}

static void printStatus() {
  RoutedState out=router.tick(millis());
  Serial.printf("STATUS,mode=%s,source=%s,armed=%d,fresh=%d,host=%d,bt_fresh=%d\n",
    modeName(router.mode()),sourceName(out.source),int(out.armed),
    int(out.fresh),int(usbHostReady),
    int(lastBtConnected));
}

static void processSerial(const char* cmd) {
  if (strcmp(cmd,"STATUS")==0) printStatus();
  else if (strcmp(cmd,"HELP")==0) printHelp();
  else if (strcmp(cmd,"DISARM")==0) {router.reset();Serial.println("OK,DISARM");}
  else if (strcmp(cmd,"MODE AUTO")==0) {router.setMode(Mode::AUTO);Serial.println("OK,MODE,AUTO");}
  else if (strcmp(cmd,"MODE BT")==0) {router.setMode(Mode::BLUETOOTH_ONLY);Serial.println("OK,MODE,BT");}
  else if (strcmp(cmd,"MODE USB")==0) {router.setMode(Mode::USB_ONLY);Serial.println("OK,MODE,USB");}
  else Serial.println("ERROR,UNKNOWN_COMMAND");
}
static void readCommands() {
  while (Serial.available()) {
    int x=Serial.read();
    if (x=='\r'||x=='\n') {
      if (serialLen) {serialCommand[serialLen]=0;processSerial(serialCommand);serialLen=0;}
    } else if (x>=32&&x<127&&serialLen+1<sizeof(serialCommand)) {
      serialCommand[serialLen++]=char(x);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.println("BOOT,SHELL_RC_PRO_STANDALONE,VERSION=1.2.2_SUPERVISED_HELD_INPUT");
  Serial.println("CHIP,ESP32_CLASSIC_BT_BLE,FLASH=4MB");
  printHelp();
  // Bluepad32 supports Xbox Wireless over BT Classic and BLE.
  BP32.setup(&onConnectedController,&onDisconnectedController);
  // BTstack must be controlled from the BTstack Core0 thread.
  ferrarible::init();
  BP32.enableVirtualDevice(false);
  BP32.enableNewBluetoothConnections(true);
  Serial.println("BT,BLUEPAD32_READY,PAIRING_ALLOWED");

  // USB Host Shield 2.0, MAX3421E only. Missing shield must never
  // block Bluetooth startup. NOTE: no shield connected = usbHostReady=0.
  SPI.begin(18,19,23,5);
  usbHostReady=(usb.Init()==0);
  Serial.printf("USB_HOST,%s,MAX3421E_CS=5,INT=17\n",
    usbHostReady?"READY":"NOT_DETECTED");
  Serial.println("READY,STANDALONE_PROBE,FERRARI_488_MAC=13:05:AA:04:66:96");
  Serial.println("SAFETY,DRIVE_REQUIRES_FERRARI_READY_XBOX_FRESH_START_650MS");
  drive.resetAll(millis());
}

void loop() {
  uint32_t now=millis();
  handleBluetooth(now);
  handleXbox360(now);
  readCommands();
  RoutedState out=router.tick(now);
  if (ferrarible::mustDisarm.exchange(false)) {
    router.reset();
    drive.resetAll(now);
    out=router.tick(now);
    Serial.println("SAFETY,CAR_LINK_CHANGED,DISARMED");
  }
  // PRIORITY: Connect Xbox Wireless Controller BEFORE Ferrari BLE.
  const ferrarible::State carState=ferrarible::current();
  // Xbox 1914 BLE HID and Ferrari BLE GATT use the SAME LE scanner.
  // Bluepad32 owns gamepad pairing: allow it to finish FIRST. Searching for
  // the Ferrari before the Xbox can monopolize/override scan parameters.
  const bool xboxPresent=(out.source==Source::BLUETOOTH && out.fresh);
  if(xboxPresent) lastGamepadLinkMs=now;
  if(!xboxPresent && !ferrarible::xboxGamepadReady.load() &&
     !ferrarible::xboxAwaitingReport.load() &&
     ferrarible::xboxHciConnected.load() &&
     uint32_t(now-ferrarible::xboxHciSince.load())>11500u &&
     uint32_t(now-ferrarible::lastXboxRecoveryMs.load())>14000u) {
      if(ferrarible::request(ferrarible::Action::RECOVER_XBOX_LINK)) {
        Serial.println("XBOX_RECOVERY,HCI_LINK_STUCK_RETRYING_BONDED_CONTROLLER");
      }
  }
  if (!xboxPresent && carState==ferrarible::State::IDLE &&
      uint32_t(now-lastGamepadRecoveryMs)>17000u &&
      uint32_t(now-lastGamepadLinkMs)>1500u &&
      // Only recover if BTstack has genuinely stopped receiving BLE adverts.
      // Frequent restarts while BLE scan is active abort Xbox pairing.
      uint32_t(now-ferrarible::lastAnyAdMs.load())>13000u) {
      if(ferrarible::request(ferrarible::Action::RECOVER_XBOX_SCAN)) {
        lastGamepadRecoveryMs=now;
        Serial.println("BT_RECOVERY,BLUEPAD32_SCAN_RESTARTED");
      }
  }
  if (!xboxPresent) {
    // No car connection without controller. Disarm before disconnecting.
    if((carState==ferrarible::State::READY ||
        carState==ferrarible::State::CONNECTING ||
        carState==ferrarible::State::DISCOVERING ||
        carState==ferrarible::State::SCANNING) &&
       uint32_t(now-lastCarActionMs)>=250u) {
      router.reset();
      drive.resetAll(now);
      if(ferrarible::request(ferrarible::Action::DISCONNECT))
        lastCarActionMs=now;
      Serial.println("BT_PRIORITY,XBOX_WAITING,CAR_DISCONNECT");
    }
  } else if (uint32_t(now-lastCarActionMs)>=2000u &&
      (carState==ferrarible::State::IDLE ||
       carState==ferrarible::State::FAILED)) {
    // Only after controller reports arrive do we search Ferrari.
    if(ferrarible::request(ferrarible::Action::SCAN)) {
      lastCarActionMs=now;
      Serial.println("BT_PRIORITY,XBOX_CONNECTED,SEARCHING_FERRARI");
    }
  } else if (carState==ferrarible::State::SCANNING &&
             uint32_t(now-lastCarActionMs)>=6000u) {
    // Ferrari off: stop our LE scan to give BT HID another opportunity.
    if(ferrarible::request(ferrarible::Action::DISCONNECT))
      lastCarActionMs=now;
  }
  // BACK: physically reconnect Ferrari after disabling motor; PC's
  // original app uses BACK for fast BLE reconnection.
  bool backDown=out.fresh && (out.buttons & 64u);
  bool backEdge=backDown && !(lastButtonsBridge&64u);
  lastButtonsBridge=out.fresh?uint8_t(out.buttons):0u;
  if(backEdge && carState==ferrarible::State::READY) {
    router.reset(); drive.resetAll(now);
    ferrarible::request(ferrarible::Action::DISCONNECT);
    Serial.println("FERRARI,BACK_RECONNECT_REQUESTED,DISARMED");
  }
  shellrc::CarPacket packet=drive.tick(out,carState==ferrarible::State::READY,now);
  if(carState==ferrarible::State::READY &&
      uint32_t(now-lastCarActionMs)>=50u) {
    if(ferrarible::sendPacket(packet.data))lastCarActionMs=now;
  }
  if(packet.armed!=lastReportedArm) {
    lastReportedArm=packet.armed;
    Serial.printf("MOTOR,ARMED=%d,DUTY=%.2f,MODE=%s\n",int(packet.armed),
                  double(packet.duty),packet.direct?"DIRECT":"PULSE");
  }
  if(carState!=lastCarState || uint32_t(now-lastCarStatusMs)>=3500u) {
    lastCarState=carState;
    lastCarStatusMs=now;
    Serial.printf("CAR,STATE=%s,ERR=%u,SCAN=%lu,CONN=%lu,TX_NEUTRAL=%lu\n",
        ferrarible::label(carState),unsigned(ferrarible::lastError.load()),
        (unsigned long)ferrarible::scans.load(),
        (unsigned long)ferrarible::connections.load(),
        (unsigned long)ferrarible::writeCount.load());
    Serial.printf("CAR_DRIVE,ARMED=%d,MODE=%s,TX=%lu\n",
        int(packet.armed),packet.direct?"DIRECT":"PULSE",
        (unsigned long)ferrarible::dataWrites.load());
  }
  if (out.source!=previousSource||out.armed!=previousArmed) {
    previousSource=out.source;
    previousArmed=out.armed;
    Serial.printf("STATE,source=%s,armed=%d\n",sourceName(out.source),int(out.armed));
  }
  if (uint32_t(now-lastPrint)>=kOutputEveryMs) {
    lastPrint=now;
    if (out.fresh) {
      // INPUT2 always reports RAW Xbox state. On the PC the original
      // Shell RC Pro applies its own safety, ramp, light and turbo logic.
      // Source is never routed directly to motors on this board.
      Serial.printf("INPUT2,%s,%d,%u,%u,%u\n",
        sourceName(out.source),int(out.raw_steer),
        unsigned(out.raw_rt),unsigned(out.raw_lt),unsigned(out.buttons));
    }
  }
  if(uint32_t(now-lastDiagMs)>=4000u) {
    lastDiagMs=now;
    Serial.printf("BT_DIAG,XBOX_ADS=%lu,OTHER_ADS=%lu,RESTARTS=%lu,LAST_XBOX_MS=%lu\n",
        (unsigned long)ferrarible::xboxAds.load(),
        (unsigned long)ferrarible::otherAds.load(),
        (unsigned long)ferrarible::scanRecoveries.load(),
        (unsigned long)ferrarible::lastXboxAdMs.load());
    Serial.printf("XBOX_ADV,TYPE=%u,ADDR_TYPE=%u,ADV0=%lu,ADV1=%lu,ADV2=%lu,ADV_OTHER=%lu,APPEARANCE=%u,NO_APPEARANCE=%lu,BONDED=%lu,LE_STATUS=%u,LE_MS=%lu,DISCONNECT_REASON=%u\n",
        unsigned(ferrarible::xboxAdvLastType.load()),
        unsigned(ferrarible::xboxAdvAddrType.load()),
        (unsigned long)ferrarible::xboxAdv0.load(),
        (unsigned long)ferrarible::xboxAdv1.load(),
        (unsigned long)ferrarible::xboxAdv2.load(),
        (unsigned long)ferrarible::xboxAdv3.load(),
        unsigned(ferrarible::xboxAppearance.load()),
        (unsigned long)ferrarible::xboxNoAppearance.load(),
        (unsigned long)ferrarible::xboxBondMatches.load(),
        unsigned(ferrarible::lastLeCompleteStatus.load()),
        (unsigned long)ferrarible::lastLeCompleteMs.load(),
        unsigned(ferrarible::lastBtDisconnectReason.load()));
    Serial.printf("XBOX_HOLD,SUPERVISION_MS=%u,LIVE=%d\n",unsigned(ferrarible::xboxSupervisionMs.load()),int(ferrarible::supervisedXboxLive(now)));
    Serial.printf("XBOX_LINK,HCI=%d,READY=%d,AGE_MS=%lu,RECOVERIES=%lu\n",
       int(ferrarible::xboxHciConnected.load()),
       int(ferrarible::xboxGamepadReady.load()),
       (unsigned long)(ferrarible::xboxHciConnected.load()?
           uint32_t(now-ferrarible::xboxHciSince.load()):0u),
       (unsigned long)ferrarible::xboxLinkRecoveries.load());
  }
  if (uint32_t(now-lastHealth)>=2500) {
    lastHealth=now;
    printStatus();
  }
  delay(10);
}
