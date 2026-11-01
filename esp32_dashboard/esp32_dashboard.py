#!/usr/bin/env python3
"""Shell RC Pro — painel local somente leitura do ESP32 Dual Xbox + Ferrari.

Backend sem frameworks: HTTP em 127.0.0.1, leitura serial com reconexão,
interpretação de telemetria e histórico limitado de eventos.
NENHUMA escrita serial nem comando aos motores ou à placa.
"""
from __future__ import annotations

from collections import deque
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import subprocess
import threading
import time
from urllib.parse import urlsplit, parse_qs

ROOT = Path(__file__).resolve().parent
PORT = os.environ.get("ESP32_PORT", "/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0")
LOG = ROOT / "firmware_esp32_dual_xbox" / "monitor_usb.log"
DASHLOG = ROOT / "esp32_dashboard_log.txt"
HTML = ROOT / "esp32_dashboard.html"
HOST = "127.0.0.1"
HTTP_PORT = 8765
BUTTON_NAMES = ["A", "B", "X", "Y", "LB", "RB", "BACK", "START"]


def default_state():
    return dict(
        transport="INICIANDO", source="NONE", mode="AUTO",
        xbox_connected=False, xbox_fresh=False, xbox_armed=False,
        xbox_last_seen=0, xbox_name="", car_state="UNKNOWN", car_last_seen=0,
        car_err=0, car_scans=0, car_connections=0, car_tx=0,
        car_armed=False, car_drive_mode="DIRECT", car_last_write=0,
        steer=0, rt=0, lt=0, buttons=0, input_last_seen=0,
        xbox_ads=0, other_ads=0, scan_restarts=0,
        xbox_adv_type=255,xbox_adv_addr_type=255,xbox_adv_appearance=0,xbox_adv_without_appearance=0,
        xbox_bond_matches=0,xbox_le_status=255,xbox_le_ms=0,xbox_disconnect_reason=255,
        xbox_hci=0,xbox_ready=0,xbox_hci_age=0,xbox_link_recoveries=0,
        host=0, baud=115200, serial_path=PORT,
        start_time=time.time(), last_frame=0, last_line="",
        firmware="", note="", connected_count=0, last_disconnect="",
    )


def extract_kv(text):
    result = {}
    for part in text.split(","):
        if "=" in part:
            key, val = part.split("=", 1)
            result[key] = val
    return result


def int0(value, fallback=0):
    try:
        return int(value)
    except (ValueError, TypeError):
        return fallback


class Monitor:
    def __init__(self):
        self.lock = threading.RLock()
        self.state = default_state()
        self.events = deque(maxlen=1600)
        self.serial_no = 0
        self.last_log_emit = {}
        self._stop = threading.Event()

    def add(self, message, category="info", *, dedup=False):
        now = time.time()
        if dedup and now - self.last_log_emit.get(message, 0) < 2.0:
            return
        self.last_log_emit[message] = now
        self.serial_no += 1
        record = dict(id=self.serial_no, at=datetime.fromtimestamp(now).strftime("%H:%M:%S"),
                      category=category, text=str(message)[:500])
        self.events.append(record)
        # 3 MB cap: keep 1 rotating backup, don't write to NEXO or any app.
        try:
            if DASHLOG.exists() and DASHLOG.stat().st_size > 3_000_000:
                DASHLOG.replace(DASHLOG.with_suffix(".old.txt"))
            with DASHLOG.open("a", encoding="utf-8") as f:
                f.write(f"[{record['at']}] [{category}] {record['text']}\n")
        except OSError:
            pass

    def consume(self, line, *, from_saved_log=False):
        line = line.strip()
        if not line:
            return
        if from_saved_log:
            # Format used by the prior serial_monitor.py: "2026-11-01 HH:MM:SS <line>"
            line = re.sub(r"^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\s+", "", line)
        now = time.time()
        with self.lock:
            s = self.state
            s["last_frame"] = now
            s["last_line"] = line[:230]
            if line.startswith("BOOT,"):
                s["firmware"] = line
                self.add(line, "system")
            elif line.startswith("CONNECTED,"):
                if line.startswith("CONNECTED,XBOX_BT,"):
                    s["xbox_name"] = line.split(",")[2] if line.count(",") >= 2 else ""
                    s["connected_count"] += 1
                    self.add("Xbox conectado • " + s["xbox_name"], "success")
                else:
                    self.add(line, "success")
            elif line.startswith(("DISCONNECTED,", "WATCHDOG,")):
                s["xbox_connected"] = False
                s["xbox_fresh"] = False
                s["xbox_armed"] = False
                s["input_last_seen"] = 0
                s["last_disconnect"] = line
                self.add(line, "error")
            elif line.startswith("STATUS,"):
                kv = extract_kv(line)
                previously = s["xbox_connected"]
                s["source"] = kv.get("source", s["source"])
                s["mode"] = kv.get("mode", s["mode"])
                s["xbox_armed"] = bool(int0(kv.get("armed", "0")))
                s["host"] = int0(kv.get("host", "0"))
                fresh = bool(int0(kv.get("fresh", "0")))
                valid = fresh and s["source"] in ("XBOX_BT", "XBOX360_USB")
                s["xbox_connected"] = valid
                s["xbox_fresh"] = valid
                s["xbox_last_seen"] = now
                if valid and not previously:
                    self.add("Controle " + s["source"] + " reconhecido no ESP32", "success")
                elif previously and not valid:
                    self.add("Controle desconectado ou sem dados válidos", "warning")
            elif line.startswith("INPUT2,"):
                f = line.split(",")
                if len(f) == 6 and f[1] in ("XBOX_BT", "XBOX360_USB"):
                    try:
                        steer, rt, lt, buttons = (int(v) for v in f[2:])
                        if -512 <= steer <= 512 and 0 <= rt <= 1023 and 0 <= lt <= 1023 and 0 <= buttons <= 255:
                            s.update(steer=steer, rt=rt, lt=lt, buttons=buttons,
                                     input_last_seen=now, source=f[1])
                    except ValueError:
                        pass
            elif line.startswith("CAR,"):
                kv = extract_kv(line)
                previous = s["car_state"]
                s["car_state"] = kv.get("STATE", "UNKNOWN")
                s["car_err"] = int0(kv.get("ERR"))
                s["car_scans"] = int0(kv.get("SCAN"))
                s["car_connections"] = int0(kv.get("CONN"))
                s["car_tx"] = int0(kv.get("TX_NEUTRAL"))
                s["car_last_seen"] = now
                if s["car_state"] != previous:
                    cat = "success" if s["car_state"] == "READY" else "warning"
                    self.add("Ferrari BLE: " + s["car_state"] + " (erro " + str(s["car_err"]) + ")", cat)
            elif line.startswith("CAR_DRIVE,"):
                kv = extract_kv(line)
                s["car_armed"] = bool(int0(kv.get("ARMED")))
                s["car_drive_mode"] = kv.get("MODE", "DIRECT")
                s["car_last_write"] = int0(kv.get("TX"))
            elif line.startswith("BT_DIAG,"):
                kv = extract_kv(line)
                s["xbox_ads"] = int0(kv.get("XBOX_ADS"))
                s["other_ads"] = int0(kv.get("OTHER_ADS"))
                s["scan_restarts"] = int0(kv.get("RESTARTS"))
            elif line.startswith("XBOX_ADV,"):
                kv=extract_kv(line)
                for field,tag in (("xbox_adv_type","TYPE"),("xbox_adv_addr_type","ADDR_TYPE"),
                                  ("xbox_adv_appearance","APPEARANCE"),
                                  ("xbox_adv_without_appearance","NO_APPEARANCE"),
                                  ("xbox_bond_matches","BONDED"),
                                  ("xbox_le_status","LE_STATUS"),
                                  ("xbox_le_ms","LE_MS"),
                                  ("xbox_disconnect_reason","DISCONNECT_REASON")):
                    s[field]=int0(kv.get(tag),255)
                if now-self.last_log_emit.get("XBOX_ADV",0)>8:
                    self.add(line,"info")
                    self.last_log_emit["XBOX_ADV"]=now
            elif line.startswith(("XBOX_SUPERVISION,", "XBOX_HOLD,", "LINKED,")):
                kv = extract_kv(line)
                if "ACTUAL_MS" in kv:
                    s["xbox_supervision_ms"] = int0(kv["ACTUAL_MS"])
                if "SUPERVISION_MS" in kv:
                    s["xbox_supervision_ms"] = int0(kv["SUPERVISION_MS"])
                if "LIVE" in kv:
                    s["xbox_hold_live"] = bool(int0(kv["LIVE"]))
                self.add(line, "info", dedup=True)
            elif line.startswith("XBOX_LINK,"):
                kv=extract_kv(line)
                s["xbox_hci"]=int0(kv.get("HCI"))
                s["xbox_ready"]=int0(kv.get("READY"))
                s["xbox_hci_age"]=int0(kv.get("AGE_MS"))
                s["xbox_link_recoveries"]=int0(kv.get("RECOVERIES"))
            elif line.startswith("XBOX_RECOVERY,"):
                self.add(line,"warning")
            elif line.startswith(("BT_RECOVERY,", "BT_PRIORITY,", "SAFETY,", "MOTOR,", "STATE,")):
                self.add(line, "warning" if line.startswith("SAFETY,") else "info")
            elif line.startswith(("ERROR,", "USB_HOST,NOT_DETECTED")):
                self.add(line, "warning", dedup=True)
            elif line.startswith(("READY,", "BT,", "USB_HOST,", "IGNORED,")):
                self.add(line, "info", dedup=True)

    def set_transport(self, name, note=""):
        with self.lock:
            if self.state["transport"] != name:
                self.state["transport"] = name
                self.state["note"] = note
                cat = "success" if name == "SERIAL_USB" else "warning"
                self.add("Monitor: " + name + ((" • " + note) if note else ""), cat)

    def snapshot(self, after):
        with self.lock:
            s = self.state.copy()
            now = time.time()
            # Timeouts prevent stale green indicators when the serial dies.
            s["xbox_connected"] = bool(s["xbox_connected"] and
                now-s["xbox_last_seen"]<4.0 and
                s["transport"] in ("SERIAL_USB","LOG_COMPARTILHADO"))
            s["xbox_armed"] = bool(s["xbox_armed"] and s["xbox_connected"])
            s["car_state"] = s["car_state"] if now-s["car_last_seen"]<8.0 else "UNKNOWN"
            s["car_armed"] = bool(s["car_armed"] and s["car_state"]=="READY" and s["xbox_connected"])
            if now-s["input_last_seen"] > 0.8 or not s["xbox_connected"]:
                s["steer"] = s["rt"] = s["lt"] = s["buttons"] = 0
            s["input_age"] = round(now-s["input_last_seen"], 2) if s["input_last_seen"] else -1
            s["last_frame_age"] = round(now-s["last_frame"], 2) if s["last_frame"] else -1
            updates = [event for event in self.events if event["id"] > after]
            return {"state":s,"events":updates[-160:], "last_id":self.serial_no}

    def run_reader(self):
        # Passive fallback when another monitor owns serial, then become owner
        # automatically when that monitor exits.
        import serial
        last_attempt = 0.
        # If an old text monitor is already running, tail NEW lines only.
        last_tail = LOG.stat().st_size if LOG.exists() else 0
        ser = None
        self.set_transport("AGUARDANDO_USB")
        while not self._stop.is_set():
            if ser is None and time.monotonic()-last_attempt >= 1.5:
                last_attempt = time.monotonic()
                try:
                    # PySerial alone allows multiple readers of a TTY.
                    # Never compete with the old text monitor for bytes.
                    busy = subprocess.run(["fuser", "-s", PORT], stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,timeout=1.0,check=False)
                    if busy.returncode == 0:
                        raise serial.SerialException("outra janela já monitora o ESP32")
                    if Path(PORT).exists() and not os.access(PORT, os.R_OK | os.W_OK):
                        raise PermissionError("sem permissão Linux para /dev/ttyUSB0")
                    ser = serial.Serial(PORT,115200,timeout=0.45,write_timeout=0.5,exclusive=True)
                    self.set_transport("SERIAL_USB","leitura exclusiva; sem enviar comandos")
                    with self.lock:
                        self.state["xbox_last_seen"]=0
                        self.state["car_last_seen"]=0
                except (serial.SerialException, OSError) as exc:
                    is_permission = isinstance(exc, PermissionError) or ("Permission denied" in str(exc))
                    is_busy = "outra janela" in str(exc) or ("busy" in str(exc).lower())
                    if is_permission:
                        self.set_transport("SEM_PERMISSAO", "Use o atalho 'Shell RC Pro - Corrigir USB ESP32'")
                    elif is_busy:
                        self.set_transport("LOG_COMPARTILHADO","porta ocupada; lendo o log antigo")
                    else:
                        self.set_transport("RECONECTANDO","Falha USB: " + str(exc)[:120])
                    # Read only new lines if another monitor currently owns the port.
                    try:
                        if is_busy and LOG.exists():
                            size = LOG.stat().st_size
                            if size < last_tail:
                                last_tail = 0
                            if size > last_tail:
                                with LOG.open("rb") as f:
                                    f.seek(last_tail)
                                    chunk = f.read(70_000)
                                    last_tail = f.tell()
                                for line in chunk.decode("utf-8","replace").splitlines():
                                    self.consume(line,from_saved_log=True)
                    except OSError:
                        pass
                    time.sleep(0.35)
            if ser is not None:
                try:
                    text = ser.readline().decode("utf-8","replace").strip()
                    if text:
                        self.consume(text)
                except (serial.SerialException, OSError) as exc:
                    try:
                        ser.close()
                    except Exception:
                        pass
                    ser = None
                    self.set_transport("RECONECTANDO","USB indisponível: " + str(exc)[:80])
                    time.sleep(.4)


MONITOR = Monitor()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def send_bytes(self, data, content_type):
        self.send_response(200)
        self.send_header("Content-Type",content_type)
        self.send_header("Content-Length",str(len(data)))
        self.send_header("Cache-Control","no-store")
        self.send_header("X-Content-Type-Options","nosniff")
        self.send_header("Content-Security-Policy","default-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' data:;")
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_GET(self):
        parsed = urlsplit(self.path)
        if parsed.path == "/api/state":
            vals = parse_qs(parsed.query)
            after = max(0,min(int0(vals.get("after",["0"])[0]), 999999999))
            state = MONITOR.snapshot(after)
            return self.send_bytes(json.dumps(state,ensure_ascii=False).encode("utf-8"),"application/json; charset=utf-8")
        if parsed.path == "/api/health":
            return self.send_bytes(b'{"ok":true,"app":"Shell RC Pro ESP32"}',"application/json")
        if parsed.path in ("/","/index.html"):
            return self.send_bytes(HTML.read_bytes(),"text/html; charset=utf-8")
        self.send_error(404,"Not found")

def main():
    threading.Thread(target=MONITOR.run_reader,daemon=True,name="ESP32 USB Reader").start()
    httpd=ThreadingHTTPServer((HOST,HTTP_PORT),Handler)
    MONITOR.add(f"Painel iniciado em http://{HOST}:{HTTP_PORT}/", "system")
    print(f"Shell RC Pro Dashboard: http://{HOST}:{HTTP_PORT}/",flush=True)
    try:
        httpd.serve_forever(poll_interval=.3)
    except KeyboardInterrupt:
        pass
    finally:
        MONITOR._stop.set()
        httpd.server_close()

if __name__ == "__main__":
    main()
