# ESP32 standalone — versão validada 1.2.2

Xbox Wireless Bluetooth → ESP32/Bluepad32 → Ferrari Shell 488 BLE.
Validado por Fábio em 01/11/2026: conexão simultânea, comandos mantidos,
direção, botões e funcionamento do conjunto. Firmware instalado e hash
verificado. O código está fixado para o Xbox e a Ferrari deste projeto.

## Usar sem notebook

1. Alimente a ESP32 pela entrada USB, com fonte USB estável de 5 V ou power bank.
2. Ligue o controle Xbox e a Ferrari. Os pareamentos ficam na memória da ESP32.
3. Mova o analógico para produzir o primeiro relatório válido. A placa busca a Ferrari.
4. Solte os gatilhos, centralize a direção, pressione START e mantenha neutro por 650 ms.
5. RT: frente; LT: ré; analógico esquerdo: direção; LB: freio; B: turbo;
   RB: direto/pulsos; A/X/Y: faróis; BACK: reconectar Ferrari; START: desarmar.

O notebook é necessário apenas para gravação e painel/log USB.
O ciclo físico de desligar e ligar em fonte externa ainda deve ser confirmado.
Alguns power banks desligam por consumo baixo; a alimentação deve permanecer ativa.

## Proteção para comandos mantidos

A classificação HID pode ser NONE ao conectar; só relatórios válidos de gamepad
liberam entrada. A placa mantém o último comando enquanto a supervisão BLE de
400 ms estiver confirmada, o Xbox estiver reconhecido e o thread BTstack emitir
heartbeat. Perda de link/supervisão/heartbeat deixa de renovar a entrada;
o watchdog de 550 ms desarma. Não é um prazo total garantido de parada.
A recuperação de HCI não desconecta um Xbox registrado aguardando o primeiro relatório.

## Compilar

- Arduino CLI com pacote esp32-bluepad32 4.1.0 e placa ESP32 genérica.
- Biblioteca USB Host Shield Library 2.0, versão 1.7.0.
- O MAX3421E é opcional e não é usado pelo Xbox Bluetooth validado.

```bash
arduino-cli compile --fqbn esp32-bluepad32:esp32:esp32 esp32_ferrari_standalone
```

## Restaurar a versão validada em uma placa já preparada

Firmware: `../firmware/validated/v1.2.2/firmware_validado_v1.2.2.bin`.
Feche o monitor serial antes de gravar. Use ambiente com esptool instalado.

```bash
cd firmware/validated/v1.2.2
sha256sum -c SHA256SUMS
python3 -m esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 write-flash 0x10000 firmware_validado_v1.2.2.bin
```

É um backup do aplicativo, para a ESP32 com bootloader/partições compatíveis.
Não execute erase-flash: preserva-se a área NVS usada pelos pareamentos.
O backup integral original permanece na máquina do proprietário.

## Testes de lógica

```bash
for name in router drive held_input; do
  g++ -std=c++17 -Wall -Wextra esp32_ferrari_standalone/tests/${name}_test.cpp -o /tmp/shell_${name}_test
  /tmp/shell_${name}_test
done
```

Faça qualquer teste inicial com as rodas suspensas.
