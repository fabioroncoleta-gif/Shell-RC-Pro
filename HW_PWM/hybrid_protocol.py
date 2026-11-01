"""Protocolo BLE privado para placa PWM NOVA; nunca enviar a FFF1 original."""
from __future__ import annotations
from dataclasses import dataclass

MAGIC = 0xC3
VERSION = 1
UUID_SERVICE = "fbdc1234-2026-4880-8000-000000000001"
UUID_WRITE = "fbdc1234-2026-4880-8000-000000000002"
PWM_BLE_NAME = "SHELL-PWM-488"

@dataclass(frozen=True)
class MotorFrame:
    seq: int
    direction: int
    duty_permille: int
    armed: bool
    turbo: bool
    sport: bool

def crc8(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xff if crc & 0x80 else (crc << 1) & 0xff
    return crc

def encode(seq: int, direction: int, duty: float,
           armed: bool, turbo: bool = False, sport: bool = False) -> bytes:
    if direction not in (-1, 0, 1):
        raise ValueError("direcao invalida")
    if not 0 <= seq <= 255:
        raise ValueError("sequencia invalida")
    if not 0.0 <= float(duty) <= 1.0:
        raise ValueError("duty invalido")
    if not armed or duty == 0 or direction == 0:
        direction, duty, armed = 0, 0.0, False
    level = int(round(duty * 1000))
    mode = {0: 0, 1: 1, -1: 2}[direction]
    flags = (1 if armed else 0) | (2 if turbo and armed else 0) | (4 if sport and armed else 0)
    body = bytes((MAGIC, VERSION, seq, mode, level & 255, level >> 8, flags))
    return body + bytes((crc8(body),))

def decode(raw: bytes) -> MotorFrame:
    if len(raw) != 8 or raw[0:2] != bytes((MAGIC, VERSION)) or crc8(raw[:7]) != raw[7]:
        raise ValueError("pacote PWM invalido")
    mode, duty, flags = raw[3], (raw[5] << 8) | raw[4], raw[6]
    if mode not in (0, 1, 2) or duty > 1000 or flags & ~7:
        raise ValueError("comando PWM invalido")
    if not flags & 1 and (mode or duty):
        raise ValueError("motor deve estar neutro quando desarmado")
    return MotorFrame(raw[2], {0: 0, 1: 1, 2: -1}[mode], duty,
                      bool(flags & 1), bool(flags & 2), bool(flags & 4))

def compose_existing_and_pwm(run, seq: int) -> tuple[bytes, bytes]:
    """Preserva farois/direcao no carro original; TODOS os bits de tracao ficam em zero.
    So usar apos ISOLAR ELETRICAMENTE O MOTOR da ponte U6 original.
    """
    original = bytearray(run.command_packet())
    for bit in (1, 2, 6):  # frente/re/turbo original NUNCA ativo no modo hibrido
        original[bit] = 0
    st = run.motor.state
    allowed = bool(run.armed and run.pad is not None and
                   not run.brake_held and not run.brake_release_wait_neutral)
    pwm = encode(seq, st.direction if allowed else 0,
                 st.duty if allowed else 0.0,
                 armed=allowed, turbo=bool(run.turbo),
                 sport=run.drive_mode in ("sport", "direct"))
    return bytes(original), pwm
