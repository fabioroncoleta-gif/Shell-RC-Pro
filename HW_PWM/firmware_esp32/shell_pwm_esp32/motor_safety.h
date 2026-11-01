#pragma once
#include <stdint.h>
#include <math.h>
// PWM: somente controle de velocidade com coast, SEM freio de curto ou reversao violenta
struct Demand {int dir=0; float duty=0; bool armed=false,sport=false,turbo=false;};
struct Output {int dir;float duty;};
class MotorSafety {
 public:
 void accept(Demand cmd,uint32_t ms) {cmd_=cmd;rx_=ms;seen_=true;}
 Output tick(uint32_t ms,bool connected) {
   float dt=started_ ? (ms-tick_)*.001f : .001f;
   started_=true;tick_=ms;if(dt>.05f)dt=.05f;
   bool valid=connected&&seen_&&(ms-rx_<=250)&&cmd_.armed&&
              (cmd_.dir==1||cmd_.dir==-1)&&cmd_.duty>0&&cmd_.duty<=1;
   if(!valid) {
     if(dir_!=0||power_>0) { last_dir_=dir_;lock_until_=ms+450; }
     dir_=0;power_=0;return {0,0};
   }
   if(cmd_.dir!=dir_) {
     if(dir_!=0) {last_dir_=dir_;lock_until_=ms+450;dir_=0;power_=0;return {0,0};}
     if(ms<lock_until_ && cmd_.dir!=last_dir_)return {0,0};
     dir_=cmd_.dir;power_=0;
   }
   float cap=(cmd_.sport||cmd_.turbo)?1.f:.65f;
   float target=cmd_.duty*cap;
   float delta=target-power_;
   float rise=(dir_==1?1.f:.8f)*dt,fall=3.f*dt;
   if(delta>rise)delta=rise;
   if(delta< -fall)delta= -fall;
   power_+=delta;
   if(power_<0)power_=0;
   if(power_>1)power_=1;
   if(power_>0)last_dir_=dir_;
   return {dir_,power_};
 }
 private:
 Demand cmd_{};bool started_=false,seen_=false;
 uint32_t rx_=0,tick_=0,lock_until_=0;
 int dir_=0,last_dir_=0;float power_=0;
};
