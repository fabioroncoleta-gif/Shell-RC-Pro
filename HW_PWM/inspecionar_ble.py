#!/usr/bin/env python3
"""Inspecao BLE somente leitura: nunca escreve comandos ao carro."""
import asyncio
from bleak import BleakClient, BleakScanner
async def main():
    cars=[d for d in await BleakScanner.discover(timeout=7)
          if (d.name or "").lower().startswith("sl-488")]
    if len(cars)!=1:
        print(f"Ferraris SL-488 encontradas: {len(cars)}; esperada 1")
        return
    print("Encontrado:",cars[0].name)
    async with BleakClient(cars[0],timeout=15) as conn:
        print("Ligado via BLE:",conn.is_connected)
        for service in conn.services:
            print("SERVICE",service.uuid)
            for char in service.characteristics:
                print(" CHAR",char.uuid,"properties:",','.join(char.properties))
                if char.uuid.lower().startswith("00002a19") and "read" in char.properties:
                    level=await conn.read_gatt_char(char)
                    print(" BATTERY",int(level[0]),"%")
    print("PASS: leitura BLE. Nenhum pacote enviado para movimentar motores.")
if __name__=="__main__":asyncio.run(main())
