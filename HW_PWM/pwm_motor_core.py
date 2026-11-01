"""PROTOTIPO SOMENTE OFFLINE de controle PWM para ponte H tipo DRV8833.
NAO instala GPIO, nao conecta Bluetooth, nao aciona motores reais.
"""
from dataclasses import dataclass
import math

@dataclass(frozen=True)
class BridgeCommand:
    mode: str           # COAST, FORWARD_PWM, REVERSE_PWM, BRAKE
    duty: float         # ciclo ativo 0..1 do PWM real
    in1: str            # 0 / 1 / PWM
    in2: str            # 0 / 1 / PWM

COAST = BridgeCommand("COAST", 0., "0", "0")
BRAKE = BridgeCommand("BRAKE", 0., "1", "1")  # Somente ponte H confirmada e devidamente dimensionada.

class PwmMotorController:
    def __init__(self, forward_s=0.85, reverse_s=1.15, dead_s=0.35, gamma=1.4, deadzone=0.07):
        self.forward_s=forward_s
        self.reverse_s=reverse_s
        self.dead_s=dead_s
        self.gamma=gamma
        self.deadzone=deadzone
        self.current_duty=0.
        self.current_rate=0.
        self.direction=0
        self.last_direction=0
        self.lock_until=0.
        self.braking=False

    def reset(self):
        self.current_duty=0.
        self.current_rate=0.
        self.direction=0
        self.last_direction=0
        self.lock_until=0.
        self.braking=False

    def _target(self, raw):
        magnitude=abs(max(-1.,min(1.,raw)))
        if magnitude<=self.deadzone:return 0.
        x=(magnitude-self.deadzone)/(1.-self.deadzone)
        return x**self.gamma

    def tick(self, throttle, brake, armed, dt_s, timestamp_s):
        """Saida logica independente de hardware. Aplicar GPIO requer placa identificada."""
        dt=max(.001,min(.050,dt_s))
        raw=max(-1.,min(1.,float(throttle)))
        target=self._target(raw)
        requested=0 if target==0 else (1 if raw>0 else -1)

        if not armed:
            self.reset()
            return COAST
        if brake:
            self.braking=True
            # Em uma ponte H adequada, IN1=IN2=HIGH e frenagem dinamica.
            self.current_duty=0.;self.current_rate=0.
            self.direction=0
            return BRAKE

        if self.braking:
            self.braking=False
            self.lock_until=max(self.lock_until,timestamp_s+.10)

        if requested==0:
            self.current_duty=0.;self.current_rate=0.
            self.direction=0
            return COAST

        if self.direction and requested!=self.direction:
            self.last_direction=self.direction
            self.direction=0
            self.current_duty=0.;self.current_rate=0.
            self.lock_until=max(self.lock_until,timestamp_s+self.dead_s)
            return COAST

        if requested!=self.last_direction and self.last_direction!=0:
            if timestamp_s<self.lock_until:
                self.current_duty=0.;self.current_rate=0.
                return COAST
            self.lock_until=0.

        if timestamp_s<self.lock_until:
            self.current_duty=0.;self.current_rate=0.
            return COAST

        if self.direction==0:
            self.direction=requested
        self.last_direction=self.direction

        duration=self.forward_s if self.direction==1 else self.reverse_s
        slope=1. / max(.1,duration)
        target_rate=min(slope,max(-2.0,(target-self.current_duty)*6.))
        max_delta_rate=8.0*dt
        self.current_rate+=max(-max_delta_rate,min(max_delta_rate,target_rate-self.current_rate))
        self.current_duty=max(0.,min(1.,self.current_duty+self.current_rate*dt))
        if abs(target-self.current_duty)<.01:
            self.current_duty=target;self.current_rate=0.

        if self.direction==1:
            return BridgeCommand("FORWARD_PWM",self.current_duty,"PWM","0")
        return BridgeCommand("REVERSE_PWM",self.current_duty,"0","PWM")
