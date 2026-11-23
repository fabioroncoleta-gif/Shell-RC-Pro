#pragma once
// Shell RC Pro behavior port. Standalone C++: does NOT transmit to any car.
#include <stdint.h>
#include <cmath>
#include <algorithm>
#include "input_core.h"

namespace shellrc {
static inline float limit(float n,float lo,float hi) {
    return std::max(lo,std::min(hi,n));
}
struct CarPacket {
    uint8_t data[8] = {1,0,0,0,0,0,0,0};
    float duty=0.f;
    bool armed=false;
    bool handbrake=false;
    bool turbo=false;
    bool direct=true;
    bool reverseFull=true;
};

class DriveModel {
 public:
    void resetMotion(uint32_t now) {
        (void)now;
        // Releasing the trigger is an immediate STOP and must not add a
        // 420ms dead period to the next command in the same direction.
        // Direction reversals alone set a short protective neutral window.
        neutralUntil_=0;
        duty_=0;rate_=0;filtered_=0;
        direction_=0;boostActive_=false;
    }
    void resetAll(uint32_t now) {
        resetMotion(now);
        prevButtons_=0; brakeHeld_=false; brakeHeading_=0;
        waitNeutral_=false;
        turbo_=false;lights_=0;
        lastUpdate_=now;phaseLight_=0;
    }
    CarPacket tick(const RoutedState& r,bool carReady,uint32_t now) {
        CarPacket o;
        float dt=(lastUpdate_==0)?0.05f:limit(float(uint32_t(now-lastUpdate_))*.001f,.001f,.12f);
        lastUpdate_=now;
        const uint8_t mask=uint8_t(r.buttons);
        auto newPress=[&](unsigned i){return (mask&(1u<<i)) && !(prevButtons_&(1u<<i));};
        // These modes can be selected even while disarmed.
        if(r.fresh) {
            if(newPress(0)) changeLights(1,now); // A steady
            if(newPress(2)) changeLights(2,now); // X fade
            if(newPress(3)) changeLights(3,now); // Y strobe
            // RB has no motor mode: forward and reverse are always continuous.
            if(newPress(1)) turbo_=!turbo_;      // B turbo toggle
        }
        prevButtons_=r.fresh?mask:0;
        o.direct=true;
        o.turbo=turbo_;
        o.reverseFull=true;
        if(!r.fresh||!r.armed||!carReady) {
            resetMotion(now);
            brakeHeld_=false;
            waitNeutral_=false;
            return o;
        }
        o.armed=true;
        const float rt=float(r.raw_rt)/1023.f;
        const float lt=float(r.raw_lt)/1023.f;
        float steer=float(r.raw_steer)/512.f;
        if(!r.brake && brakeHeld_) {
            brakeHeld_=false;
            int want=(rt>.085f&&lt<=.085f)?1:
                     (lt>.085f&&rt<=.085f)?-1:0;
            if(brakeHeading_ && want && want!=brakeHeading_) {
                waitNeutral_=true;
                resetMotion(now);
            }
        }
        if(r.brake && !brakeHeld_) {
            brakeHeld_=true;
            lockedSteer_=steer;
            brakeHeading_=direction_;
        }
        if(brakeHeld_) steer=lockedSteer_;
        o.data[3]=steer < -.24f ? 1 : 0;
        o.data[4]=steer > .24f ? 1 : 0;
        o.data[5]=getLight(now)?1:0;
        if(r.brake) {
            o.handbrake=true;
            return o;  // Keeps direction and ramp for immediate resume
        }
        if(waitNeutral_) {
            if(lt<.07f && rt<.07f) waitNeutral_=false;
            else {resetMotion(now); return o;}
        }
        int want=(rt>.085f&&lt<=.085f)?1:
                 (lt>.085f&&rt<=.085f)?-1:0;
        if(want==0) { resetMotion(now); return o; }
        if(direction_ && want!=direction_) {
            resetMotion(now);
            neutralUntil_=now+160u; // safe transition between forward and reverse
            return o;
        }
        if((int32_t)(now-neutralUntil_)<0) return o;
        if(direction_==0) {
            direction_=want;rate_=0;filtered_=0;
        }
        const float trigger=(want>0)?rt:lt;
        const float alpha=1.f-expf(-dt/.09f);
        filtered_+=alpha*(trigger-filtered_);
        const float normal=limit((filtered_-.085f)/.915f,0.f,1.f);
        const float minDuty=(want<0)?.24f:.20f;
        const float exp=(want<0)?1.40f:1.25f;
        float target=(normal>0)?minDuty+(1.f-minDuty)*powf(normal,exp):0.f;
        if(trigger>=.96f) target=1.f;
        float up=(want<0)?.78f:1.1f;
        float down=(want<0)?1.8f:2.6f;
        float desired=limit((target-duty_)*6.f,-down,up);
        rate_+=limit(desired-rate_,-8.f*dt,8.f*dt);
        float next=duty_+rate_*dt;
        if((rate_>=0.f&&next>=target)||(rate_<0.f&&next<=target)) {
            next=target;rate_=0;
        }
        duty_=limit(next,0.f,1.f);
        if(target>=.999f&&duty_>=.97f) {duty_=1.f;rate_=0;}
        // FFF1 motor commands are binary. No pulse-density modulation,
        // no periodic zero packets and no hidden reverse speed limiter.
        const bool active=(direction_!=0 && trigger>=.13f);
        o.duty=active?1.f:0.f;
        if(direction_==1 && turbo_ && duty_>=.80f) boostActive_=true;
        if(duty_<.55f || direction_<0 || !turbo_) boostActive_=false;
        if(active&&direction_==1)o.data[1]=1;
        else if(active&&direction_==-1)o.data[2]=1;
        o.data[6]=boostActive_?1:0;
        o.reverseFull=true;
        return o;
    }
 private:
    void changeLights(int mode,uint32_t now) {
        lights_=(lights_==mode)?0:mode;
        lightSince_=now;phaseLight_=0;
    }
    bool getLight(uint32_t now) {
        if(lights_==0)return false;
        if(lights_==1)return true;
        if(lights_==3)return (uint32_t(now-lightSince_)%333u)<133u;
        // PC fade uses sigma-delta over 3.2s sinusoidal brightness.
        const float f=float(uint32_t(now-lightSince_)%3200u)/3200.f;
        float intensity=.5f-.5f*cosf(6.283185307f*f);
        phaseLight_+=intensity;
        if(phaseLight_>=1.f) {phaseLight_-=1.f;return true;}
        return false;
    }
    uint32_t lastUpdate_=0,neutralUntil_=0,lightSince_=0;
    float duty_=0,rate_=0,filtered_=0,phaseLight_=0;
    float lockedSteer_=0;
    int direction_=0,brakeHeading_=0;
    uint8_t prevButtons_=0,lights_=0;
    bool turbo_=false;
    bool brakeHeld_=false,waitNeutral_=false,boostActive_=false;
};
}
