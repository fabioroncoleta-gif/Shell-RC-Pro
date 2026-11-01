"""Controle de tracao BLE digital: filtragem de gatilho, rampa com limite de jerk,
intertravamento de reversao e sigma-delta em frequencia fixa.
O brinquedo NAO possui velocidade proporcional conhecida: isto e aproximacao.
"""
from dataclasses import dataclass
import math

def clamp(x, a=0., b=1.):
    return max(a,min(b,float(x)))

def curve(t,c,reverse=False):
    dead=float(c.get("trigger_deadzone",.085))
    if t <= dead:
        return 0.
    t=clamp((t-dead)/(1.-dead))
    if t>=float(c.get("full_throttle_at",.96)):
        return clamp(c.get("max_reverse" if reverse else "max_forward",1.))
    expo=float(c.get("reverse_expo",1.30) if reverse else c.get("forward_expo",1.25))
    minimum=float(c.get("reverse_min_duty",.25) if reverse else c.get("forward_min_duty",.20))
    return clamp((minimum+(1.-minimum)*t**expo)*
                 float(c.get("max_reverse" if reverse else "max_forward",1.)))

@dataclass
class MotorState:
    duty:float=0.
    direction:int=0
    rate:float=0.
    input_filtered:float=0.
    neutral_until:float=0.
    phase:float=0.
    is_on:bool=False

class MotorController:
    def __init__(self,cfg):
        self.cfg=cfg
        self.state=MotorState()
        self.reverse_full_enabled=False

    def reset(self, now=0.):
        previous=self.state.direction
        if previous:
            self.state.neutral_until=max(self.state.neutral_until,
                                        now+float(self.cfg.get("reverse_lockout",.42)))
        self.state.duty=0.
        self.state.direction=0
        self.state.rate=0.
        self.state.input_filtered=0.
        self.state.phase=0.
        self.state.is_on=False

    def step(self, rt,lt,dt,now,armed=True):
        s=self.state;c=self.cfg
        if not armed:
            self.reset(now)
            return 0.,0
        dt=clamp(dt,.001,.12)
        dead=float(c.get("trigger_deadzone",.085))
        if rt>dead and lt>dead:
            want=0;t=0.
        elif rt>dead:
            want=1;t=clamp(rt)
        elif lt>dead:
            want=-1;t=clamp(lt)
        else:
            want=0;t=0.
        # Parada prioritaria: nunca conservar o motor ligado apos soltar ambos.
        if want==0:
            self.reset(now)
            return 0.,0
        if s.direction and want!=s.direction:
            self.reset(now)
            return 0.,0
        if now < s.neutral_until:
            return 0.,0
        if s.direction==0:
            s.direction=want
            s.rate=0.
            s.input_filtered=0.
        tau=float(c.get("input_filter_s",.09))
        alpha=1.-math.exp(-dt/max(.015,tau))
        s.input_filtered+=alpha*(t-s.input_filtered)
        target=curve(s.input_filtered,c,reverse=(want==-1))
        if want==-1 and not self.reverse_full_enabled:
            # Limite temporal aproximado via comandos digitais da placa original.
            target*=clamp(c.get("reverse_default_factor",.5))
        if not target:
            s.duty=0.;s.rate=0.
            return 0.,s.direction
        up=float(c.get("reverse_accel_rate",.78) if want==-1 else c.get("forward_accel_rate",1.1))
        down=float(c.get("reverse_decel_rate",1.8) if want==-1 else c.get("forward_decel_rate",2.6))
        jerk=float(c.get("jerk_limit",8.0))
        desired_rate=clamp((target-s.duty)*6.,-down,up)
        s.rate+=clamp(desired_rate-s.rate,-jerk*dt,jerk*dt)
        next_duty=s.duty+s.rate*dt
        if (s.rate>=0 and next_duty>=target) or (s.rate<0 and next_duty<=target):
            next_duty=target;s.rate=0.
        s.duty=clamp(next_duty)
        if target>=.999 and s.duty>=.97:
            s.duty=1.;s.rate=0.
        return s.duty,s.direction

    def packet(self,steering,lights=False,armed=True,drive_mode="pulse",turbo=False,direct_trigger=None):
        s=self.state;c=self.cfg
        if not armed:
            return bytes([1,0,0,0,0,int(lights),0,0])
        if drive_mode=="direct":
            # Teste GitHub shell-rc Direct Drive: sem alternancia periodica.
            # ATENCAO: e liga/desliga, nao PWM real; inicia abruptamente.
            # Protocolo binario: sem pulsos, nao ha como regular potencia.
            # Inicia no inicio do gatilho, em vez de esperar duty subir ate 45%.
            threshold=float(c.get("direct_engage_trigger",.13))
            if direct_trigger is None:
                # Compatibilidade para simulacoes de pacote sem input real.
                active=(s.direction != 0 and s.duty >= float(c.get("direct_min_duty",.45)))
            else:
                active=(s.direction != 0 and float(direct_trigger) >= threshold)
            s.phase=0.
        elif s.duty<.001 or s.direction==0:
            s.phase=0.
            active=False
        elif s.duty>=1.-1e-6:
            s.phase=0.
            active=True
        else:
            # Quantizacao sigma-delta: distribui pulsos ON ao longo dos ticks.
            s.phase+=s.duty
            active=s.phase>=1.
            if active:s.phase-=1.
        s.is_on=active
        left=int(steering < -float(c.get("steer_deadzone",.24)))
        right=int(steering > float(c.get("steer_deadzone",.24)))
        return bytes([1,int(active and s.direction==1),
                      int(active and s.direction==-1),left,right,int(lights),
                      int(bool(turbo) and bool(armed)),0])
