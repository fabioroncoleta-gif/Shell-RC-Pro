# Shell RC Pro — versão oficial estável v1.4.19 (21/11/2026)

**Versão v1.4.19 confirmada pelo proprietário como funcionando com dois controles e dois carros simultaneamente, com a resposta do Xbox 360 melhorada.** Preserva o pareamento dinâmico (primeiro controle → primeiro carro), a conexão independente dos carros Shell/Ferrari e a condução suave sem pulsos.

| Componente | Interface |
| --- | --- |
| ESP32 clássico | Gateway autônomo, alimentação USB |
| Xbox 360 sem fio | Receptor USB no MAX3421E USB Host Shield, ligado à ESP32 por SPI |
| Xbox Wireless Bluetooth | Bluepad32 / Bluetooth da própria ESP32 |
| Ferrari Shell F80 + SF90 XX Stradale | Dois canais BLE/GATT separados, com característica FFF1 gravável |

A v1.4.18 corrigiu os cortes de aceleração sustentada do Xbox 360 com a supervisão independente de rádio (checagem a cada ~1 segundo, parada segura sem resposta por mais de 2300 ms ou desconexão). A **v1.4.19 oficial** melhora a latência ao consultar apenas o endpoint USB do controle pareado e distribuir a descoberta de controles entre os quatro slots, sem concentrar várias consultas na mesma iteração. O Xbox Bluetooth, a conexão BLE dos carrinhos e a lógica do motor não foram alterados nesta atualização.

**Compilação reprodutível:** Arduino CLI, placa `esp32-bluepad32:esp32:esp32`, biblioteca USB Host Shield 2.0 incluída em `esp32_multicar/vendor`.

```bash
arduino-cli compile -b esp32-bluepad32:esp32:esp32 \
  --libraries esp32_multicar/vendor esp32_multicar
```

**Pacote oficial:** `firmware/versions/v1.4.19-usb360-fast-poll/` contém o `.ino.bin`, `SHA256.txt` e cópias das fontes alteradas. O backup anterior **v1.4.18** permanece na tag Git `v1.4.18`, para retorno. Para auditoria, o binário é gravado no offset ESP32 `0x10000` com o esquema de partições compatível; não apagar a flash inteira nem os pareamentos. A compilação, a gravação e o checksum foram verificados; 10 testes nativos passaram. O proprietário confirmou funcionamento real em 21/11/2026. Testar sempre com rodas suspensas antes do uso.

---

# ESP32 Multicar — v1.3.0

Xbox Bluetooth → ESP32 → miniatura Shell BLE. A versão 1.2.2 da 488
permanece preservada, com firmware e tag próprios para reversão.

## Trocar pelo PC

Abra o painel em http://127.0.0.1:8765/ e atualize a página.
Clique **Buscar**, escolha um carrinho na lista e clique **Conectar**.
A busca/troca desarma a saída. Aguarde READY e pressione START em neutro
para liberar a condução novamente. Não mantenha START pressionado durante a troca.

## Trocar sem notebook

Alimente a ESP32 pela USB. O Xbox conecta primeiro; a descoberta dos
carrinhos ocorre automaticamente depois da conexão HID, mesmo antes do
primeiro movimento do analógico. Só dados válidos permitem armar.

- Direcional esquerdo/direito: seleciona o carrinho anterior/próximo da lista.
- BACK: reconecta e refaz a busca quando existe um carro conectado.
- START: armar/desarmar em neutro; RT/LT, direção, B, LB, RB e A/X/Y mantêm suas funções.
- Após toda troca, solte START e pressione novamente em neutro.

## Descoberta e compatibilidade

Descobre até 8 candidatos com nome SL- ou SHELL, elimina duplicatas e
inclui endereços já conhecidos. Antes de liberar condução, exige FFF1
gravável e transmite um neutro inicial. O pacote continua sendo o protocolo
Shell de 8 bytes usado pelo aplicativo PC; a presença de BLE/FFF1 isolada
não comprova a mesma resposta física em todo modelo.

A última conexão bem-sucedida é lembrada em NVS. Com vários candidatos,
prioriza esse carro; sem preferência disponível, aguarda seleção em vez
de escolher um carro arbitrário. Uma escolha explícita indisponível não
é substituída por outro carrinho. Cada troca bloqueia comandos de movimento,
neutraliza a saída quando possível, desconecta e exige novo START.

## Validação em 01/11/2026

- Firmware compilado, gravado e hash verificado na ESP32.
- Detectou SL-SF-25, verificou FFF1 e conectou em READY com ERR=0.
- Seleção pelo endpoint real do painel testada: desarmar, desconectar,
  buscar e reconectar à SF-25; conexão Xbox preservada.
- Testes C++ de frota, roteamento, condução e comandos mantidos passaram.
- 12 testes Python do painel passaram; JavaScript verificado por node --check.
- Dois carros simultâneos e a resposta física completa da SF-25 ainda
  precisam de validação presencial. A regra de ambiguidade foi testada em simulação.

## Compilar e testar

Arduino CLI, pacote esp32-bluepad32 4.1.0 e USB Host Shield Library 2.0 1.7.0.

```bash
arduino-cli compile --fqbn esp32-bluepad32:esp32:esp32 esp32_multicar
for name in fleet held_input router drive; do
  g++ -std=c++17 -Wall -Wextra esp32_multicar/tests/${name}_test.cpp -o /tmp/multicar_${name}
  /tmp/multicar_${name}
done
```

## Firmware e reversão

Nova versão: ../firmware/versions/v1.3.0/.
Versão anterior validada pelo proprietário: ../firmware/validated/v1.2.2/.
Em placa já preparada, o aplicativo é gravado no offset 0x10000.
Feche o monitor USB e verifique SHA256SUMS antes de usar esptool.
Não use erase-flash, preservando pareamentos. Teste primeiro com rodas suspensas.
