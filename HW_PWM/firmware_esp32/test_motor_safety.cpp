#include "shell_pwm_esp32/motor_safety.h"
#include <cassert>
#include <cstdio>
int main(){
 MotorSafety m;Demand f;f.dir=1;f.duty=1;f.armed=true;f.sport=true;
 m.accept(f,10);auto x=m.tick(10,true);assert(x.dir==1&&x.duty<=.01);
 for(unsigned t=20;t<=200;t+=10){m.accept(f,t);x=m.tick(t,true);}
 assert(x.dir==1&&x.duty>.1&&x.duty<.25);
 x=m.tick(470,true);assert(x.dir==0&&x.duty==0);
 m.accept(f,480);x=m.tick(480,true);assert(x.duty<=.02);
 for(unsigned t=490;t<=950;t+=10){m.accept(f,t);x=m.tick(t,true);}
 assert(x.dir==1&&x.duty>0);
 Demand r=f;r.dir=-1;m.accept(r,960);x=m.tick(960,true);
 assert(x.dir==0&&x.duty==0);
 for(unsigned t=970;t<1410;t+=10){m.accept(r,t);x=m.tick(t,true);assert(x.duty==0);}
 m.accept(r,1430);x=m.tick(1430,true);assert(x.dir==-1&&x.duty<.05);
 r.armed=false;m.accept(r,1440);x=m.tick(1440,true);assert(x.dir==0&&x.duty==0);
 puts("PASS: watchdog, rampa, coast, reversao com espera, desarme");
}
