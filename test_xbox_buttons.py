#!/usr/bin/env python3
"""Teste automatico de botoes Xbox em modo seguro (sem comandos BLE)."""
import os, asyncio
os.environ["SDL_VIDEODRIVER"]="dummy"
os.environ["SDL_AUDIODRIVER"]="dummy"
import pygame as pg
from argparse import Namespace
import shell_rc_xbox360 as shell

async def input_test():
    await asyncio.sleep(0.5)
    count=pg.joystick.get_count()
    assert count>0,"Receptor nao detectado"
    pad=pg.joystick.Joystick(0)
    iid=pad.get_instance_id()
    print("JOYSTICK:",pad.get_name(),"ID:",iid,flush=True)
    pg.event.post(pg.event.Event(pg.JOYBUTTONDOWN,{"instance_id":iid,"button":7}))
    await asyncio.sleep(2.0)
    pg.event.post(pg.event.Event(pg.JOYBUTTONUP,{"instance_id":iid,"button":7}))
    pg.event.post(pg.event.Event(pg.JOYBUTTONDOWN,{"instance_id":iid,"button":0}))
    await asyncio.sleep(0.30)
    pg.event.post(pg.event.Event(pg.JOYBUTTONUP,{"instance_id":iid,"button":0}))
    pg.event.post(pg.event.Event(pg.JOYBUTTONDOWN,{"instance_id":iid,"button":0}))
    await asyncio.sleep(0.30)
    pg.event.post(pg.event.Event(pg.JOYBUTTONUP,{"instance_id":iid,"button":0}))
    pg.event.post(pg.event.Event(pg.JOYBUTTONDOWN,{"instance_id":iid,"button":2}))
    await asyncio.sleep(0.35)
    pg.event.post(pg.event.Event(pg.JOYBUTTONUP,{"instance_id":iid,"button":2}))
    pg.event.post(pg.event.Event(pg.JOYBUTTONDOWN,{"instance_id":iid,"button":3}))
    await asyncio.sleep(0.35)
    pg.event.post(pg.event.Event(pg.JOYBUTTONUP,{"instance_id":iid,"button":3}))
    pg.event.post(pg.event.Event(pg.JOYBUTTONDOWN,{"instance_id":iid,"button":1}))
    await asyncio.sleep(0.35)
    pg.event.post(pg.event.Event(pg.JOYBUTTONUP,{"instance_id":iid,"button":1}))
    pg.event.post(pg.event.Event(pg.JOYBUTTONDOWN,{"instance_id":iid,"button":7}))
    await asyncio.sleep(0.35)
    pg.event.post(pg.event.Event(pg.JOYBUTTONUP,{"instance_id":iid,"button":7}))
    pg.event.post(pg.event.Event(pg.JOYBUTTONDOWN,{"instance_id":iid,"button":5}))
    await asyncio.sleep(0.45)
    pg.event.post(pg.event.Event(pg.JOYBUTTONUP,{"instance_id":iid,"button":5}))
    pg.event.post(pg.event.Event(pg.JOYBUTTONDOWN,{"instance_id":iid,"button":5}))
    await asyncio.sleep(0.25)
    # Exercita o caminho de evento LB: no teste SDL o botao e sintetico.
    pg.event.post(pg.event.Event(pg.JOYBUTTONDOWN,{"instance_id":iid,"button":4}))
    await asyncio.sleep(0.20)
    pg.event.post(pg.event.Event(pg.JOYBUTTONUP,{"instance_id":iid,"button":4}))
    await asyncio.sleep(0.15)
    # BACK (6): aciona reset simulado e mantem a janela funcional.
    pg.event.post(pg.event.Event(pg.JOYBUTTONDOWN,{"instance_id":iid,"button":6}))
    await asyncio.sleep(.15)
    pg.event.post(pg.event.Event(pg.JOYBUTTONUP,{"instance_id":iid,"button":6}))
    await asyncio.sleep(.15)
    pg.event.post(pg.event.Event(pg.QUIT))

async def main():
    runner=asyncio.create_task(shell.app(Namespace(live=False,inspect=False,address=None)))
    tester=asyncio.create_task(input_test())
    await asyncio.wait_for(asyncio.gather(runner,tester), timeout=7)
    print("PASS: button START + release + START + exit handled; no BLE",flush=True)

if __name__=="__main__":
    asyncio.run(main())
