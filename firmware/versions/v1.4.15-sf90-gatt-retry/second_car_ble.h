#pragma once
// Independent second GATT client. Every HCI/GATT operation runs on Core0.
// Player 1 remains owned by ferrarible; this client never adopts that peer.
#include <Arduino.h>
#include <atomic>
#include <Preferences.h>
#include "fleet_core.h"
namespace secondcar {
enum class State:uint8_t {IDLE,SCANNING,CONNECTING,DISCOVERING,READY,FAILED};
static std::atomic<State> state{State::IDLE};
static std::atomic<bool> enabled{false},mustDisarm{true},jobPending{false};
static std::atomic<uint32_t> writes{0},lastPublished{0};
static std::atomic<uint8_t> lastError{0};
static bd_addr_t target={};
static bool reserved=false,busy=false,validating=false;
static hci_con_handle_t handle=HCI_CON_HANDLE_INVALID;
static uint16_t valueHandle=0,properties=0;
static uint32_t phaseSince=0,nextScan=0,gattRetryAfter=0;
static uint8_t gattRetries=0;
static bool gattRetryPending=false;
static uint8_t pending[8]={1,0,0,0,0,0,0,0},transfer[8]={1,0,0,0,0,0,0,0};
static const uint8_t neutral[8]={1,0,0,0,0,0,0,0};
static shellrc::Fleet fleet;
static Preferences prefs;
static btstack_packet_callback_registration_t events={};
static btstack_context_callback_registration_t registerJob={},sendJob={};
static btstack_timer_source_t timer={};
static bool owns(const uint8_t* address){return reserved && memcmp(target,address,6)==0;}
static bool ready(){return state.load()==State::READY;}
static void set(State s){state.store(s);phaseSince=millis();}
static void fail(uint8_t err){
 lastError.store(err);mustDisarm.store(true);busy=false;validating=false;
 gattRetryPending=false;gattRetries=0;
 if(handle!=HCI_CON_HANDLE_INVALID)gap_disconnect(handle);
 set(State::FAILED);nextScan=millis()+2000u;
}
static void markReady(){
 validating=false;busy=false;lastError.store(0);mustDisarm.store(true);
 lastPublished.store(millis());set(State::READY);fleet.remember(target);
 prefs.putBytes("preferred",target,6);
 int i=fleet.find(target);
 Serial.printf("CAR2,READY,NAME=%s,ADDR=%02X:%02X:%02X:%02X:%02X:%02X,START_REQUIRED\n",
  i>=0?fleet.cars[i].name:"Shell BLE",target[0],target[1],target[2],target[3],target[4],target[5]);
}
static void gattCallback(uint8_t,uint16_t,uint8_t*,uint16_t);
static uint8_t write(const uint8_t* bytes){
 if(handle==HCI_CON_HANDLE_INVALID || !valueHandle || busy)return 0xfe;
 memcpy(transfer,bytes,8);
 if(properties&ATT_PROPERTY_WRITE_WITHOUT_RESPONSE){
  uint8_t err=gatt_client_write_value_of_characteristic_without_response(handle,valueHandle,8,transfer);
  if(!err)++writes;return err;
 }
 busy=true;
 uint8_t err=gatt_client_write_value_of_characteristic(&gattCallback,handle,valueHandle,8,transfer);
 if(err)busy=false;
 return err;
}
static void gattCallback(uint8_t type,uint16_t,uint8_t* packet,uint16_t){
 if(type!=HCI_EVENT_PACKET)return;
 uint8_t event=hci_event_packet_get_type(packet);
 if(event==GATT_EVENT_CHARACTERISTIC_QUERY_RESULT && !busy){
  if(gatt_event_characteristic_query_result_get_handle(packet)!=handle)return;
  gatt_client_characteristic_t ch={};gatt_event_characteristic_query_result_get_characteristic(packet,&ch);
  Serial.printf("CAR2_GATT,FFF1_HANDLE=%u,PROPERTIES=%u\n",
      unsigned(ch.value_handle),unsigned(ch.properties));
  if(ch.properties&(ATT_PROPERTY_WRITE|ATT_PROPERTY_WRITE_WITHOUT_RESPONSE)){
   valueHandle=ch.value_handle;properties=ch.properties;
  }
 }else if(event==GATT_EVENT_QUERY_COMPLETE){
  if(gatt_event_query_complete_get_handle(packet)!=handle)return;
  uint8_t err=gatt_event_query_complete_get_att_status(packet);
  if(busy){busy=false;if(err){fail(err);return;}++writes;if(validating)markReady();return;}
  if(err || !valueHandle){
    Serial.printf("CAR2_GATT,QUERY_FAIL,STATUS=%u,HANDLE=%u,ATTEMPT=%u\n",
        unsigned(err),unsigned(valueHandle),unsigned(gattRetries+1));
    if(gattRetries<2 && handle!=HCI_CON_HANDLE_INVALID){
      ++gattRetries;
      gattRetryAfter=millis()+240u;
      gattRetryPending=true;
      return;
    }
    int i=fleet.find(target);if(i>=0)fleet.cars[i].rejected=true;
    fail(err?err:0xfd);return;
  }
  validating=true;uint8_t result=write(neutral);
  if(result){validating=false;fail(result);}else if(!busy)markReady();
 }
}
static void onHci(uint8_t type,uint16_t,uint8_t* packet,uint16_t){
 if(type!=HCI_EVENT_PACKET)return;
 uint8_t event=hci_event_packet_get_type(packet);
 if(event==GAP_EVENT_ADVERTISING_REPORT && state.load()==State::SCANNING){
  bd_addr_t addr;gap_event_advertising_report_get_address(packet,addr);
  if(memcmp(addr,ferrarible::kTarget,6)==0)return;
  const uint8_t* data=gap_event_advertising_report_get_data(packet);
  unsigned len=gap_event_advertising_report_get_data_length(packet);char name[48]={};
  for(unsigned at=0;at<len;){unsigned n=data[at];if(!n || at+n>=len)break;
   if((data[at+1]==0x09 || data[at+1]==0x08)&&n>1){unsigned k=n-1;if(k>47)k=47;memcpy(name,data+at+2,k);name[k]=0;if(data[at+1]==0x09)break;}at+=n+1;
  }
  fleet.observe(addr,gap_event_advertising_report_get_address_type(packet),name,gap_event_advertising_report_get_rssi(packet));
 }else if(event==HCI_EVENT_LE_META){
  uint8_t sub=hci_event_le_meta_get_subevent_code(packet);
  if(sub==HCI_SUBEVENT_LE_CONNECTION_COMPLETE && state.load()==State::CONNECTING){
   bd_addr_t peer;hci_subevent_le_connection_complete_get_peer_address(packet,peer);
   if(memcmp(peer,target,6))return;
   uint8_t err=hci_subevent_le_connection_complete_get_status(packet);if(err){fail(err);return;}
   handle=hci_subevent_le_connection_complete_get_connection_handle(packet);
   valueHandle=0;properties=0;busy=false;
   gattRetries=0;gattRetryPending=false;
   set(State::DISCOVERING);
   uint16_t timeout=hci_subevent_le_connection_complete_get_supervision_timeout(packet);
   gap_update_connection_parameters(handle,6,12,0,timeout);
   err=gatt_client_discover_characteristics_for_handle_range_by_uuid16(&gattCallback,handle,1,0xffff,0xfff1);
   if(err)fail(err);
  }else if(sub==HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE &&
    hci_subevent_le_connection_update_complete_get_connection_handle(packet)==handle){
   Serial.printf("CAR2_BLE_TIMING,STATUS=%u,INTERVAL_US=%lu\n",
    unsigned(hci_subevent_le_connection_update_complete_get_status(packet)),
    (unsigned long)hci_subevent_le_connection_update_complete_get_conn_interval(packet)*1250u);
  }
 }else if(event==HCI_EVENT_DISCONNECTION_COMPLETE){
  if(hci_event_disconnection_complete_get_connection_handle(packet)!=handle || handle==HCI_CON_HANDLE_INVALID)return;
  handle=HCI_CON_HANDLE_INVALID;valueHandle=0;busy=false;validating=false;
  gattRetryPending=false;gattRetries=0;mustDisarm.store(true);
  set(State::IDLE);nextScan=millis()+1500u;Serial.println("CAR2,DISCONNECTED,PLAYER2_DISARMED");
 }
}
static void tick(btstack_timer_source_t*){
 uint32_t now=millis();State phase=state.load();
 if(phase==State::DISCOVERING && gattRetryPending &&
     (int32_t)(now-gattRetryAfter)>=0 && handle!=HCI_CON_HANDLE_INVALID){
    uint8_t err=gatt_client_discover_characteristics_for_handle_range_by_uuid16(
        &gattCallback,handle,1,0xffff,0xfff1);
    Serial.printf("CAR2_GATT,RETRY=%u,START_STATUS=%u\n",
        unsigned(gattRetries),unsigned(err));
    if(!err){gattRetryPending=false;valueHandle=0;phaseSince=now;}
    else {gattRetryAfter=now+200u;}
 }
 if(phase==State::READY && uint32_t(now-lastPublished.load())>550u){
  if(!busy)write(neutral);mustDisarm.store(true);
 }
 if((phase==State::CONNECTING && uint32_t(now-phaseSince)>4000u) ||
    (phase==State::DISCOVERING && uint32_t(now-phaseSince)>3000u)){
  if(phase==State::CONNECTING)gap_connect_cancel();fail(0xee);
 }
 if(enabled.load() && ferrarible::current()==ferrarible::State::READY){
  phase=state.load();
  if((phase==State::IDLE || phase==State::FAILED) && handle==HCI_CON_HANDLE_INVALID && (int32_t)(now-nextScan)>=0){
   fleet.beginScan();uni_bt_le_scan_start();set(State::SCANNING);Serial.println("CAR2,SCANNING,EXCLUDES_PLAYER1_CAR");
  }else if(phase==State::SCANNING && uint32_t(now-phaseSince)>=2200u){
   Serial.printf("CAR2_SCAN,SEEN=%d,CAR1_EXCLUDED=1,OWNER_ENABLED=%d\n",
       int(fleet.count),int(enabled.load()));
   for(int i=0;i<fleet.count;i++)
     Serial.printf("CAR2_SCAN,CANDIDATE=%d,NAME=%s,SEEN=%d,REJECTED=%d\n",
       i+1,fleet.cars[i].name,int(fleet.cars[i].seen),int(fleet.cars[i].rejected));
   for(int i=0;i<fleet.count;i++)if(memcmp(fleet.cars[i].address,ferrarible::kTarget,6)==0)fleet.cars[i].rejected=true;
   int chosen=-1;
   if(reserved){int i=fleet.find(target);if(i>=0 && fleet.cars[i].seen && !fleet.cars[i].rejected)chosen=i;}
   else chosen=fleet.choose();
   if(chosen>=0){
    memcpy(target,fleet.cars[chosen].address,6);reserved=true;
    uni_bt_le_scan_stop();uint8_t err=gap_connect(target,bd_addr_type_t(fleet.cars[chosen].type));
    lastError.store(err);mustDisarm.store(true);set(err?State::FAILED:State::CONNECTING);
    if(err)nextScan=now+2000u;
    Serial.printf("CAR2,CONNECTING,NAME=%s,STATUS=%u\n",fleet.cars[chosen].name,unsigned(err));
   }else{set(State::IDLE);nextScan=now+1500u;Serial.println("CAR2,WAITING_FOR_SECOND_CAR");}
  }
 }
 btstack_run_loop_set_timer(&timer,50);btstack_run_loop_add_timer(&timer);
}
static void runSend(void*){
 if(ready()){
  uint8_t err=write(pending);lastError.store(err);
 }
 jobPending.store(false,std::memory_order_release);
}
static bool sendPacket(const uint8_t* bytes){
 if(!ready())return false;
 bool expected=false;if(!jobPending.compare_exchange_strong(expected,true))return false;
 memcpy(pending,bytes,8);lastPublished.store(millis());
 sendJob.callback=&runSend;sendJob.context=nullptr;btstack_run_loop_execute_on_main_thread(&sendJob);return true;
}
static void registerLink(void*){
 prefs.begin("shellrc2",false);prefs.getBytes("preferred",fleet.preferred,6);
 events.callback=&onHci;hci_add_event_handler(&events);
 btstack_run_loop_set_timer_handler(&timer,&tick);btstack_run_loop_set_timer(&timer,50);btstack_run_loop_add_timer(&timer);
 Serial.println("DUAL,READY,PLAYER1=XBOX360_USB,PLAYER2=XBOX_BT");
}
static void init(){registerJob.callback=&registerLink;registerJob.context=nullptr;btstack_run_loop_execute_on_main_thread(&registerJob);}
}
