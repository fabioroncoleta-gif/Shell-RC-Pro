/*
 * NEXO / Shell RC Pro: classic ESP32 dual-gamepad BLE-car gateway.
 * Player 1: Xbox360 receiver through MAX3421E (SPI18/19/23, CS5, INT17).
 * Player 2: Bluepad32 native Bluetooth; independent car, router and DriveModel.
 * Headless build: external MAX3421E only; no LCD driver or LCD GPIO bus.
 * Motors require a fresh valid gamepad, ready car and physical START in neutral.
 * BLE operations run on the BTstack thread; Arduino services USB and drive routing.
 */
#include <Arduino.h>
#include <SPI.h>
#include <Bluepad32.h>
#include <Usb.h>
#include <usbhub.h>
#include <XBOXRECV.h>
// MAX3421E USB Host with Xbox360 receiver; native ESP32 Bluetooth retained.
#include "src/input_core.h"
#include "src/control_owner.h"
static bool secondCarOwns(const uint8_t*);
#include "src/ferrari_ble_link.h"
#include "src/second_car_ble.h"
#include "src/drive_logic.h"

using shellrc::GamepadSample;
using shellrc::RoutedState;
using shellrc::Router;
using shellrc::Source;
using shellrc::Mode;
static bool secondCarOwns(const uint8_t* a){return secondcar::owns(a);}

static Router router; // Player 1: Xbox360 USB exclusively.
static Router btRouter; // Player 2: native Bluetooth exclusively.
static shellrc::DriveModel btDrive;
static bool btNeedStartRelease=true;
static uint32_t lastBtDriveTickMs=0,lastBtCarTxMs=0,lastPairScanMs=0;

// Dynamic controller-to-car assignment.
// Routers remain source-specific, but cars are assigned by VALID controller arrival order.
static ControlOwner car1Owner=ControlOwner::NONE, car2Owner=ControlOwner::NONE;
static bool lastBtConnected = false;
static bool usbControllerConnected=false;
static uint32_t usbConnectedSince=0, btConnectedSince=0;
static uint32_t lastUsbReportSeq=0;
static bool usbInputStale=true;
static uint32_t lastUsbStaleLog=0;
static constexpr uint32_t kUsbInputDeadlineMs=220; // fail closed on missing real HID data

static bool ownerActive(ControlOwner o) {
  return o==ControlOwner::USB360 ? usbControllerConnected :
         o==ControlOwner::BT ? lastBtConnected : false;
}
static const char* ownerName(ControlOwner o) {
  return o==ControlOwner::USB360 ? "XBOX360_USB" :
         o==ControlOwner::BT ? "XBOX_BT" : "NONE";
}

static uint8_t lastBtCarPacket[8]={1,0,0,0,0,0,0,0};
static ControllerPtr gamepads[BP32_MAX_GAMEPADS] = {};
static USB usb;
static XBOXRECV xbox360(&usb);
static bool usbHostReady = false;
static uint32_t usbRecoveryStarted=0,lastUsbRecoveryMs=0;
static bool forceUsbRecovery=false;
// MAX3421E profile; GPIO16 and GPIO27 are not used.
static int usbInitResult=-127;
static uint8_t usbRevision=0;
static void usbDiag() {
  uint8_t mask=0;
  for(int i=0;i<4;i++) if(xbox360.Xbox360Connected[i]) mask|=(1u<<i);
  Serial.printf("USB_DIAG,REV=%02X,INIT=%d,READY=%d,TASK=%02X,ENUM_ERR=%02X,RECEIVER=%d,CONTROLLERS=%02X,INT=%d\n",
    unsigned(usbRevision),usbInitResult,int(usbHostReady),unsigned(usb.getUsbTaskState()),unsigned(usb.getUsbTaskError()),
    int(xbox360.XboxReceiverConnected),unsigned(mask),digitalRead(17));
  uint32_t t=millis();uint32_t seen=xbox360.lastValidReportMs[0];
  Serial.printf("USB_HID,PACKETS=%lu,AGE_MS=%lu,IDLE_NEUTRAL=%u,OWNER=%s\n",
      (unsigned long)xbox360.validReportCount[0],
      (unsigned long)(seen?uint32_t(t-seen):0),
      unsigned(usbInputStale),ownerName(car1Owner));
}
static void usbHostInit() {
  pinMode(5,OUTPUT);digitalWrite(5,HIGH);pinMode(17,INPUT);
  SPI.begin(18,19,23,5);
  delay(50);
  // MAX3421E powers up in half-duplex mode: enable separate MISO first.
  usb.regWr(rPINCTL, bmFDUPSPI | bmINTLEVEL);
  delay(5);
  usbRevision=usb.regRd(rREVISION);
  if(usbRevision!=0x01 && usbRevision!=0x12 && usbRevision!=0x13) {
    usbInitResult=-127;usbHostReady=false;
    Serial.printf("USB_HOST,SPI_FAIL,REV=%02X,CHECK_3V3_RESET_SCK_MISO_MOSI_CS\n",unsigned(usbRevision));
  } else {
    xbox360.Release();
    usb.GetAddressPool().FreeAddress(0x41); // clear stale addresses on host reset
    usbInitResult=usb.Init();usbHostReady=(usbInitResult==0);
    usb.setUsbTaskState(USB_DETACHED_SUBSTATE_INITIALIZE);
    Serial.printf("USB_HOST,%s,REV=%02X,INIT=%d,CS=5,INT=17\n",
      usbHostReady?"READY":"INIT_FAILED",unsigned(usbRevision),usbInitResult);
  }
  usbDiag();
}
static uint32_t lastUsbPoll = 0;
static uint32_t lastPrint = 0;
static uint32_t lastHealth = 0;
static uint32_t lastBtData = 0;
static GamepadSample lastBtSample = {};
static uint32_t lastBtKeepAlive = 0;
static char serialCommand[40];
static size_t serialLen = 0;
static Source previousSource = Source::NONE;
static bool previousArmed = false;
static const uint32_t kOutputEveryMs = 400; // Keep serial telemetry off the control critical path.
static uint32_t lastCarTxMs=0, lastDriveTickMs=0;
static uint8_t lastCarTxPacket[8]={1,0,0,0,0,0,0,0};
static uint32_t maxUsbTaskUs=0;
static uint32_t lastCarActionMs=0;
static uint32_t lastCarStatusMs=0;
static ferrarible::State lastCarState=ferrarible::State::IDLE;
static bool lastCarLink=false;
static uint32_t lastGamepadRecoveryMs=0;
static uint32_t lastGamepadLinkMs=0;
static uint32_t lastDiagMs=0;
static shellrc::DriveModel drive;
static uint8_t lastButtonsBridge=0;
static uint16_t lastFleetButtons=0;
static bool requireStartRelease=true;
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

static RoutedState ownerState(ControlOwner o,uint32_t now) {
  if(o==ControlOwner::USB360)return router.tick(now);
  if(o==ControlOwner::BT)return btRouter.tick(now);
  return RoutedState{};
}
static void resetRoute(ControlOwner o) {
  if(o==ControlOwner::USB360) router.reset();
  else if(o==ControlOwner::BT) btRouter.reset();
}
static void resetCar1(uint32_t now) {
  resetRoute(car1Owner);
  drive.resetAll(now);
}
static void resetCar2(uint32_t now) {
  resetRoute(car2Owner);
  btDrive.resetAll(now);
}
static void syncOwners(uint32_t now) {
  // Release only the slot whose controller actually disappeared. Never silently
  // move a live controller to the other car while driving.
  if(car1Owner!=ControlOwner::NONE && !ownerActive(car1Owner)) {
    Serial.printf("ASSIGN,CAR1,RELEASE,%s\n",ownerName(car1Owner));
    resetCar1(now); car1Owner=ControlOwner::NONE; requireStartRelease=true;
  }
  if(car2Owner!=ControlOwner::NONE && !ownerActive(car2Owner)) {
    Serial.printf("ASSIGN,CAR2,RELEASE,%s\n",ownerName(car2Owner));
    resetCar2(now); car2Owner=ControlOwner::NONE; btNeedStartRelease=true;
  }

  auto assigned=[](ControlOwner o){return car1Owner==o || car2Owner==o;};
  ControlOwner first=ControlOwner::NONE;
  if(usbControllerConnected && !assigned(ControlOwner::USB360)) first=ControlOwner::USB360;
  if(lastBtConnected && !assigned(ControlOwner::BT)) {
    if(first==ControlOwner::NONE ||
       (btConnectedSince && usbConnectedSince && btConnectedSince<usbConnectedSince))
      first=ControlOwner::BT;
  }

  if(car1Owner==ControlOwner::NONE && first!=ControlOwner::NONE) {
    car1Owner=first; resetCar1(now); requireStartRelease=true;
    Serial.printf("ASSIGN,CAR1,%s,FIRST_VALID_CONTROLLER\n",ownerName(first));
  }

  ControlOwner remaining=ControlOwner::NONE;
  if(usbControllerConnected && !assigned(ControlOwner::USB360)) remaining=ControlOwner::USB360;
  if(lastBtConnected && !assigned(ControlOwner::BT)) remaining=ControlOwner::BT;
  if(car2Owner==ControlOwner::NONE && remaining!=ControlOwner::NONE) {
    car2Owner=remaining; resetCar2(now); btNeedStartRelease=true;
    Serial.printf("ASSIGN,CAR2,%s,SECOND_VALID_CONTROLLER\n",ownerName(remaining));
  }
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
      // Sending an Xbox LED command before HID class negotiation can
      // interfere with early BLE setup on some controllers.
      if(ctl->isGamepad())ctl->setPlayerLEDs(1 << i);
      return;
    }
  }
  Serial.println("IGNORED,NO_GAMEPAD_SLOT");
}

static void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (gamepads[i] == ctl) gamepads[i] = nullptr;
  }
  btRouter.disconnected(Source::BLUETOOTH);btNeedStartRelease=true;
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
        btConnectedSince=now;
        ferrarible::xboxGamepadReady.store(true);
        ferrarible::xboxAwaitingReport.store(false);
        ctl->setPlayerLEDs(1 << i);
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
          (in.start ? 128u : 0u) |
          ((ctl->dpad() & DPAD_LEFT) ? 256u : 0u) |
          ((ctl->dpad() & DPAD_RIGHT) ? 512u : 0u);
      btRouter.record(Source::BLUETOOTH,in,now);
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
      btRouter.record(Source::BLUETOOTH,lastBtSample,now);
      lastBtKeepAlive=now;
      lastBtData=now;
    }
  }
  if (lastBtConnected && uint32_t(now-lastBtData)>Router::kWatchdogMs) {
    btRouter.disconnected(Source::BLUETOOTH);btNeedStartRelease=true;
    lastBtConnected = false;
    lastBtSample = {};
    Serial.println("WATCHDOG,XBOX_BT,DISARMED");
  }
}

static void handleXbox360(uint32_t now) {
  if (!usbHostReady) return;  // still scan for Bluetooth when shield absent
  const uint32_t usbStartedUs=micros();
  usb.Task();
  if(usb.getUsbTaskState()!=USB_STATE_RUNNING){router.disconnected(Source::XBOX360_USB);return;}
  const uint32_t usbDurationUs=uint32_t(micros()-usbStartedUs);
  if(usbDurationUs>maxUsbTaskUs)maxUsbTaskUs=usbDurationUs;
  if (uint32_t(now-lastUsbPoll)<4) return;
  lastUsbPoll=now;
  if (!xbox360.XboxReceiverConnected) {
    if(usbControllerConnected) Serial.println("DISCONNECTED,XBOX360_USB,DISARMED");
    usbControllerConnected=false;
    router.disconnected(Source::XBOX360_USB);
    return;
  }
  int first = -1;
  for (int i=0;i<4;i++) {
    if (xbox360.Xbox360Connected[i] & 0x80) {first=i;break;}
  }
  if (first < 0) {
    if(usbControllerConnected) Serial.println("DISCONNECTED,XBOX360_USB,DISARMED");
    usbControllerConnected=false;
    router.disconnected(Source::XBOX360_USB);
    return;
  }
  // The receiver can stay paired while its HID endpoint stops reporting.
  // Never re-record a cached R2/L2 value as if it were a fresh packet.
  const uint32_t reportAt=xbox360.lastValidReportMs[first];
  const uint32_t seq=xbox360.validReportCount[first];
  // usb.Task() may receive the HID report AFTER the loop time snapshot.
  // Compare against a timestamp captured after USB polling to avoid unsigned
  // underflow incorrectly classifying a brand-new report as stale.
  const uint32_t usbSampleNow=millis();
  if(!reportAt) {
    // The USB receiver may be paired but has not delivered any valid HID
    // state yet. Never infer gamepad commands from its slot bit alone.
    return;
  }
  if(!usbControllerConnected){
    usbControllerConnected=true;usbConnectedSince=usbSampleNow;
    Serial.printf("CONNECTED,XBOX360_USB,slot=%d,FIRST_VALID_REPORT=1\n",first);
  }
  if(uint32_t(usbSampleNow-reportAt)>kUsbInputDeadlineMs) {
    // Xbox 360 wireless receiver reports can be event-driven. Silence at idle
    // must NOT unpair the controller or erase START arming state. However a
    // held old throttle or button must NEVER continue driving.
    if(!usbInputStale) {
      usbInputStale=true;
      Serial.printf("SAFETY,USB_HID_IDLE_NEUTRAL,AGE_MS=%lu\n",
          (unsigned long)uint32_t(usbSampleNow-reportAt));
    }
    if(uint32_t(usbSampleNow-lastUsbStaleLog)>=75u) {
      GamepadSample neutral;
      neutral.connected=true;
      router.record(Source::XBOX360_USB,neutral,usbSampleNow);
      lastUsbStaleLog=usbSampleNow;
    }
    return;
  }
  if(seq==lastUsbReportSeq)return;
  lastUsbReportSeq=seq;
  if(usbInputStale) {
    usbInputStale=false;
    Serial.println("USB_HID,NEW_REPORT_AFTER_IDLE");
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
  Serial.println("COMMANDS: STATUS | USB | USB RETRY | MODE AUTO | MODE USB | MODE BT | DISARM | CARS | SCAN | CAR 1..8 | NEXT | PREV | HELP");
  Serial.println("SAFETY,DRIVE_REQUIRES_VALID_FFF1_NEUTRAL_START");
}

static void printStatus() {
  RoutedState out=ownerState(car1Owner,millis());
  Serial.printf("STATUS,mode=%s,source=%s,armed=%d,fresh=%d,host=%d,bt_fresh=%d,CAR1=%s,CAR2=%s\n",
    modeName(router.mode()),sourceName(out.source),int(out.armed),
    int(out.fresh),int(usbHostReady),
    int(lastBtConnected),ownerName(car1Owner),ownerName(car2Owner));
}

static void processSerial(const char* cmd) {
  if (strcmp(cmd,"STATUS")==0) printStatus();
  else if(strcmp(cmd,"PAIR")==0)ferrarible::request(ferrarible::Action::PAIR_SECOND);
  else if (strcmp(cmd,"USB")==0) usbDiag();
  else if (strcmp(cmd,"USB RETRY")==0) {forceUsbRecovery=true;Serial.println("USB_RECOVERY,REQUESTED");}
  else if (strcmp(cmd,"HELP")==0) printHelp();
  else if(strcmp(cmd,"CARS")==0)ferrarible::fleetCommand(-3);
  else if(strcmp(cmd,"SCAN")==0){router.reset();drive.resetAll(millis());ferrarible::fleetCommand(-2);}
  else if(strcmp(cmd,"NEXT")==0 || strcmp(cmd,"PREV")==0){router.reset();drive.resetAll(millis());ferrarible::fleetCommand(strcmp(cmd,"NEXT")==0?-4:-5);}
  else if(strncmp(cmd,"CAR ",4)==0 && cmd[4]>='1'&&cmd[4]<='8'&&cmd[5]==0){router.reset();drive.resetAll(millis());ferrarible::fleetCommand(cmd[4]-'1');}
  else if (strcmp(cmd,"DISARM")==0) {router.reset();btRouter.reset();btDrive.resetAll(millis());btNeedStartRelease=true;Serial.println("OK,DISARM,BOTH_PLAYERS");}
  else if (strcmp(cmd,"MODE AUTO")==0) {router.setMode(Mode::USB_ONLY);Serial.println("OK,DUAL,DYNAMIC_FIRST_CONTROLLER_FIRST_CAR");}
  else if (strcmp(cmd,"MODE BT")==0) {Serial.println("OK,DUAL,DYNAMIC_ASSIGNMENT");}
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

static void tickSecondPlayer(uint32_t now){
  if(secondcar::mustDisarm.exchange(false)){
    resetCar2(now);btNeedStartRelease=true;
  }
  RoutedState out=ownerState(car2Owner,now);
  if(btNeedStartRelease){
    resetCar2(now);
    if(out.fresh && !(out.buttons&128u))btNeedStartRelease=false;
    out=ownerState(car2Owner,now);
  }
  // BLE pairing of car 2 must remain independent from HID report freshness.
  // A paired but idle Xbox360 may not emit reports continuously.
  // Motor safety is handled separately by DriveModel.tick(out,...).
  secondcar::enabled.store(car2Owner!=ControlOwner::NONE && ownerActive(car2Owner));
  static shellrc::CarPacket packet;
  if(uint32_t(now-lastBtDriveTickMs)>=10u || !out.fresh || !out.armed || !secondcar::ready()){
    lastBtDriveTickMs=now;packet=btDrive.tick(out,secondcar::ready(),now);
  }
  bool changed=memcmp(packet.data,lastBtCarPacket,8)!=0;
  uint32_t age=uint32_t(now-lastBtCarTxMs);
  if(secondcar::ready() && ((changed && age>=18u)||age>=55u)){
    if(secondcar::sendPacket(packet.data)){memcpy(lastBtCarPacket,packet.data,8);lastBtCarTxMs=now;}
  }
  static uint32_t lastStatus=0;
  if(uint32_t(now-lastStatus)>=2500u){
    lastStatus=now;
    Serial.printf("PLAYER2,CONTROLLER=%s,LINKED=%d,FRESH=%d,ARMED=%d,CAR_STATE=%u,CAR_READY=%d,TX=%lu,ERR=%u,STEER=%d,RT=%u,LT=%u\n",
      ownerName(car2Owner),int(ownerActive(car2Owner)),int(out.fresh),int(packet.armed),unsigned(secondcar::state.load()),int(secondcar::ready()),
      (unsigned long)secondcar::writes.load(),unsigned(secondcar::lastError.load()),int(out.raw_steer),unsigned(out.raw_rt),unsigned(out.raw_lt));
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.println("BOOT,SHELL_RC_PRO_STANDALONE,VERSION=1.4.14_CAR2_PAIRING_STABLE");
  Serial.println("CHIP,ESP32_CLASSIC_BT_BLE,FLASH=4MB");
  printHelp();
  // Bluepad32 supports Xbox Wireless over BT Classic and BLE.
  BP32.setup(&onConnectedController,&onDisconnectedController);
  // BTstack must be controlled from the BTstack Core0 thread.
  ferrarible::init();
  secondcar::init();
  router.setMode(Mode::USB_ONLY);btRouter.setMode(Mode::BLUETOOTH_ONLY);
  router.enableAutoArm(true);btRouter.enableAutoArm(true);
  btDrive.resetAll(millis());
  BP32.enableVirtualDevice(false);
  BP32.enableNewBluetoothConnections(true);
  Serial.println("BT,BLUEPAD32_READY,PAIRING_ALLOWED");

  // No LCD/SD: release the formerly assigned wires.
  pinMode(16,INPUT);pinMode(27,INPUT);
  usbHostInit();
  Serial.println("READY,STANDALONE_MULTICAR,SHELL_FFF1");
  Serial.println("SAFETY,DRIVE_REQUIRES_FERRARI_READY_XBOX_FRESH_START_650MS");
  drive.resetAll(millis());
}

void loop() {
  uint32_t now=millis();
  handleBluetooth(now);
  handleXbox360(now);
  readCommands();
  // USB polling and BLE dispatch advance millis() during this loop.
  // Use a new timestamp for router freshness to avoid uint32 underflow
  // when an input was timestamped later than this loop's initial snapshot.
  now=millis();
  syncOwners(now);
  // Enable pairing once after car 1 is ready. Repeatedly restarting the
  // shared LE scanner every 5s can interrupt Xbox HID negotiation.
  if(!lastBtConnected && !ferrarible::xboxHciConnected.load() &&
      ferrarible::current()==ferrarible::State::READY &&
      lastPairScanMs==0u){
    if(ferrarible::request(ferrarible::Action::PAIR_SECOND))lastPairScanMs=now?now:1u;
  }
  router.allowAutoArm(
      (car1Owner==ControlOwner::USB360 && ferrarible::current()==ferrarible::State::READY) ||
      (car2Owner==ControlOwner::USB360 && secondcar::ready()));
  btRouter.allowAutoArm(
      (car1Owner==ControlOwner::BT && ferrarible::current()==ferrarible::State::READY) ||
      (car2Owner==ControlOwner::BT && secondcar::ready()));
  tickSecondPlayer(now);
  const bool usbFailed=!usbHostReady || usb.getUsbTaskState()==USB_STATE_ERROR;
  if(!usbRecoveryStarted && (forceUsbRecovery || (usbFailed && uint32_t(now-lastUsbRecoveryMs)>=5000u))){
    forceUsbRecovery=false;router.reset();drive.resetAll(now);requireStartRelease=true;
    usbRecoveryStarted=now?now:1u;
    Serial.println("USB_RECOVERY,DISARMED,WAITING_NEUTRAL");
  }
  if(usbRecoveryStarted && uint32_t(now-usbRecoveryStarted)>=100u){
    usbHostInit();lastUsbRecoveryMs=millis();usbRecoveryStarted=0;
    Serial.println("USB_RECOVERY,REINITIALIZED,START_REQUIRED");
  }
  RoutedState out=ownerState(car1Owner,now);
  if (ferrarible::mustDisarm.exchange(false)) {
    requireStartRelease=true;
    resetCar1(now);
    out=ownerState(car1Owner,now);
    Serial.println("SAFETY,CAR_LINK_CHANGED,DISARMED");
  }
  if(requireStartRelease){
    resetCar1(now);
    if(out.fresh && !(out.buttons&128u))requireStartRelease=false;
    out=ownerState(car1Owner,now);
  }
  // PRIORITY: Connect Xbox Wireless Controller BEFORE Ferrari BLE.
  uint16_t fleetButtons=out.fresh?out.buttons&768u:0;
  uint16_t fleetEdges=fleetButtons&~lastFleetButtons;lastFleetButtons=fleetButtons;
  if(fleetEdges){resetCar1(now);ferrarible::fleetCommand((fleetEdges&512u)?-4:-5);out=ownerState(car1Owner,now);}
  const ferrarible::State carState=ferrarible::current();
  // Xbox 1914 BLE HID and Ferrari BLE GATT use the SAME LE scanner.
  // Bluepad32 owns gamepad pairing: allow it to finish FIRST. Searching for
  // the Ferrari before the Xbox can monopolize/override scan parameters.
  const bool xboxPresent=(out.source==Source::BLUETOOTH && out.fresh) ||
      (ferrarible::xboxHciConnected.load() &&
       (ferrarible::xboxAwaitingReport.load() || ferrarible::xboxGamepadReady.load()));
  if(xboxPresent || (out.source==Source::XBOX360_USB && out.fresh)) lastGamepadLinkMs=now;
  // A bare HCI link is NOT a usable gamepad. Do not start a car scan
  // while its HID handshake is incomplete; recover the Xbox first.
  const bool xboxHidReady = (out.fresh &&
      (out.source==Source::BLUETOOTH || out.source==Source::XBOX360_USB)) ||
      ferrarible::xboxGamepadReady.load();
  // An idle Xbox 360 is still paired to its USB receiver. The receiver
  // may not send HID reports until a button changes; control input freshness
  // must NEVER cancel the independent BLE connection to the car.
  bool usb360RadioPaired=false;
  if(usbHostReady && usb.getUsbTaskState()==USB_STATE_RUNNING &&
     xbox360.XboxReceiverConnected) {
    for(int i=0;i<4;i++)
      if(xbox360.Xbox360Connected[i]&0x80u){usb360RadioPaired=true;break;}
  }
  const bool carLinkAllowed = xboxHidReady || usbControllerConnected ||
      usb360RadioPaired || carState==ferrarible::State::READY;
  if(!ferrarible::xboxGamepadReady.load() &&
     ferrarible::xboxHciConnected.load() &&
     uint32_t(now-ferrarible::xboxHciSince.load())>60000u &&
     uint32_t(now-ferrarible::lastXboxRecoveryMs.load())>60000u) {
      if(ferrarible::request(ferrarible::Action::RECOVER_XBOX_LINK)) {
        Serial.println("XBOX_RECOVERY,HCI_LINK_STUCK_RETRYING_BONDED_CONTROLLER");
      }
  }
  if (!xboxPresent && !usbControllerConnected && carState==ferrarible::State::IDLE &&
      uint32_t(now-lastGamepadRecoveryMs)>11000u &&
      uint32_t(now-lastGamepadLinkMs)>1500u &&
      // Only recover if BTstack has genuinely stopped receiving BLE adverts.
      // Frequent restarts while BLE scan is active abort Xbox pairing.
      uint32_t(now-ferrarible::lastAnyAdMs.load())>8500u) {
      if(ferrarible::request(ferrarible::Action::RECOVER_XBOX_SCAN)) {
        lastGamepadRecoveryMs=now;
        Serial.println("BT_RECOVERY,BLUEPAD32_SCAN_RESTARTED");
      }
  }
  if (!carLinkAllowed) {
    // Input is not linked yet. Preserve safe disarmed state; don't scan until
    // USB controller is paired or Bluepad32 has usable HID input.
    if((carState==ferrarible::State::READY ||
        carState==ferrarible::State::CONNECTING ||
        carState==ferrarible::State::DISCOVERING ||
        carState==ferrarible::State::SCANNING) &&
       uint32_t(now-lastCarActionMs)>=250u) {
      resetCar1(now);
      if(ferrarible::request(ferrarible::Action::DISCONNECT))
        lastCarActionMs=now;
      Serial.println("BT_PRIORITY,XBOX_WAITING,CAR_DISCONNECT");
    }
  } else if (uint32_t(now-lastCarActionMs)>=800u &&
      (carState==ferrarible::State::IDLE ||
       carState==ferrarible::State::FAILED || carState==ferrarible::State::SELECTING)) {
    // Only after controller reports arrive do we search Ferrari.
    if(ferrarible::request(ferrarible::Action::SCAN)) {
      lastCarActionMs=now;
      Serial.println("BT_PRIORITY,XBOX_CONNECTED,SEARCHING_FERRARI");
    }
  } else if (carState==ferrarible::State::SCANNING &&
             uint32_t(now-lastCarActionMs)>=5000u) {
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
    resetCar1(now);
    ferrarible::request(ferrarible::Action::DISCONNECT);
    Serial.println("FERRARI,BACK_RECONNECT_REQUESTED,DISARMED");
  }
  // Preserve the original 10ms motion/pulse cadence while servicing USB faster.
  static shellrc::CarPacket packet;
  if(uint32_t(now-lastDriveTickMs)>=10u || !out.fresh || !out.armed ||
      carState!=ferrarible::State::READY) {
    lastDriveTickMs=now;
    packet=drive.tick(out,carState==ferrarible::State::READY,now);
  }
  const bool packetChanged=memcmp(packet.data,lastCarTxPacket,8)!=0;
  const uint32_t txAge=uint32_t(now-lastCarTxMs);
  if(carState==ferrarible::State::READY &&
      ((packetChanged && txAge>=12u) || txAge>=50u)) {
    if(ferrarible::sendPacket(packet.data)) {
      memcpy(lastCarTxPacket,packet.data,8);lastCarTxMs=now;
    }
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
    usbDiag();
    Serial.printf("LATENCY,USB_MAX_US=%lu,READ_MS=4,DRIVE_MS=10,TX_CHANGE_MS=10,TX_HEARTBEAT_MS=30\n",(unsigned long)maxUsbTaskUs);
    maxUsbTaskUs=0;
  }
  delay(1);
}
