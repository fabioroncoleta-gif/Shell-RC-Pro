"""Sessao dupla BLE: carrinho original para direcao/luzes, placa isolada para PWM.
A ativacao real depende de vistoria eletrica e nao ocorre por padrao.
"""
from __future__ import annotations
import asyncio
import json
import time
from pathlib import Path
try:
    from .hybrid_protocol import PWM_BLE_NAME, UUID_WRITE, compose_existing_and_pwm, encode
except ImportError: # scripts/tests executados diretamente em HW_PWM
    from hybrid_protocol import PWM_BLE_NAME, UUID_WRITE, compose_existing_and_pwm, encode
ORIGINAL_UUID = "0000fff1-0000-1000-8000-00805f9b34fb"
APPROVAL_PATH = Path(__file__).with_name("INSTALLATION_APPROVAL.json")

def validate_approval(path=APPROVAL_PATH):
    """Exige vistoria documentada. Arquivo nao prova seguranca fisica por si so."""
    if not Path(path).is_file():
        raise RuntimeError("Modulo fisico nao vistoriado: INSTALLATION_APPROVAL.json ausente.")
    p = json.loads(Path(path).read_text())
    keys = ("motor_isolated_from_u6", "drv8833_or_equivalent_physically_installed",
            "wiring_inspected", "motor_current_measured", "power_supply_measured",
            "wheels_up_bench_passed")
    if not all(p.get(k) is True for k in keys):
        raise RuntimeError("Vistoria da instalacao PWM INCOMPLETA; modo real bloqueado.")
    v = p.get("actual_supply_voltage_v")
    peak = p.get("measured_motor_stall_current_a")
    driver_limit = p.get("driver_peak_rating_a")
    if not all(type(x) in (int, float) and x > 0 for x in (v, peak, driver_limit)):
        raise RuntimeError("Tensao/corrente/limite do driver nao documentados.")
    if not (2.7 <= v <= 10.8) or peak > driver_limit:
        raise RuntimeError("Motor/alimentacao fora do limite do driver; bloqueado.")
    return True

async def run_hybrid(run):
    from bleak import BleakClient, BleakScanner
    cfg = run.cfg
    while run.running:
        if run.pad is None:
            await asyncio.sleep(.2)
            continue
        run.msg("Procurando Ferrari e placa PWM separada...")
        try:
            discovered = await BleakScanner.discover(timeout=5.0)
            if run.args.address:
                original = run.args.address
            else:
                cars = [x for x in discovered if (x.name or "").startswith("SL-") and
                        cfg["car_name_filter"].lower() in (x.name or "").lower()]
                if len(cars) != 1:
                    raise RuntimeError(f"Ferraris detectadas: {len(cars)} (esperado 1)")
                original = cars[0]
            if run.args.pwm_address:
                pwm_target = run.args.pwm_address
            else:
                pwms = [x for x in discovered if (x.name or "") == PWM_BLE_NAME]
                if len(pwms) != 1:
                    raise RuntimeError(f"Controladores PWM detectados: {len(pwms)} (esperado 1)")
                pwm_target = pwms[0]
            async with BleakClient(original, timeout=12) as original_ble:
                async with BleakClient(pwm_target, timeout=12) as pwm_ble:
                    car_char = original_ble.services.get_characteristic(ORIGINAL_UUID)
                    pwm_char = pwm_ble.services.get_characteristic(UUID_WRITE)
                    if not car_char or not pwm_char:
                        raise RuntimeError("Caracteristica BLE esperada nao encontrada")
                    # Somente apos os dois links conectados podemos permitir armar.
                    run.stop("DOIS LINKS CONECTADOS: pressione START para armar com rodas suspensas")
                    run.link = True
                    seq = 0
                    await asyncio.gather(
                        original_ble.write_gatt_char(car_char, bytes((1, 0, 0, 0, 0, 0, 0, 0)), response=False),
                        pwm_ble.write_gatt_char(pwm_char, encode(seq, 0, 0, False), response=False))
                    seq = (seq + 1) & 255
                    while (run.running and run.pad is not None and
                           original_ble.is_connected and pwm_ble.is_connected):
                        tick = time.monotonic()
                        car_data, pwm_data = compose_existing_and_pwm(run, seq)
                        seq = (seq + 1) & 255
                        # PWM deve sempre ter watchdog local, tambem para travamento do notebook.
                        await asyncio.gather(
                            original_ble.write_gatt_char(car_char, car_data, response=False),
                            pwm_ble.write_gatt_char(pwm_char, pwm_data, response=False))
                        run.sent = car_data
                        await asyncio.sleep(max(.001, cfg["interval"] - (time.monotonic() - tick)))
                    run.stop("LINK ENCERRADO: desarmado")
                    if pwm_ble.is_connected:
                        try:
                            await pwm_ble.write_gatt_char(pwm_char, encode(seq, 0, 0, False), response=False)
                        except Exception:
                            pass
                    if original_ble.is_connected:
                        try:
                            await original_ble.write_gatt_char(car_char, bytes((1, 0, 0, 0, 0, 0, 0, 0)), response=False)
                        except Exception:
                            pass
        except Exception as exc:
            run.msg("Falha hibrido: " + str(exc)[:125])
        finally:
            run.link = False
            run.stop("Desconectado; watchdog PWM deve desligar tracao")
        if run.running:
            await asyncio.sleep(2)
