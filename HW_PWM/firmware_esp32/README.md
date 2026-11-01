# Shell PWM 488 — Controle proporcional da tracao
**Estado 30/10/2026:** firmware-fonte ESP32 criado; modulo fisico NAO instalado, NAO gravado e NAO ensaiado. Sem conectores/soldas novos.

## Projeto
Notebook Linux + controle Xbox 360:
- Ferrarri Shell original por Bluetooth: direcao, farois; tracao original desabilitada no modo hibrido.
- Placa ESP32 adicional por BLE: recebe comandos de potencia, gera PWM 20 kHz para ponte H separada e DIMENSIONADA.
- Motor traseiro amarelo/vermelho só poderá conectar à ponte NOVA quando totalmente isolado eletricamente do U6 original.

## Recursos existentes no codigo
RT/LT proporcional; RB esporte/precisao (modo direto/pulsos original); B libera turbo;
LB interrompe tracao por COAST e preserva a direcao no pacote original;
rampa suave, intertravamento de reversao, watchdog 250ms, desarme e verificacao de pacotes CRC.
Os GPIO 25/26 no sketch sao apenas referencias de projeto, NAO instrucoes de solda.

## PASSOS BLOQUEADORES — requerem verificacao fisica
1. Registrar fotos completas do verso da placa Shell (face das trilhas) com U6 e solda dos fios amarelo/vermelho.
2. Multimetro, com bateria DESCONECTADA: continuidade entre saidas do motor e U6; identificar conexoes e garantir separacao futura completa.
3. Medir bateria, corrente normal e corrente de partida/bloqueio do motor com equipamento apropriado e limites de corrente.
4. Dimensionar driver de potencia e alimentacao regulada do ESP32; nao presumir corrente do DRV8833.
5. Projeto de montagem mecanica e revisao dos fios, protecao contra curto, bateria preservada.
6. Compilar sketch em Arduino ESP32 3.x, instalar firmware e testar com carga de bancada antes de ligar o motor.
7. Testar aquecimento, perda de BLE, aceleração e reversao com rodas suspensas.
8. Habilitar modo real somente apos vistoria e relatorio. O arquivo INSTALLATION_APPROVAL.json nao existe por seguranca.

## Comandos
Teste offline: `cd ~/Shell_RC_Pro_Xbox360/HW_PWM && ./verificar_instalacao.sh`.
Sketch: `firmware_esp32/shell_pwm_esp32/shell_pwm_esp32.ino`.
Modo real previsto (bloqueado): `cd ~/Shell_RC_Pro_Xbox360 && ./run.sh --hybrid-live`.
O codigo nao prova seguranca de uma instalacao fisica, nem autentica a origem BLE; controle de acesso ao BLE e um requisito adicional antes de operar em area compartilhada.

## Compilacao confirmada no notebook — 30/10/2026
- Arduino CLI v1.5.1 e ESP32 Core v3.3.12 instalados em HW_PWM/toolchain.
- Sketch compilou com sucesso para esp32:esp32:esp32 (ESP32 generico).
- 1089790 bytes de flash (83%) e 41572 bytes RAM global (12%).
- Conexao Bluetooth GATT original detectada: SL-488 Challenge Evo.
- Servicos observados FFF0/FFF1/FFF2, 6936/FE01 e 180F/2A19.
- Bateria reportada 56%. Nenhuma escrita de comando de movimento nesses testes.
- Firmware do ESP32 adicional nao foi gravado: nenhum ESP32 separado detectado em USB.
- Firmware do chip YC1323 original permanece inteiramente inalterado.
- O codigo compilar NAO torna o circuito fisico comprovado; exige vistoria, dimensoes, alimentacao, driver, isolamento do U6 e testes.
