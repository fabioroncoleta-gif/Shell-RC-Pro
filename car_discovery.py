"""Descoberta segura de miniaturas Shell BLE (sem transmitir comandos)."""
from __future__ import annotations

import json
from pathlib import Path

SHELL_PREFIXES = ("SL-", "SHELL")


def address_of(device):
    return str(getattr(device, "address", "") or "").upper()


def is_shell_candidate(device, known_addresses=()):
    name = str(getattr(device, "name", "") or "").strip().upper()
    known = {str(a).upper() for a in known_addresses}
    return bool(name.startswith(SHELL_PREFIXES) or address_of(device) in known)


def filter_shell_devices(devices, known_addresses=()):
    """Descarta BLE aleatorios e elimina anuncios duplicados."""
    found = {}
    for d in devices:
        address = address_of(d)
        if address and is_shell_candidate(d, known_addresses):
            found[address] = d
    return sorted(found.values(), key=lambda d: ((d.name or "").upper(), address_of(d)))


def choose_device(devices, manual_address=None, last_address=None, legacy_address=None):
    """Nao adivinha qual carrinho conectar quando ha mais de um candidato."""
    by_address = {address_of(d): d for d in devices}
    if manual_address:
        return by_address.get(manual_address.upper())
    for address in (last_address, legacy_address):
        if address and address.upper() in by_address:
            return by_address[address.upper()]
    return devices[0] if len(devices) == 1 else None


class FleetRegistry:
    def __init__(self, path):
        self.path = Path(path)
        self.cars = {}
        self.last_address = None
        if self.path.exists():
            try:
                data = json.loads(self.path.read_text(encoding="utf-8"))
                self.last_address = data.get("last_address")
                self.cars = dict(data.get("cars") or {})
            except (OSError, ValueError, TypeError):
                pass

    def remember_connected(self, device):
        address = address_of(device)
        if not address:
            return
        self.cars[address] = str(getattr(device, "name", None) or address)
        self.last_address = address
        data = {"last_address": self.last_address, "cars": self.cars}
        tmp = self.path.with_suffix(".tmp")
        tmp.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        tmp.replace(self.path)
