#include "../src/dual_rc_runtime.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <stdint.h>
using namespace shellrc;
static GamepadSample sample(bool start, uint16_t rt=0,uint16_t lt=0,int16_t steer=0){
 GamepadSample s;s.connected=true;s.start=start;s.rt=rt;s.lt=lt;s.steer=steer;s.buttons=start?128u:0u;return s;
}
struct Stats {
 unsigned tx1=0,tx2=0,armed1=0,armed2=0,moveForward1=0,moveReverse2=0;
 unsigned crossTo1=0,crossTo2=0,txAfterCar1Lost=0,ignoredReadiness=0;
};
int main(){
 constexpr uint32_t idBT=0x10001u,idUSB=0x20001u;
 constexpr uint64_t carRed=0xAABBCC01u,carYellow=0xAABBCC02u;
 DualRuntime r;Stats stats;
 r.attachController(idBT,Source::BLUETOOTH,0);
 r.carAdvertised(carRed);
 r.attachController(idUSB,Source::XBOX360_USB,20);
 r.carAdvertised(carYellow);
 assert(r.pair.lookup(idBT)->carId==carRed && r.pair.lookup(idUSB)->carId==carYellow);
 r.carGattVerified(carRed,40);
 r.carGattVerified(carYellow,60);
 for(uint32_t ms=80;ms<5000;ms+=20){
   if(ms==2600)r.carLost(carRed,ms);
   if(ms==3000)r.controllerLost(idBT,ms);
   const bool start=(ms==100 || ms==120);
   const bool driving=ms>=900;
   if(ms<3000)r.submit(idBT,sample(start,driving?950:0,0,driving?-350:0),ms);
   r.submit(idUSB,sample(start,0,driving?950:0,driving?+350:0),ms);
   r.tick(ms);
   if(ms<2600 && !r.outputs[0].carReady)++stats.ignoredReadiness;
   for(int i=0;i<2;i++) {
     const auto &v=r.outputs[i];
     const bool send=v.carReady && v.carId!=0;
     if(ms%60==0 && send){
       if(i==0){
          ++stats.tx1;
          if(v.carId!=carRed)++stats.crossTo1;
          if(v.motor.armed)++stats.armed1;
          if(v.motor.data[1])++stats.moveForward1;
          if(ms>=2600)++stats.txAfterCar1Lost;
       }else{
          ++stats.tx2;
          if(v.carId!=carYellow)++stats.crossTo2;
          if(v.motor.armed)++stats.armed2;
          if(v.motor.data[2])++stats.moveReverse2;
       }
     }
   }
   if(ms>=2600 && ms<3000) {
     assert(!r.outputs[0].motor.armed);
     if (!r.outputs[1].motor.armed) {
        std::cerr << "FAIL @ms="<<ms<<" route.armed="<<r.outputs[1].route.armed
                  <<" fresh="<<r.outputs[1].route.fresh<<" carReady="<<r.outputs[1].carReady
                  <<" controller="<<r.outputs[1].controllerId<<std::endl;
        return 2;
     }
   }
 }
 assert(stats.tx1>10&&stats.tx2>10);
 assert(stats.moveForward1>10 && stats.moveReverse2>10);
 assert(!stats.crossTo1&&!stats.crossTo2&&!stats.txAfterCar1Lost);
 assert(stats.armed2>stats.armed1); // car2 keeps running after car1 gone
 std::ofstream f("diagnostics/nexo_dual_preview/virtual_end_to_end_result.json");
 f << "{\n  \"scope\": \"SIMULATED TRANSPORT ONLY; NO RADIO, PCB, OR USB POWER MEASURED\",\n";
 f << "  \"duration_ms\": 5000,\n  \"controller_order\": [\"BT\", \"USB360\"],\n";
 f << "  \"car_order\": [\"red\", \"yellow\"],\n";
 f << "  \"car1_packets\": "<<stats.tx1<<",\n  \"car2_packets\": "<<stats.tx2<<",\n";
 f << "  \"car1_forward_packets\": "<<stats.moveForward1<<",\n";
 f << "  \"car2_reverse_packets\": "<<stats.moveReverse2<<",\n";
 f << "  \"cross_car_packets\": "<<stats.crossTo1+stats.crossTo2<<",\n";
 f << "  \"car1_packets_after_disconnect\": "<<stats.txAfterCar1Lost<<",\n";
 f << "  \"passed\": true\n}\n";
 std::cout<<"PASS E2E 5s simulated: red TX="<<stats.tx1<<" forward="<<stats.moveForward1
          <<" yellow TX="<<stats.tx2<<" reverse="<<stats.moveReverse2
          <<" crosses="<<(stats.crossTo1+stats.crossTo2)
          <<" after disconnect="<<stats.txAfterCar1Lost<<"\n";
}
