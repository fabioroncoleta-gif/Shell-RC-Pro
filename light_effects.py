"""Efeitos de farol para Shell Racing: comandos BLE digitais ON/OFF.
Sem PWM eletrico no carrinho; 'fade' usa modulacao temporal aproximada.
"""
from math import cos, pi

MODES=("off","steady","strobe","fade")
LABELS={"off":"DESLIGADOS","steady":"LIGADOS","strobe":"STROBO","fade":"PULSANDO SUAVE"}

class LightEffects:
    def __init__(self,cfg):
        self.cfg=cfg
        self.mode="off"
        self.start=0.
        self.accumulator=0.
        self.last_output=False

    def select(self,mode,now):
        if mode not in MODES or mode=="off":
            raise ValueError("Light mode must be steady, strobe or fade")
        # Apertar o mesmo botao novamente desliga o efeito.
        self.mode="off" if self.mode==mode else mode
        self.start=float(now)
        self.accumulator=0.
        self.last_output=False
        return self.mode

    def off(self):
        self.mode="off"
        self.accumulator=0.
        self.last_output=False

    def brightness(self,now):
        elapsed=max(0.,float(now)-self.start)
        if self.mode=="off":
            return 0.
        if self.mode=="steady":
            return 1.
        if self.mode=="strobe":
            hz=max(.5,min(6.,float(self.cfg.get("strobe_hz",3.0))))
            # flash rapido 40% do ciclo, aprox. 3 piscadas por segundo
            return 1. if ((elapsed*hz+1e-7)%1.) < float(self.cfg.get("strobe_on_fraction",.4)) else 0.
        period=max(1.5,float(self.cfg.get("fade_period_s",3.2)))
        # 0 -> 100 -> 0, rampa sinusoidal de intensidade pretendida.
        return .5-.5*cos(2*pi*(elapsed/period))

    def output(self,now):
        intensity=self.brightness(now)
        if intensity<=0.:
            self.last_output=False
        elif intensity>=1.:
            self.last_output=True
        elif self.mode=="strobe":
            self.last_output=bool(intensity)
        else:
            # 1 bit disponivel (0/1). Aproxima media de luz com sigma-delta.
            self.accumulator+=intensity
            self.last_output=(self.accumulator>=1.)
            if self.last_output:
                self.accumulator-=1.
        return self.last_output

    def label(self):
        return LABELS[self.mode]
