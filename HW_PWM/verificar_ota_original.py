#!/usr/bin/env python3
"""Read-only OTA capability inventory. NEVER writes BLE characteristics."""
import asyncio
import json
from datetime import datetime, timezone
from pathlib import Path
from bleak import BleakScanner, BleakClient
ROOT=Path(__file__).resolve().parent
OTA_NAMES={
 '8d53dc1d-1db7-4cd3-868b-8a527460aa84':'SMP / MCUboot indicator',
 '0000fe59-0000-1000-8000-00805f9b34fb':'Nordic secure DFU indicator'
}
async def main():
    devices = await BleakScanner.discover(timeout=7.0)
    cars=[d for d in devices if (d.name or '')=='SL-488 Challenge Evo']
    if len(cars)!=1:
        raise RuntimeError(f'Esperado exatamente uma Ferrari; detectadas {len(cars)}')
    result={
      'time_utc':datetime.now(timezone.utc).isoformat(),
      'name':cars[0].name,
      'address':cars[0].address,
      'read_only':True,
      'write_requests':0,
      'services':[],
      'known_ota_service_identified':False,
      'proprietary_ota_status':'UNKNOWN',
      'flash_attempted':False,
      'original_firmware_preserved':True
    }
    async with BleakClient(cars[0],timeout=12) as conn:
        for service in conn.services:
            entry={'uuid':service.uuid,'characteristics':[]}
            for ch in service.characteristics:
                item={'uuid':ch.uuid,'properties':list(ch.properties)}
                if ch.uuid.lower().startswith('00002a19') and 'read' in ch.properties:
                    try:item['battery_percent']=int((await conn.read_gatt_char(ch))[0])
                    except Exception as e:item['battery_read_error']=type(e).__name__
                entry['characteristics'].append(item)
            result['services'].append(entry)
        result['known_ota_service_identified']=any(s['uuid'].lower() in OTA_NAMES for s in result['services'])
    (ROOT/'BLE_ORIGINAL_OTA_PREFLIGHT.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(result,ensure_ascii=False,indent=2))
    print('Seguro: nenhum comando de motor, flash ou atualização enviado.')
asyncio.run(main())
