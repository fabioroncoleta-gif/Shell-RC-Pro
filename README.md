# Shell RC Pro — versão oficial v1.4.19

Versão validada por Fábio em **21/11/2026**: dois controles Xbox e dois carrinhos Shell/Ferrari simultaneamente, com melhora da resposta do Xbox 360. Documentação revisada em 23/11/2026 sem alteração do firmware.

- [Código completo, dependência USB incluída e testes](esp32_multicar/).
- [Guia atual: montagem, comandos e funcionamento](docs/GUIA_ATUAL.md).
- [Firmware oficial e checksum](firmware/versions/v1.4.19-usb360-fast-poll/).
- [Identificação da versão validada](firmware/CURRENT_VALIDATED.json).
- [Histórico de backups](firmware/versions/README.md). A v1.4.18 permanece disponível para retorno.

## Conjunto atual

| Componente | Função |
| --- | --- |
| ESP32 clássico com Bluetooth Classic/BLE | Central autônoma alimentada por USB |
| USB Host MAX3421E + receptor Xbox 360 sem fio | Entrada do Xbox 360 |
| Xbox Wireless Bluetooth compatível com Bluepad32 | Segunda entrada, pelo rádio da ESP32 |
| Dois carrinhos Shell/Ferrari BLE compatíveis com FFF1 | Canais de controle independentes; F80 e SF90 XX Stradale usados na validação |

O primeiro controle com dados válidos recebe o primeiro canal de carro; o segundo recebe o outro. A descoberta dos carros, a validação GATT e a recuperação de conexão são independentes. O firmware atual funciona sem tela e sem notebook no caminho dos comandos.

## Comandos da versão oficial

| Comando | Função |
| --- | --- |
| Analógico esquerdo | Direção esquerda/direita digital |
| RT / LT | Frente / ré contínuas, sem pulsos de motor |
| LB mantido | Neutro do motor e preservação do comando de direção durante o acionamento |
| B | Alterna turbo |
| A | Alterna farol fixo |
| X | Alterna efeito fade do farol |
| Y | Alterna strobo |
| RB | Sem função de alternar pulsos no firmware atual |
| START | Comando de armar/desarmar; o modo automático também pode rearmar após neutro |
| BACK | Desarma; no primeiro canal também solicita reconexão BLE |

**Armar automaticamente:** carro READY, entrada válida, gatilhos soltos e direção centralizada por 650 ms. START não é obrigatório para a liberação automática. START e BACK não são travas permanentes: o sistema pode rearmar após novo período em neutro.

**Tração digital:** o pacote BLE exposto pelo projeto liga/desliga frente e ré. Sem pulsos não há velocidade física proporcional ao gatilho nem limite de ré a 50% ou desbloqueio por três toques. A rampa interna auxilia a temporização do turbo; não modula a potência do motor. LB não trava mecanicamente o eixo e preserva o comando, sem sensor de ângulo das rodas.

## Compilar

Arduino CLI, pacote `esp32-bluepad32` 4.1.0 e placa `esp32-bluepad32:esp32:esp32`. A biblioteca USB Host Shield 2.0 modificada está em `esp32_multicar/vendor` e deve ser usada na compilação.

```bash
arduino-cli compile --fqbn esp32-bluepad32:esp32:esp32 \
  --libraries esp32_multicar/vendor esp32_multicar
```

O binário salvo é o aplicativo ESP32, gravado em `0x10000` sobre instalação com bootloader e partições compatíveis. Não é imagem completa para placa vazia. Consulte o guia antes de gravar; a revisão documental não altera a placa nem os pareamentos.

## Histórico do projeto

O aplicativo Python para PC, `esp32_ferrari_standalone/`, os backups v1.2.2/v1.3.x e `HW_PWM/` permanecem preservados. Suas instruções e recursos pertencem às respectivas versões e não definem o comportamento da v1.4.19. O projeto PWM é experimental e separado.

Primeiro teste com rodas suspensas. Em perda de BLE, a central pode não conseguir entregar o pacote de parada; desligar o carro é o corte físico disponível.
