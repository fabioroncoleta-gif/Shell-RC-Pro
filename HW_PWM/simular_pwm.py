#!/usr/bin/env python3
"""Gera uma sequencia de comandos PWM 100% OFFLINE para prova de logica."""
import csv
from pathlib import Path
from pwm_motor_core import PwmMotorController
BASE=Path(__file__).resolve().parent
m=PwmMotorController()
rows=[]
for idx in range(600):
    now=idx*.01
    if now<2: throttle,brake=.82,False
    elif now<2.6: throttle,brake=.82,True
    elif now<3.5: throttle,brake=.82,False
    elif now<4: throttle,brake=0.,False
    else:throttle,brake=-.68,False
    s=m.tick(throttle,brake,True,.01,now)
    rows.append([f"{now:.2f}",f"{throttle:.2f}",int(brake),s.mode,
                 f"{s.duty:.4f}",s.in1,s.in2])
with (BASE/"simulacao_pwm_20kHz.csv").open("w",newline="") as f:
    w=csv.writer(f)
    w.writerow(["tempo_s","gatilho_assinado","LB","modo_ponte_H","duty_PWMe","IN1","IN2"])
    w.writerows(rows)
spans={}
for row in rows:spans[row[3]]=spans.get(row[3],0)+1
report=[
"TESTE OFFLINE - PROPOSTA PWM ELETRICO (NAO TESTADO NO CARRINHO)",
"Simulacao gera apenas instrucoes para uma ponte H hipotetica do tipo DRV8833.",
"Frequencia PWM de projeto: 20 kHz (somente referencia; a placa ainda nao foi identificada).",
"Contrastando: programa BLE antigo usa comandos de motor a ~20 Hz, com ON/OFF bruto.",
"20 kHz / 20 Hz = 1000 ciclos PWM por cada intervalo BLE (nao e medicao real).",
"CENARIO: 0-2 s RT 82%; 2-2.6 s LB; 2.6-3.5 s RT 82%; 3.5-4 s neutro; 4-6 s re 68%.",
"Tempo de simulação 6 s, passo 10 ms.",
"Estados na simulação: "+", ".join(f"{k}={n*10}ms" for k,n in spans.items()),
"DADOS: simulacao_pwm_20kHz.csv",
"ATENCAO: duty e comando virtual, nao RPM medido nem potencia eletrica real.",
"SIMULACAO APROVADA somente para sequenciamento logico, SEM SEGURANCA ELETRICA VALIDADA.",
]
out="\n".join(report)+"\n"
(BASE/"RESULTADO_BANCADA.txt").write_text(out)
print(out)
