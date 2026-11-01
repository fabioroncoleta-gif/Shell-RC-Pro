#!/usr/bin/env python3
"""Shell RC Pro Xbox360 -> USB receiver (Linux) -> BLE Ferrari Shell.
MODO SEGURO por padrao; --live habilita envio ao carrinho.
"""
import asyncio, argparse, json, time
from pathlib import Path
from motor_profile import MotorController,curve
from light_effects import LightEffects
from reverse_safety import ReverseUnlock
from car_discovery import FleetRegistry, address_of, filter_shell_devices, choose_device
BASE=Path(__file__).resolve().parent
CTRL_UUID="0000fff1-0000-1000-8000-00805f9b34fb"
NEUTRAL=bytes([1,0,0,0,0,0,0,0])
CONFIG_DEFAULT=dict(steering_axis=0,lt_axis=2,rt_axis=5,start_button=7,back_button=6,
                    lights_button=0,fade_button=2,strobe_button=3,
                    turbo_button=1,rb_mode_button=5,lb_brake_button=4,direct_engage_trigger=.13,
                    strobe_hz=3.0,strobe_on_fraction=.4,fade_period_s=3.2,
                    car_name_filter="488",max_forward=1.0,
                    max_reverse=1.0,forward_expo=1.25,reverse_expo=1.4,
                    forward_min_duty=.20,reverse_min_duty=.24,
                    trigger_deadzone=.085,full_throttle_at=.96,
                    input_filter_s=.09,forward_accel_rate=1.1,reverse_accel_rate=.78,
                    forward_decel_rate=2.6,reverse_decel_rate=1.8,
                    jerk_limit=8.,reverse_lockout=.42,steer_deadzone=.24,
                    interval=.05,invert_steering=False,button_debounce_s=.3)

def clip(x,a=0,b=1): return min(b,max(a,x))
def trig(x,neutral):
    return clip((x-neutral)/(1-neutral)) if 1-neutral>0.15 else 0
class Run:
    def __init__(self,args,cfg):
        self.args=args;self.cfg=cfg
        self.running=True; self.pad=None;self.link=False
        self.armed=False;self.arm_asked=False;self.neutral_start=None
        self.lt0=0;self.rt0=0;self.lt=0;self.rt=0;self.steer=0
        self.speed=0;self.direction=0;self.phase=0
        self.motor=MotorController(cfg)
        self.reverse_logic=ReverseUnlock(
            full_threshold=cfg.get("reverse_triplet_full_threshold",.94),
            release_threshold=cfg.get("reverse_triplet_release_threshold",.18),
            window_s=cfg.get("reverse_triplet_window_s",3.0))
        self.drive_mode=str(cfg.get("default_drive_mode","pulse"))
        self.boost_output=False
        self.turbo=False
        self.brake_held=False
        self.brake_release_wait_neutral=False
        self.brake_steering=0.
        self.brake_original_direction=0
        self.brake_resume_check=False
        self.last_button_time={}
        self.lights_ctrl=LightEffects(cfg)
        self.status="Aguardando receptor Xbox";self.sent=NEUTRAL
        self.car="-"
        self.battery_pct=None
        self.fleet=FleetRegistry(BASE/"fleet.json")
        self.cars=[]
        self.selected_car_address=None
        self.active_car_address=None
        self.switch_requested=False
        self.scanning=False
        self.incompatible_addresses=set()
        self.last_pad_update=time.monotonic()
        self.last_ble_success=None
        self.ble_write_failures=0
        self.cached_ble_target=None
        self.reconnect_target_address=None
        self.reconnect_pending=False
        self.reconnect_event=asyncio.Event()
        self.reconnect_generation=0
        self.quick_reset_attempts=0
        self.reconnect_started_at=None
        self.last_reconnect_ms=None
        self.last_back_time=-100.
    def msg(self,s):
        self.status=s
        print(time.strftime("%H:%M:%S"),s,flush=True)
    def stop(self,s):
        self.reverse_logic.reset()
        self.motor.reverse_full_enabled=False
        self.armed=False;self.arm_asked=False;self.neutral_start=None
        self.speed=0;self.direction=0;self.phase=0;self.sent=NEUTRAL
        self.motor.reset(time.monotonic())
        self.turbo=False
        self.boost_output=False
        self.brake_held=False
        self.brake_release_wait_neutral=False
        self.brake_steering=0.
        self.brake_original_direction=0
        self.brake_resume_check=False
        self.msg(s)
    def select_car(self, index):
        if not 0 <= index < min(len(self.cars),9):
            self.msg("Ferrari nao encontrada nessa posicao; tecla R para buscar")
            return
        address=address_of(self.cars[index])
        if self.link and address==self.active_car_address:
            self.msg("Esta Ferrari ja esta conectada")
            return
        self.stop("Selecionada outra Ferrari: desarmado; conectando...")
        self.selected_car_address=address
        self.switch_requested=True

    def refresh_cars(self):
        self.stop("Atualizando lista de Ferraris: motor desarmado")
        self.selected_car_address=None
        self.switch_requested=True
        self.incompatible_addresses.clear()

    def request_fast_reconnect(self,now=None):
        """BACK: desarme imediato, reconexao exclusiva da ultima Ferrari."""
        now=time.monotonic() if now is None else now
        if now-self.last_back_time < float(self.cfg.get("back_debounce_ms",350))/1000.:
            return False
        self.last_back_time=now
        address=(self.active_car_address or self.reconnect_target_address or
                 self.selected_car_address or self.fleet.last_address or
                 self.cfg.get("preferred_car_address"))
        self.stop("BACK: parada imediata e reinicio da conexao")
        self.link=False   # impede novo START durante a troca da conexao
        self.reconnect_generation+=1
        self.reconnect_event.set()  # acorda transmissao BLE que estiver pendente
        self.switch_requested=True
        self.reconnect_started_at=now
        self.quick_reset_attempts=0
        if not address:
            self.reconnect_pending=False
            self.msg("BACK: nenhum carrinho conhecido; iniciar busca normal")
            return True
        self.reconnect_pending=True
        self.reconnect_target_address=address
        self.selected_car_address=address
        if not getattr(self.args,"live",False) and not getattr(self.args,"hybrid_live",False):
            self.reconnect_pending=False
            self.switch_requested=False
            self.reconnect_event.clear()
            self.msg("BACK: reinicio simulado (nenhuma conexao BLE real)")
        elif getattr(self.args,"hybrid_live",False):
            self.reconnect_pending=False
            self.msg("BACK: apenas parada segura; modo PWM hibrido sem reset BLE")
        else:
            self.msg("BACK: tentando reconectar a mesma Ferrari: "+address)
        return True

    def set_light_mode(self, mode):
        now=time.monotonic()
        if now-self.last_button_time.get(mode,-100.)<self.cfg.get("button_debounce_s",.30):
            return
        self.last_button_time[mode]=now
        self.lights_ctrl.select(mode,now)
        self.msg("FAROIS: "+self.lights_ctrl.label())

    def toggle_turbo(self):
        if self.brake_held or self.brake_release_wait_neutral:
            self.msg("LB: turbo bloqueado ate liberar tracao")
            return
        now=time.monotonic()
        if now-self.last_button_time.get("turbo",-100.)<self.cfg.get("button_debounce_s",.30):
            return
        self.last_button_time["turbo"]=now
        self.turbo=not self.turbo
        if not self.turbo: self.boost_output=False
        self.msg("TURBO: "+("LIGADO" if self.turbo else "DESLIGADO"))

    def toggle_drive_mode(self):
        if self.armed or self.arm_asked:
            self.msg("Desarme com START antes de trocar o modo de motor")
            return
        self.drive_mode="direct" if self.drive_mode=="pulse" else "pulse"
        self.boost_output=False
        self.motor.reset(time.monotonic())
        label="DIRETO: sem pulsos, velocidade nao proporcional" if self.drive_mode=="direct" else "PULSOS: velocidade aproximada"
        self.msg("MOTOR: "+label)

    def apply_handbrake(self, pressed):
        """LB corta apenas a tracao; memoriza volante e potencia para retomar."""
        pressed=bool(pressed)
        if pressed and not self.brake_held:
            self.brake_held=True
            self.brake_steering=self.steer
            self.brake_original_direction=self.motor.state.direction
            self.brake_resume_check=False
            self.arm_asked=False
            self.neutral_start=None
            # O estado do motor e mantido, mas nenhum bit de frente/re sera enviado.
            self.msg("LB: motor em neutro; direcao preservada")
        elif not pressed and self.brake_held:
            self.brake_held=False
            self.brake_resume_check=True
            self.msg("LB solto: retomada pelo RT/LT sem exigir neutro")

    def command_packet(self):
        """LB mantem o comando de direcao, sem frente/re/turbo."""
        lights=self.lights_ctrl.output(time.monotonic())
        if self.brake_held or self.brake_release_wait_neutral:
            steer=self.brake_steering if self.brake_held else self.steer
            dead=float(self.cfg.get("steer_deadzone",.24))
            return bytes([1,0,0,int(steer < -dead),int(steer > dead),int(lights),0,0])
        trigger=self.rt if self.motor.state.direction==1 else self.lt
        limited_reverse=(self.motor.state.direction==-1 and not self.reverse_logic.unlocked)
        use_boost=self.turbo and self.armed and not limited_reverse
        if self.drive_mode=="direct":
            # A placa original admite apenas duas condicoes de potencia: normal e turbo.
            # Na partida manter a tracao normal sem alternar bits de frente/re.
            # Turbo B e liberado na rampa virtual com histerese; NAO e PWM eletrico.
            if not use_boost or self.motor.state.direction==0:
                self.boost_output=False
            else:
                on=float(self.cfg.get("direct_boost_on_duty",.80))
                off=float(self.cfg.get("direct_boost_off_duty",.55))
                if self.boost_output:
                    if self.motor.state.duty < off: self.boost_output=False
                elif self.motor.state.duty >= on:
                    self.boost_output=True
            use_boost=use_boost and self.boost_output
        # O firmware original usa comandos binarios. Para re reduzida,
        # so ha aproximacao por tempo ON/OFF, nao PWM eletrico verdadeiro.
        reverse_drive_mode="pulse" if limited_reverse else self.drive_mode
        return self.motor.packet(self.steer,lights,self.armed,
                  reverse_drive_mode,use_boost,direct_trigger=trigger)

    def find_pad(self,pg):
        # pygame.joystick.Joystick nao implementa get_attached().
        # Conferimos se o ID do controle ainda existe na enumeracao SDL.
        if self.pad:
            try:
                current_id = self.pad.get_instance_id()
                connected_ids = {pg.joystick.Joystick(i).get_instance_id()
                                 for i in range(pg.joystick.get_count())}
                if self.pad.get_init() and current_id in connected_ids:
                    return
            except (pg.error, AttributeError):
                pass
            self.pad=None
            self.stop("Controle desconectado; motor desarmado")
        for i in range(pg.joystick.get_count()):
            p=pg.joystick.Joystick(i);p.init()
            if "xbox" in p.get_name().lower() or "360" in p.get_name().lower():
                self.pad=p
                a,b=self.cfg["lt_axis"],self.cfg["rt_axis"]
                if p.get_numaxes()<=max(a,b,self.cfg["steering_axis"]):
                    self.pad=None;self.msg("Eixos nao compativeis; use --inspect");return
                self.lt0=p.get_axis(a);self.rt0=p.get_axis(b)
                self.msg("Xbox encontrado: "+p.get_name()+f" (LT0={self.lt0:.2f},RT0={self.rt0:.2f})")
                return
    def read_pad(self,dt):
        p=self.pad
        if p is None:
            self.motor.reset(time.monotonic())
            self.speed=0;self.direction=0
            self.brake_release_wait_neutral=True
            return
        c=self.cfg
        try:
            lt=trig(p.get_axis(c["lt_axis"]),self.lt0)
            rt=trig(p.get_axis(c["rt_axis"]),self.rt0)
            steer=clip(p.get_axis(c["steering_axis"]),-1,1)
            brake_pressed=bool(p.get_button(c["lb_brake_button"]))
        except Exception as controller_error:
            self.pad=None
            self.stop("FALHA XBOX: leitura interrompida; motor desarmado: "+str(controller_error)[:70])
            return
        self.lt,self.rt,self.steer=lt,rt,steer
        self.last_pad_update=time.monotonic()
        if c["invert_steering"]:self.steer=-self.steer
        neutral=self.lt<0.07 and self.rt<0.07 and abs(self.steer)<0.12
        self.apply_handbrake(brake_pressed)
        unlocked=self.reverse_logic.update(
            self.lt,self.rt,time.monotonic(),
            armed=self.armed,
            brake=(self.brake_held or self.brake_release_wait_neutral))
        if unlocked:
            self.motor.reverse_full_enabled=True
            self.msg("RE TOTAL LIBERADA: LT acionado ate o fim 3 vezes")
        if self.brake_held:
            # Mantem o estado interno do gatilho; apenas bloqueia a saida BLE.
            return
        if self.brake_resume_check:
            self.brake_resume_check=False
            requested_dir=1 if self.rt>c["trigger_deadzone"] and self.lt<=c["trigger_deadzone"] else (-1 if self.lt>c["trigger_deadzone"] and self.rt<=c["trigger_deadzone"] else 0)
            if self.brake_original_direction and requested_dir and requested_dir!=self.brake_original_direction:
                # Inversao forçada durante LB: nao permitir tranco reverso.
                self.brake_release_wait_neutral=True
                self.motor.reset(time.monotonic())
                self.msg("LB: sentido mudou; solte RT/LT antes de inverter")
            else:
                self.brake_release_wait_neutral=False
        if self.brake_release_wait_neutral:
            if self.lt<0.07 and self.rt<0.07:
                self.brake_release_wait_neutral=False
                self.msg("LB: sentido liberado")
            else:
                self.motor.reset(time.monotonic())
                self.speed=0.;self.direction=0
                return
        if self.arm_asked:
            if (getattr(self.args,"live",False) or getattr(self.args,"hybrid_live",False)) and not self.link:
                self.arm_asked=False
                self.neutral_start=None
                self.msg("START: aguarde conectar a Ferrari via Bluetooth")
            elif neutral:
                if self.neutral_start is None:self.neutral_start=time.monotonic()
                if time.monotonic()-self.neutral_start>=float(c.get("arm_neutral_seconds",.65)):
                    self.armed=True;self.arm_asked=False;self.msg("ARMADO (teste no ar!)")
            else:self.neutral_start=None
        self.speed,self.direction=self.motor.step(self.rt,self.lt,dt,time.monotonic(),self.armed)
    def check_input_watchdog(self,now=None):
        """Corta a tracao se os comandos do controle pararem de atualizar."""
        now=time.monotonic() if now is None else now
        if self.armed and now-self.last_pad_update > float(self.cfg.get("input_watchdog_s",.4)):
            self.stop("WATCHDOG: controle sem atualizacao; motor desarmado")
            return True
        return False

    async def write_ble_safe(self,client,char,packet,response):
        """Uma escrita pendente por vez. Desconecta ao vencer o prazo."""
        try:
            await asyncio.wait_for(
                client.write_gatt_char(char,packet,response=response),
                timeout=float(self.cfg.get("ble_write_timeout_s",.85)))
        except Exception:
            self.ble_write_failures+=1
            raise
        self.last_ble_success=time.monotonic()

    async def write_drive_or_reset(self,client,char,packet,response):
        """Interrompe a espera de uma escrita quando BACK for pressionado."""
        if self.reconnect_event.is_set():
            return False
        writer=asyncio.create_task(self.write_ble_safe(client,char,packet,response))
        reset=asyncio.create_task(self.reconnect_event.wait())
        try:
            completed,_=await asyncio.wait((writer,reset),return_when=asyncio.FIRST_COMPLETED)
            if reset in completed:
                writer.cancel()
                await asyncio.gather(writer,return_exceptions=True)
                return False
            await writer  # propaga falha normal de escrita
            return not self.reconnect_event.is_set()
        finally:
            if not writer.done():
                writer.cancel()
                await asyncio.gather(writer,return_exceptions=True)
            reset.cancel()
            await asyncio.gather(reset,return_exceptions=True)

    async def ble_loop(self):
        if getattr(self.args, "hybrid_live", False):
            from HW_PWM.hybrid_ble import validate_approval, run_hybrid
            validate_approval()
            await run_hybrid(self)
            return
        if not self.args.live:
            self.msg("MODO TESTE: NENHUM comando Bluetooth e enviado")
            while self.running:
                self.sent=self.command_packet()
                await asyncio.sleep(self.cfg["interval"])
            return
        from bleak import BleakClient,BleakScanner
        while self.running:
            if not self.pad:
                await asyncio.sleep(.25)
                continue
            epoch=self.reconnect_generation
            fast=self.reconnect_pending
            target=None
            client=None
            char=None
            response=False
            try:
                requested=self.reconnect_target_address if fast else (self.args.address or self.selected_car_address)
                if fast:
                    self.quick_reset_attempts+=1
                    self.msg(f"BACK: tentativa {self.quick_reset_attempts} sem reiniciar o app")
                    # Reutilizar o BLEDevice retornado pelo BlueZ evita 4 s de scan.
                    if (self.quick_reset_attempts==1 and self.cached_ble_target is not None
                        and address_of(self.cached_ble_target)==requested.upper()):
                        target=self.cached_ble_target
                    elif self.quick_reset_attempts<=3:
                        target=await BleakScanner.find_device_by_address(
                            requested,
                            timeout=float(self.cfg.get("back_find_timeout_ms",1200))/1000.)
                if target is None:
                    self.scanning=True
                    self.msg("Buscando Ferraris Shell por Bluetooth...")
                    old=self.cfg.get("preferred_car_address")
                    known=set(self.fleet.cars)
                    if old: known.add(old)
                    if requested: known.add(requested)
                    discovered=await BleakScanner.discover(timeout=4.0)
                    self.cars=[d for d in filter_shell_devices(discovered,known)
                               if address_of(d) not in self.incompatible_addresses]
                    self.scanning=False
                    if epoch != self.reconnect_generation:
                        continue
                    if not self.cars:
                        self.msg("Nenhuma Ferrari visivel; deixe ligada e tente BACK")
                        await asyncio.sleep(.2 if fast else 2)
                        continue
                    target=choose_device(
                        self.cars,manual_address=requested,
                        last_address=None if fast else self.fleet.last_address,
                        legacy_address=None if fast else old)
                    if target is None:
                        self.msg("Ferrari esperada indisponivel; sem trocar para outro carrinho")
                        await asyncio.sleep(.2 if fast else 2)
                        continue
                if epoch != self.reconnect_generation:
                    continue
                address=address_of(target)
                self.cached_ble_target=target
                self.switch_requested=False
                self.reconnect_event.clear()
                timeout=(float(self.cfg.get("back_connect_timeout_ms",3500))/1000.
                         if fast else 10.)
                client=BleakClient(target,timeout=timeout)
                self.msg("BLE: conectando a "+str(target.name or address))
                await asyncio.wait_for(client.connect(),timeout=timeout+.25)
                if epoch!=self.reconnect_generation or self.switch_requested:
                    continue
                char=client.services.get_characteristic(CTRL_UUID)
                if not char or not any("write" in prop for prop in char.properties):
                    self.incompatible_addresses.add(address)
                    self.msg("Ferrari sem controle FFF1 gravavel; conexao recusada")
                    self.reconnect_pending=False
                    continue
                response="write-without-response" not in char.properties
                # Neutralizar antes de marcar a conexao como pronta.
                await self.write_ble_safe(client,char,NEUTRAL,response)
                if epoch!=self.reconnect_generation or self.switch_requested:
                    continue
                self.active_car_address=address
                self.link=True
                self.car=getattr(target,"name",None) or address
                self.stop("BLE CONECTADO: "+self.car+"; START para habilitar motor")
                if fast and self.reconnect_started_at is not None:
                    self.last_reconnect_ms=round((time.monotonic()-self.reconnect_started_at)*1000)
                    self.msg(f"BACK: reconectado em {self.last_reconnect_ms} ms; motor DESARMADO")
                self.reconnect_pending=False
                self.reconnect_target_address=None
                self.quick_reset_attempts=0
                self.battery_pct=None
                try:
                    self.fleet.remember_connected(target)
                except OSError as error:
                    self.msg("Conectado, mas nao foi possivel salvar historico: "+str(error)[:50])
                try:
                    level=client.services.get_characteristic("00002a19-0000-1000-8000-00805f9b34fb")
                    if level and "read" in level.properties:
                        battery=await asyncio.wait_for(client.read_gatt_char(level),timeout=.65)
                        if battery and 0<=battery[0]<=100:
                            self.battery_pct=int(battery[0])
                except Exception:
                    pass
                while self.running and self.pad and client.is_connected and not self.switch_requested:
                    begin=time.monotonic()
                    self.check_input_watchdog(begin)
                    packet=self.command_packet()
                    self.sent=packet
                    completed=await self.write_drive_or_reset(client,char,packet,response)
                    if not completed or epoch!=self.reconnect_generation:
                        break
                    await asyncio.sleep(max(.001,self.cfg["interval"]-(time.monotonic()-begin)))
            except asyncio.TimeoutError:
                self.stop("BLE: limite de tempo excedido, reconectando se solicitado")
                if fast:
                    self.cached_ble_target=None  # proxima tentativa faz busca direta nova
            except Exception as error:
                self.stop("BLE: falha "+str(error)[:120])
                if fast:
                    self.cached_ble_target=None
            finally:
                self.scanning=False
                # Corta sempre a tracao no software ANTES de tentar desligar o BLE.
                self.stop("BLE: seguro/desarmado; encerrando sessao anterior")
                if client is not None and client.is_connected:
                    if char is not None:
                        count=1 if self.reconnect_pending else 3
                        neutral_limit=float(self.cfg.get("back_neutral_timeout_ms",250))/1000. if fast or self.reconnect_pending else float(self.cfg.get("ble_write_timeout_s",.85))
                        for _ in range(count):
                            try:
                                await asyncio.wait_for(
                                    client.write_gatt_char(char,NEUTRAL,response=response),
                                    timeout=neutral_limit)
                            except Exception as error:
                                self.msg("BLE: nao confirmou parada neutra: "+str(error)[:90])
                                break
                    try:
                        await asyncio.wait_for(client.disconnect(),
                            timeout=float(self.cfg.get("back_disconnect_timeout_ms",700))/1000.)
                    except Exception as error:
                        self.msg("BLE: desconexao nao confirmada: "+str(error)[:90])
                self.link=False
                self.active_car_address=None
            if self.reconnect_pending:
                self.msg("BACK: buscando reconexao para mesma Ferrari")
                await asyncio.sleep(float(self.cfg.get("back_retry_delay_ms",120))/1000.)
            else:
                await asyncio.sleep(.4)

def paint(pg,screen,fonts,s):
    large,small=fonts;screen.fill((16,23,36))
    def write(msg,y,col=(230,235,248),font=None):
        screen.blit((font or small).render(msg,True,col),(28,y))
    write("SHELL RC PRO | CONTROLE XBOX 360",25,font=large)
    real = s.args.live or getattr(s.args,"hybrid_live",False)
    label = "PWM HIBRIDO (requer vistoria)" if getattr(s.args,"hybrid_live",False) else ("MODO REAL" if real else "MODO SIMULACAO (sem Bluetooth)")
    write(label,83,(250,184,96) if real else (92,207,255))
    write("CONTROLE: "+(s.pad.get_name() if s.pad else "receptor ainda nao conectado"),132)
    battery=f" | BATERIA: {s.battery_pct}%" if s.link and s.battery_pct is not None else ""
    write("FERRARI: "+(s.car if s.link else "aguardando")[:35]+battery,171)
    write("MOTOR: "+("ARMADO" if s.armed else "DESARMADO"),210,(111,234,155) if s.armed else (255,179,94))
    reverse_label="RE: TOTAL LIBERADA" if s.reverse_logic.unlocked else f"RE: LIMITADA 50% (LT x3 para liberar) [{s.reverse_logic.count}/3]"
    write(reverse_label,396,(255,205,136))
    write("FAROIS: "+s.lights_ctrl.label()+"  | A: fixo  X: fade  Y: strobo",238)
    boost_label="ATIVO" if s.boost_output else ("AGUARDANDO RAMPA" if s.turbo and s.drive_mode=="direct" else ("ON" if s.turbo else "OFF"))
    write("MODO: "+("DIRETO (normal -> turbo)" if s.drive_mode=="direct" else "PULSOS")+" | TURBO: "+boost_label,195,(255,195,105))
    if s.brake_held:
        write("LB: motor neutro | direcao congelada | retorno do gatilho",420,(255,118,107))
    elif s.brake_release_wait_neutral:
        write("LB: inversao bloqueada | solte os dois gatilhos",420,(255,118,107))
    for i,(label,val) in enumerate([("RT acelerar",s.rt),("LT re",s.lt),("Gatilho filtrado (binario no direto)",s.speed)]):
        y=270+i*53
        write(label,y)
        pg.draw.rect(screen,(48,59,82),(260,y,460,24),border_radius=5)
        pg.draw.rect(screen,(81,166,238),(260,y,round(460*clip(val)),24),border_radius=5)
        screen.blit(small.render(f"{val*100:.0f}%", True, (230,235,248)), (740,y))
    write(f"Direcao: {s.steer:+.2f} | Comando: {s.sent.hex(' ')}",446)
    write("BACK: reconectar | LB: freio | RB: modo | B: turbo | A/X/Y: farois",481)
    write(s.status[:100],513,(186,200,219))
    write("PRIMEIRO TESTE: rodas suspensas. Desligue o carro se perder a conexao BLE.",543,(246,179,96))
    write("FERRARIS DETECTADAS | 1-9 selecionar | R atualizar lista",575,(115,211,247))
    if not s.cars:
        write("Nenhum carrinho visivel" if not s.scanning else "Procurando...",601)
    for i, d in enumerate(s.cars[:9]):
        addr=address_of(d)
        marker="ATIVA" if s.link and s.active_car_address==addr else ("ESCOLHIDA" if s.selected_car_address==addr else "")
        write(f"{i+1}. {(d.name or 'Ferrari')[:29]}  {addr}  {marker}",601+i*22)
    pg.display.flip()

async def app(args):
    import pygame as pg
    cfg=dict(CONFIG_DEFAULT)
    path=Path(__file__).with_name("config.json")
    if path.exists():cfg.update(json.loads(path.read_text()))
    pg.init();pg.joystick.init()
    s=Run(args,cfg)
    if args.inspect:
        s.msg("Conecte receptor e controle; mova gatilhos e analogo. Ctrl+C sai.")
        while True:
            pg.event.pump();s.find_pad(pg)
            if s.pad:
                p=s.pad
                print("EIXOS",[(i,round(p.get_axis(i),2)) for i in range(p.get_numaxes())],
                      "BOTOES",[i for i in range(p.get_numbuttons()) if p.get_button(i)],flush=True)
            await asyncio.sleep(0.6)
    screen=pg.display.set_mode((880,825));pg.display.set_caption("Shell RC Pro - Xbox 360 | Multicarros")
    fonts=(pg.font.SysFont("DejaVu Sans",27,bold=True),pg.font.SysFont("DejaVu Sans",19))
    worker=asyncio.create_task(s.ble_loop());last=time.monotonic()
    try:
        while s.running:
            now=time.monotonic();dt=min(now-last,.15);last=now
            pg.event.pump();s.find_pad(pg)
            for e in pg.event.get():
                if e.type==pg.QUIT:s.running=False
                if e.type==pg.KEYDOWN:
                    if pg.K_1 <= e.key <= pg.K_9:
                        s.select_car(e.key-pg.K_1)
                    if e.key==pg.K_r:
                        s.refresh_cars()
                    if e.key==pg.K_q:s.running=False
                    if e.key==pg.K_F5:s.request_fast_reconnect()
                    if e.key==pg.K_ESCAPE:s.stop("DESARMADO por ESC")
                    if e.key==pg.K_SPACE:
                        s.arm_asked=True;s.neutral_start=None;s.msg(f"Aguardando neutro por {cfg.get('arm_neutral_seconds',.65):.2f} s")
                    if e.key==pg.K_b:
                        s.toggle_turbo()
                    if e.key==pg.K_d:
                        s.toggle_drive_mode()
                    if e.key==pg.K_l:
                        s.set_light_mode("steady")
                    if e.key==pg.K_x:
                        s.set_light_mode("fade")
                    if e.key==pg.K_y:
                        s.set_light_mode("strobe")
                if e.type==pg.MOUSEBUTTONDOWN and e.button==1:
                    x,y=e.pos
                    if 601 <= y < 601+22*min(len(s.cars),9):
                        s.select_car((y-601)//22)
                if e.type==pg.JOYBUTTONDOWN and s.pad and e.instance_id==s.pad.get_instance_id():
                    # LB e tratado exclusivamente pelo estado lido a cada frame.
                    # Nao misturar eventos atrasados com a leitura corrente do botao.
                    if e.button==cfg["lb_brake_button"]:
                        pass
                    elif e.button==cfg.get("back_button",6):
                        s.request_fast_reconnect()
                    elif e.button==cfg["start_button"]:
                        if s.armed:s.stop("DESARMADO por START")
                        else:s.arm_asked=True;s.neutral_start=None;s.msg("START: aguardando neutro")
                    elif e.button==cfg["turbo_button"]:
                        s.toggle_turbo()
                    elif e.button==cfg["rb_mode_button"]:
                        s.toggle_drive_mode()
                    elif e.button==cfg["lights_button"]:
                        s.set_light_mode("steady")
                    elif e.button==cfg["fade_button"]:
                        s.set_light_mode("fade")
                    elif e.button==cfg["strobe_button"]:
                        s.set_light_mode("strobe")
                # JOYBUTTONUP do LB nao altera a tracao:
                # somente a leitura atual do controle pode liberar o freio.
                if e.type==pg.JOYDEVICEREMOVED and s.pad and e.instance_id==s.pad.get_instance_id():
                    s.pad=None;s.stop("CONTROLE DESCONECTADO: desarmado")
            s.read_pad(dt)
            paint(pg,screen,fonts,s)
            await asyncio.sleep(.025)
    finally:
        s.stop("Encerrando; enviando parada se Bluetooth estiver ativo")
        s.running=False
        try:await asyncio.wait_for(worker,timeout=3)
        except asyncio.TimeoutError:
            worker.cancel();await asyncio.gather(worker,return_exceptions=True)
        pg.quit()

if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("--live",action="store_true",help="Envia comandos para a Ferrari real")
    ap.add_argument("--inspect",action="store_true",help="Exibe indices dos eixos e botoes do controle")
    ap.add_argument("--address",help="Endereco Bluetooth da Ferrari original")
    ap.add_argument("--pwm-address",help="Endereco Bluetooth da controladora PWM adicional")
    ap.add_argument("--hybrid-live",action="store_true",help="PWM real somente apos vistoria fisica documentada e firmware instalado")
    args = ap.parse_args()
    if args.hybrid_live:
        if args.live:
            ap.error("Use somente --hybrid-live, sem --live")
        from HW_PWM.hybrid_ble import validate_approval
        try:
            validate_approval()
        except RuntimeError as err:
            ap.error(str(err))
    try: asyncio.run(app(args))
    except KeyboardInterrupt: pass
