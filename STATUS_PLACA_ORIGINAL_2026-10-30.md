# Ferrari Shell 488 Challenge Evo — somente placa original

## Estado do projeto
- **Sem hardware adicional**. O programa do notebook controla a Ferrari original via BLE FFF1.
- **Sem regravação de firmware** no chip YC1323. A conexão com a Ferrari NÃO demonstra OTA.
- Características consultadas por inventário *somente leitura*: FFF0/FFF1/FFF2, 6936/FE01, 180F/2A19.
- **Nenhum canal OTA confirmado**. O 6936/FE01 tem funções desconhecidas e NÃO será usado para escrita por tentativa.
- Uma plataforma ESP32 foi compilada separadamente para estudo, mas NÃO é firmware compatível com o YC1323 e NÃO deve ser gravada nele.
- Firmware original preservado.

## Versão operacional via BLE original
- Modo inicial: **DIRETO**, sem pulsos de liga/desliga na tração.
- RT/LT: tração normal binária, marcha à ré com bloqueio de inversão.
- Botão B: solicitação de turbo; a habilitação do bit turbo ocorre após subida do acelerador virtual, com histerese.
- Botão RB: alterna modo DIRETO/PULSOS somente desarmado.
- LB: corta tração com direção preservada (não trava o eixo).
- A/X/Y: faróis fixos/fade/strobo.
- START + gatilhos neutros: armar, ESC/START: desarmar.
- **Limitação incontornável com comandos conhecidos**: tração DIRETA já entrega potência normal plena; a rampa virtual só controla a transição para turbo, não torna a partida progressiva em hardware.
- Use pela primeira vez com rodas suspensas; nenhum teste físico de velocidade/torque foi executado nesta revisão.

## Referências
- Documentação do controle binário da família: https://github.com/csabigee/shell-rc
- Produto YC13xx: https://www.yichip.com/yc13xx

## Arquivos principais
- shell_rc_xbox360.py / config.json: aplicativo original com ajustes.
- test_original_ble_only.py: testes dos dois estágios.
- HW_PWM/BLE_ORIGINAL_OTA_PREFLIGHT.json: inventário BLE seguro; 0 escritas.
- Cópias anteriores: *.V3_backup_before_original_ble_only
