#!/usr/bin/env python3
"""Experimento OFFLINE: estimativa de transicoes eletricas de frente/re.
Nao conecta Bluetooth e nunca emite comandos para o carrinho.
Nao modela corrente/torque reais: constantes da placa e motor desconhecidas.
"""
from pathlib import Path
import json
cfg=json.loads(Path(__file__).with_name("config.json").read_text())
interval_ms=float(cfg["interval"])*1000.0
lines=[
 "SIMULACAO ISOLADA - ALTERNANCIA FRENTE/RE - NAO TRANSMITIDA",
 "O protocolo so aceita frente/re/zero (1 bit por sentido).",
 "Nao conhecemos corrente, velocidade real nem capacidade da ponte H.",
 f"Intervalo atual de envio BLE: {interval_ms:.0f} ms (nominal).",
 "Periodo de reversao | Inversoes teoricas/s | Ciclos BLE por reversao",
]
for ms in [1,5,10,20,50,100]:
    reversals=1000//ms-1
    lines.append(f"{ms:>16} ms | {reversals:>20} | {ms/interval_ms:>8.2f}")
lines+=["",
 "INFERENCIA: reversao a cada 1/5/10 ms seria mais rapida que o ciclo BLE.",
 "Mesmo se possivel, comandar sentido oposto durante o giro pode elevar corrente",
 "e esforco mecanico, sem garantia de freio ou bloqueio das rodas.",
 "RESULTADO: INVALIDO COMO FREIO SEGURO. SOMENTE SIMULACAO.",
 "LB REAL: corta frente/re e preserva bit de direcao; sem comando de sentido oposto."
]
output="\n".join(lines)+"\n"
Path(__file__).with_name("RELATORIO_FREIO_SIMULACAO.txt").write_text(output)
print(output)
