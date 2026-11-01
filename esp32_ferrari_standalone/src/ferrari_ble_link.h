#pragma once
/*
 * Ferrari Shell BLE client for the Bluepad32/BTstack ESP32 core.
 * HCI/GATT functions MUST run on BTstack Core0, NEVER Arduino Core1.
 * Only accepts one whitelisted SL-488 Challenge Evo MAC.
 * Stage 1 firmware sends neutral packet only; no motor commands.
 */
#include <Arduino.h>
#include <atomic>
#include "held_input_policy.h"
extern "C" {
#include <btstack.h>
#include <bt/uni_bt_le.h>
#include <bt/uni_bt.h>
#include <ble/le_device_db.h>
#include <uni_hid_device.h>
}
namespace ferrarible {
enum class State : uint8_t { IDLE=0, SCANNING=1, CONNECTING=2,
    DISCOVERING=3, READY=4, FAILED=5 };
enum class Action : uint8_t { REGISTER=1, SCAN=2, SEND_NEUTRAL=3, DISCONNECT=4, SEND_PACKET=5, RECOVER_XBOX_SCAN=6, RECOVER_XBOX_LINK=7 };
static const uint8_t kNeutral[8] = {1,0,0,0,0,0,0,0};
static uint8_t pendingPacket[8] = {1,0,0,0,0,0,0,0};
static uint8_t transferPacket[8] = {1,0,0,0,0,0,0,0};
static std::atomic<uint32_t> dataWrites{0};
static const bd_addr_t kTarget = {0x13,0x05,0xAA,0x04,0x66,0x96};
static std::atomic<uint8_t> state{uint8_t(State::IDLE)};
static std::atomic<uint8_t> lastError{0};
static std::atomic<uint32_t> connections{0}, scans{0}, writeCount{0};
static std::atomic<uint32_t> lastEventMs{0};
static std::atomic<uint32_t> xboxAds{0}, otherAds{0}, scanRecoveries{0};
static std::atomic<uint32_t> lastXboxAdMs{0}, lastAnyAdMs{0};
static std::atomic<uint32_t> xboxAdv0{0}, xboxAdv1{0}, xboxAdv2{0}, xboxAdv3{0};
static std::atomic<uint8_t> xboxAdvLastType{255}, xboxAdvAddrType{255};
static std::atomic<uint16_t> xboxAppearance{0};
static std::atomic<uint32_t> xboxNoAppearance{0};
static std::atomic<uint32_t> xboxBondMatches{0};
static std::atomic<uint8_t> lastLeCompleteStatus{255}, lastBtDisconnectReason{255};
static std::atomic<bool> xboxHciConnected{false}, xboxGamepadReady{false}, xboxAwaitingReport{false};
static std::atomic<uint32_t> xboxHciSince{0}, xboxLinkRecoveries{0}, lastXboxRecoveryMs{0};
static std::atomic<uint16_t> xboxSupervisionMs{0};
static uint8_t xboxSupervisionRetries=0;
static std::atomic<uint32_t> xboxHeartbeatMs{0};
static btstack_timer_source_t xboxHeartbeatTimer = {};
static hci_con_handle_t xboxHandle=HCI_CON_HANDLE_INVALID;
static std::atomic<uint32_t> lastLeCompleteMs{0};
static bool xboxBondChecked=false;
static const bd_addr_t kXbox = {0x28,0xEA,0x0B,0xDD,0x0F,0x8E};
static std::atomic<bool> mustDisarm{true};
static std::atomic<bool> operationPending{false};
static std::atomic<bool> registered{false};
static hci_con_handle_t handle = HCI_CON_HANDLE_INVALID;
static uint16_t writeHandle = 0;
static uint16_t writeProps = 0;
static bool queryIsWrite = false;
static btstack_packet_callback_registration_t packetRegistration = {};
static btstack_context_callback_registration_t requestRegistration = {};
static Action requestedAction = Action::REGISTER;
static uint32_t scanStartedAt = 0;

// A held report is usable only with a confirmed <=400ms radio timeout
// and a live BTstack thread. If either disappears, the router watchdog stops it.
static bool supervisedXboxLive(uint32_t now) {
    return shellrc::canHoldInput(xboxHciConnected.load(),xboxGamepadReady.load(),
        xboxSupervisionMs.load(),uint32_t(now-xboxHeartbeatMs.load()));
}
static uint8_t knownXboxDevice(uni_hid_device_t* d,void*) {
    return !d->parent && d->vendor_id==0x045e && d->product_id==0x0b13 &&
        int(d->controller_type)==32 && d->conn.connected &&
        d->conn.protocol==UNI_BT_CONN_PROTOCOL_BLE;
}
static void xboxHeartbeat(btstack_timer_source_t* timer) {
    xboxHeartbeatMs.store(millis());
    // Resolve the actual Bluepad32 device handle, including private BLE
    // addresses; a fixed public address alone does not identify every reconnect.
    uni_hid_device_t* d=uni_hid_device_get_instance_with_predicate(&knownXboxDevice,nullptr);
    if(d && d->conn.handle!=HCI_CON_HANDLE_INVALID && d->conn.handle!=xboxHandle) {
        xboxHandle=d->conn.handle;
        xboxHciConnected.store(true);
        xboxHciSince.store(millis());
        xboxSupervisionMs.store(0);
        xboxSupervisionRetries=0;
        int err=gap_update_connection_parameters(xboxHandle,12,24,0,40);
        Serial.printf("XBOX_SUPERVISION,RESOLVED_HANDLE=%u,STATUS=%d\n",unsigned(xboxHandle),err);
    }
    btstack_run_loop_set_timer(timer,100);
    btstack_run_loop_add_timer(timer);
}
static void set(State value) {
    state.store(uint8_t(value),std::memory_order_release);
    lastEventMs.store(millis(),std::memory_order_release);
}
static State current() {
    return State(state.load(std::memory_order_acquire));
}
static const char* label(State s) {
    switch(s) {
        case State::IDLE:return "IDLE";
        case State::SCANNING:return "SCANNING";
        case State::CONNECTING:return "CONNECTING";
        case State::DISCOVERING:return "DISCOVERING";
        case State::READY:return "READY";
        default:return "FAILED";
    }
}
static void gattCallback(uint8_t packetType, uint16_t channel,
                         uint8_t *packet, uint16_t size) {
    (void)channel; (void)size;
    if(packetType != HCI_EVENT_PACKET) return;
    const uint8_t eventType=hci_event_packet_get_type(packet);
    if(eventType==GATT_EVENT_CHARACTERISTIC_QUERY_RESULT && !queryIsWrite) {
        if(gatt_event_characteristic_query_result_get_handle(packet)!=handle) return;
        gatt_client_characteristic_t ch = {};
        gatt_event_characteristic_query_result_get_characteristic(packet,&ch);
        if(ch.properties & (ATT_PROPERTY_WRITE | ATT_PROPERTY_WRITE_WITHOUT_RESPONSE)) {
            writeHandle=ch.value_handle;
            writeProps=ch.properties;
        }
    } else if(eventType==GATT_EVENT_QUERY_COMPLETE) {
        if(gatt_event_query_complete_get_handle(packet)!=handle) return;
        uint8_t err=gatt_event_query_complete_get_att_status(packet);
        lastError.store(err);
        if(queryIsWrite) {
            queryIsWrite=false;
            if(err != 0) { set(State::FAILED); mustDisarm.store(true); }
            else { ++writeCount; set(State::READY); }
            return;
        }
        if(err != 0 || !writeHandle) {
            set(State::FAILED);
            mustDisarm.store(true);
            return;
        }
        // First neutral is written only by explicit SEND_NEUTRAL
        // from the BTstack thread, before any future motor-capable firmware.
        set(State::READY);
        mustDisarm.store(true);
    }
}
static void onHci(uint8_t packetType,uint16_t channel,uint8_t* packet,uint16_t size) {
    (void)channel; (void)size;
    if(packetType!=HCI_EVENT_PACKET) return;
    uint8_t eventType=hci_event_packet_get_type(packet);
    if(eventType==GAP_EVENT_ADVERTISING_REPORT) {
        bd_addr_t addr;
        gap_event_advertising_report_get_address(packet,addr);
        lastAnyAdMs.store(millis());
        if(!bd_addr_cmp(addr,kXbox)) {
            ++xboxAds;
            lastXboxAdMs.store(millis());
            const uint8_t advType=gap_event_advertising_report_get_advertising_event_type(packet);
            const uint8_t addrType=gap_event_advertising_report_get_address_type(packet);
            xboxAdvLastType.store(advType);
            xboxAdvAddrType.store(addrType);
            switch(advType) {
                case 0:++xboxAdv0;break;
                case 1:++xboxAdv1;break;
                case 2:++xboxAdv2;break;
                default:++xboxAdv3;break;
            }
            const uint8_t *raw=gap_event_advertising_report_get_data(packet);
            const uint8_t len=gap_event_advertising_report_get_data_length(packet);
            uint16_t appearance=0;
            for(unsigned pos=0;pos<len;) {
                const unsigned fieldSize=raw[pos];
                if(!fieldSize || pos+fieldSize>=len) break;
                if(fieldSize>=3 && raw[pos+1]==0x19) {
                    appearance=uint16_t(raw[pos+2]) |
                        (uint16_t(raw[pos+3])<<8);
                }
                pos+=fieldSize+1u;
            }
            if(appearance==0)++xboxNoAppearance;
            xboxAppearance.store(appearance);
            if(!xboxBondChecked) {
                xboxBondChecked=true;
                uint32_t matches=0;
                for(int i=0;i<le_device_db_max_count();i++) {
                    int storedType;
                    bd_addr_t storedAddr;
                    le_device_db_info(i,&storedType,storedAddr,nullptr);
                    if(storedType==addrType && !bd_addr_cmp(storedAddr,addr))++matches;
                }
                xboxBondMatches.store(matches);
            }
        } else ++otherAds;
        if(current()!=State::SCANNING) return;
        if(!bd_addr_cmp(addr,kTarget)) {
            uint8_t type=gap_event_advertising_report_get_address_type(packet);
            // Do NOT bypass Bluepad32's scanner book-keeping.
            uni_bt_le_scan_stop();
            uint8_t status=gap_connect(addr,bd_addr_type_t(type));
            lastError.store(status);
            if(status==0) set(State::CONNECTING);
            else { set(State::FAILED); mustDisarm.store(true); }
        }
    } else if(eventType==HCI_EVENT_LE_META) {
        if(hci_event_le_meta_get_subevent_code(packet)==HCI_SUBEVENT_LE_CONNECTION_COMPLETE) {
            lastLeCompleteStatus.store(hci_subevent_le_connection_complete_get_status(packet));
            lastLeCompleteMs.store(millis());
            bd_addr_t xboxPeer;
            hci_subevent_le_connection_complete_get_peer_address(packet,xboxPeer);
            if(!bd_addr_cmp(xboxPeer,kXbox)) {
                uint8_t status=hci_subevent_le_connection_complete_get_status(packet);
                if(!status) {
                    xboxHandle=hci_subevent_le_connection_complete_get_connection_handle(packet);
                    xboxSupervisionMs.store(hci_subevent_le_connection_complete_get_supervision_timeout(packet)*10u);
                    xboxSupervisionRetries=0;
                    xboxHciConnected.store(true);
                    int updateErr=gap_update_connection_parameters(xboxHandle,12,24,0,40);
                    Serial.printf("XBOX_SUPERVISION,REQUEST_MS=400,STATUS=%d\n",updateErr);
                    xboxHciSince.store(millis());
                    xboxGamepadReady.store(false);
                }
            }
            if(current()!=State::CONNECTING) return;
            bd_addr_t peer;
            hci_subevent_le_connection_complete_get_peer_address(packet,peer);
            if(bd_addr_cmp(peer,kTarget)) return;
            uint8_t err=hci_subevent_le_connection_complete_get_status(packet);
            lastError.store(err);
            if(err) { set(State::FAILED); mustDisarm.store(true); return; }
            handle=hci_subevent_le_connection_complete_get_connection_handle(packet);
            writeHandle=0;writeProps=0;queryIsWrite=false;
            ++connections;
            set(State::DISCOVERING);
            err=gatt_client_discover_characteristics_for_handle_range_by_uuid16(
                    &gattCallback,handle,1,0xffff,0xfff1);
            if(err) {lastError.store(err);set(State::FAILED);mustDisarm.store(true);}
        }
        else if(hci_event_le_meta_get_subevent_code(packet)==HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE &&
                hci_subevent_le_connection_update_complete_get_connection_handle(packet)==xboxHandle) {
            uint8_t status=hci_subevent_le_connection_update_complete_get_status(packet);
            if(status==0) xboxSupervisionMs.store(
                hci_subevent_le_connection_update_complete_get_supervision_timeout(packet)*10u);
            Serial.printf("XBOX_SUPERVISION,STATUS=%u,ACTUAL_MS=%u\n",
                unsigned(status),unsigned(xboxSupervisionMs.load()));
            // Xbox requests its own 3000ms profile after HID setup. Reapply
            // only after that completed update, with a strict retry bound.
            if(status==0 && xboxSupervisionMs.load()>400 && xboxSupervisionRetries<3) {
                ++xboxSupervisionRetries;
                int err=gap_update_connection_parameters(xboxHandle,12,24,0,40);
                Serial.printf("XBOX_SUPERVISION,REAPPLY=%u,STATUS=%d\n",unsigned(xboxSupervisionRetries),err);
            }
        }
    } else if(eventType==HCI_EVENT_DISCONNECTION_COMPLETE) {
        lastBtDisconnectReason.store(hci_event_disconnection_complete_get_reason(packet));
        const uint16_t lost=hci_event_disconnection_complete_get_connection_handle(packet);
        if(lost==xboxHandle && xboxHandle!=HCI_CON_HANDLE_INVALID) {
            xboxHandle=HCI_CON_HANDLE_INVALID;
            xboxHciConnected.store(false);
            xboxSupervisionMs.store(0);
            xboxGamepadReady.store(false);
        }
        if(lost!=handle || handle==HCI_CON_HANDLE_INVALID) return;
        handle=HCI_CON_HANDLE_INVALID;writeHandle=0;writeProps=0;
        queryIsWrite=false;
        mustDisarm.store(true);
        set(State::IDLE);
    }
}
static void runRequest(void* context) {
    (void)context;
    switch (requestedAction) {
      case Action::REGISTER:
        if(!registered.exchange(true)) {
            packetRegistration.callback=&onHci;
            hci_add_event_handler(&packetRegistration);
            btstack_run_loop_set_timer_handler(&xboxHeartbeatTimer,&xboxHeartbeat);
            btstack_run_loop_set_timer(&xboxHeartbeatTimer,100);
            btstack_run_loop_add_timer(&xboxHeartbeatTimer);
        }
        break;
      case Action::SCAN:
        if(registered.load() && (current()==State::IDLE ||
                                      current()==State::FAILED)) {
            // Bluepad32 owns BLE scan params and active scanner state.
            // Shared scan delivers Ferrari GAP events to our handler.
            uni_bt_le_scan_start();
            scanStartedAt=millis();
            ++scans;
            set(State::SCANNING);
        }
        break;
      case Action::RECOVER_XBOX_SCAN:
        if(current()!=State::READY && current()!=State::CONNECTING &&
           current()!=State::DISCOVERING && !xboxHciConnected.load()) {
            uni_bt_le_scan_stop();
            uni_bt_enable_new_connections_unsafe(true);
            uni_bt_le_scan_start();
            ++scanRecoveries;
        }
        break;
      case Action::RECOVER_XBOX_LINK:
        if(xboxHciConnected.load() && !xboxGamepadReady.load() && !xboxAwaitingReport.load() &&
           xboxHandle!=HCI_CON_HANDLE_INVALID &&
           uint32_t(millis()-xboxHciSince.load())>10000u &&
           uint32_t(millis()-lastXboxRecoveryMs.load())>14000u) {
            // Bluepad32 sometimes gets stuck after HCI LE successful connect
            // but before HIDS gamepad ready. Recover ONLY that known Xbox handle.
            lastXboxRecoveryMs.store(millis());
            uint8_t err=gap_disconnect(xboxHandle);
            if(err==0)++xboxLinkRecoveries;
        }
        break;
      case Action::SEND_NEUTRAL:
      case Action::SEND_PACKET:
        if(current()==State::READY && writeHandle && handle!=HCI_CON_HANDLE_INVALID) {
            uint8_t err;
            // GATT with response does not copy: transferPacket stays stable
            // until GATT_EVENT_QUERY_COMPLETE on Core0.
            memcpy(transferPacket,requestedAction==Action::SEND_PACKET ?
                   pendingPacket : kNeutral,8);
            if(writeProps & ATT_PROPERTY_WRITE_WITHOUT_RESPONSE) {
                err=gatt_client_write_value_of_characteristic_without_response(
                    handle,writeHandle,sizeof(transferPacket),transferPacket);
                if(err==0) {
                    ++writeCount;
                    if(requestedAction==Action::SEND_PACKET) ++dataWrites;
                }
            } else {
                queryIsWrite=true;
                err=gatt_client_write_value_of_characteristic(
                    &gattCallback,handle,writeHandle,sizeof(transferPacket),transferPacket);
                if(err==0) set(State::DISCOVERING);
                else queryIsWrite=false;
            }
            lastError.store(err);
        }
        break;
      case Action::DISCONNECT:
        if(handle!=HCI_CON_HANDLE_INVALID) {
            gap_disconnect(handle);
            mustDisarm.store(true);
        } else if(current()==State::CONNECTING) gap_connect_cancel();
        else if(current()==State::SCANNING) uni_bt_le_scan_stop();
        set(State::IDLE);
        break;
    }
    operationPending.store(false,std::memory_order_release);
}
static bool request(Action action) {
    bool expected=false;
    if(!operationPending.compare_exchange_strong(expected,true)) return false;
    requestedAction=action;
    requestRegistration.callback=&runRequest;
    requestRegistration.context=nullptr;
    btstack_run_loop_execute_on_main_thread(&requestRegistration);
    return true;
}
static bool sendPacket(const uint8_t* bytes) {
    bool expected=false;
    if(!operationPending.compare_exchange_strong(expected,true)) return false;
    memcpy(pendingPacket,bytes,8);
    requestedAction=Action::SEND_PACKET;
    requestRegistration.callback=&runRequest;
    requestRegistration.context=nullptr;
    btstack_run_loop_execute_on_main_thread(&requestRegistration);
    return true;
}
static void init() { request(Action::REGISTER); }
} // namespace ferrarible
