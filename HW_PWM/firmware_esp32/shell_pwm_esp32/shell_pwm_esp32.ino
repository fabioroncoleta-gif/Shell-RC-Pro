/* Shell RC PRO - ESP32/Arduino 3.x BLE + PWM (PROTOTIPO).
  O motor traseiro deve estar ISOLADO da ponte original U6.
  OS GPIOs 25/26 SAO EXEMPLOS: nao conectar nada sem projeto eletrico medido.
  O firmware permanece NAO INSTALADO em hardware.
*/
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <esp_arduino_version.h>
#include "motor_safety.h"
#if ESP_ARDUINO_VERSION_MAJOR < 3
#error Requer ESP32 Arduino Core 3.x com ledcAttach
#endif
constexpr int IN1=25, IN2=26, PWM_HZ=20000, PWM_BITS=10;
constexpr int PWM_MAX=(1<<PWM_BITS)-1;
constexpr char SERVICE[]="fbdc1234-2026-4880-8000-000000000001";
constexpr char CHAR_WRITE[]="fbdc1234-2026-4880-8000-000000000002";
MotorSafety motor;
portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
bool ble_link=false, fresh=false, got_seq=false;
uint8_t last_seq=0;
Demand pending{};
uint32_t pending_at=0;
uint8_t checksum(const uint8_t *p,size_t n) {
  uint8_t c=0;
  for(size_t i=0;i<n;i++){
    c^=p[i];
    for(int j=0;j<8;j++)c=(c&128)?uint8_t((c<<1)^7):uint8_t(c<<1);
  }
  return c;
}
void coast(){
  ledcWrite(IN1,0);
  ledcWrite(IN2,0);
}
void output_pwm(Output out){
  if(!out.dir||out.duty<=0){coast();return;}
  int duty=(int)roundf(out.duty*PWM_MAX);
  // Nunca aplicar comando de freio dinamico (dois pinos altos).
  if(out.dir>0){ledcWrite(IN2,0);ledcWrite(IN1,duty);}
  else{ledcWrite(IN1,0);ledcWrite(IN2,duty);}
}
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    portENTER_CRITICAL(&mux);
    ble_link=true;fresh=false;got_seq=false;
    portEXIT_CRITICAL(&mux);
  }
  void onDisconnect(BLEServer *s) override {
    portENTER_CRITICAL(&mux);
    ble_link=false;fresh=false;got_seq=false;
    portEXIT_CRITICAL(&mux);
    coast();
    s->getAdvertising()->start();
  }
};
class WriteCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* ch) override {
    String raw=ch->getValue();
    if(raw.length()!=8)return;
    const uint8_t *p=(const uint8_t*)raw.c_str();
    if(p[0]!=0xC3||p[1]!=1||checksum(p,7)!=p[7])return;
    int mode=p[3], duty=p[4] | (p[5]<<8), flags=p[6];
    if(mode>2||duty>1000||(flags&~7))return;
    if(!(flags&1) && (mode||duty))return;
    uint8_t seq=p[2];
    portENTER_CRITICAL(&mux);
    uint8_t diff=(uint8_t)(seq-last_seq);
    if(ble_link&&(!got_seq||(diff>0&&diff<128))){
      last_seq=seq;got_seq=true;
      pending={mode==1?1:mode==2?-1:0,duty/1000.f,
               bool(flags&1),bool(flags&4),bool(flags&2)};
      pending_at=millis();fresh=true;
    }
    portEXIT_CRITICAL(&mux);
  }
};
void setup(){
  pinMode(IN1,OUTPUT);pinMode(IN2,OUTPUT);
  digitalWrite(IN1,LOW);digitalWrite(IN2,LOW);
  bool ok1=ledcAttach(IN1,PWM_HZ,PWM_BITS);
  bool ok2=ledcAttach(IN2,PWM_HZ,PWM_BITS);
  if(!ok1||!ok2){while(true){digitalWrite(IN1,LOW);digitalWrite(IN2,LOW);delay(10);}}
  coast();
  BLEDevice::init("SHELL-PWM-488");
  BLEServer *server=BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  BLEService *service=server->createService(SERVICE);
  BLECharacteristic *write=service->createCharacteristic(
    CHAR_WRITE,BLECharacteristic::PROPERTY_WRITE|BLECharacteristic::PROPERTY_WRITE_NR);
  write->setCallbacks(new WriteCallbacks());
  service->start();
  server->getAdvertising()->addServiceUUID(SERVICE);
  server->getAdvertising()->start();
}
void loop(){
  bool is_link,has_command;
  Demand cmd;
  uint32_t at;
  portENTER_CRITICAL(&mux);
  is_link=ble_link;has_command=fresh;cmd=pending;at=pending_at;fresh=false;
  portEXIT_CRITICAL(&mux);
  if(is_link&&has_command)motor.accept(cmd,at);
  output_pwm(motor.tick(millis(),is_link));
  delay(5);
}
