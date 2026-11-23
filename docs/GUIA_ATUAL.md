# Guia atual — Shell RC Pro v1.4.19

Revisado em 23/11/2026 com base no código da tag v1.4.19. Validação física registrada em 21/11/2026. Esta revisão não altera o firmware.

## Ligações da central

| MAX3421E | ESP32 clássico |
| --- | --- |
| SCK | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| SS / CS | GPIO5 |
| INT | GPIO17 |

Conservar a alimentação e o terra da montagem validada, respeitando as tensões da placa MAX3421E utilizada. O receptor Xbox 360 fica na USB do MAX3421E. O outro Xbox usa Bluetooth nativo. O LCD foi removido e GPIO16/GPIO27 estão livres como entradas. Este firmware usa MAX3421E; CH376A e NRF24 não substituem esse componente nesta versão.

## Ligar e usar

1. Alimente a ESP32 por USB e ligue os controles e carrinhos. A central aguarda entradas válidas dos controles para organizar a descoberta dos carros; não depende de uma sequência manual fixa.
2. A atribuição dos canais considera a chegada de dados válidos dos controles. Cada carro exige conexão GATT, FFF1 gravável e neutro inicial para ficar READY.
3. Solte RT/LT, centralize a direção e solte START/LB/BACK. Com o carro READY e dados válidos, 650 ms em neutro permitem armar automaticamente.
4. RT avança; LT dá ré. Soltar o gatilho gera neutro, sem rampa de parada proposital. Ambos pressionados não acionam tração.
5. Após reconexão, mantenha novamente neutro para permitir a liberação automática. A tentativa de recuperação não garante alcance ou prazo fixo de reconexão.

START também permite armar/desarmar, mas não bloqueia permanentemente a liberação automática. BACK desarma; quando usado pelo controle atribuído ao primeiro canal e esse carro está READY, também solicita desconexão e nova busca/reconexão. O segundo canal não tem a mesma ação BACK de reconexão explícita; possui recuperação própria do enlace.

## Funções preservadas e limites

- Frente e ré contínuas sem modulação por pulsos; RB não muda o modo do motor.
- Tração binária acima do limiar de cerca de 13% do gatilho; não é aceleração proporcional física. Ré sem limite de 50% e sem sequência de três toques.
- B alterna turbo; A alterna farol fixo; X alterna fade; Y alterna strobo. A resposta luminosa depende dos recursos do carrinho.
- LB envia neutro no motor e conserva o comando de direção capturado ao apertar. Ao soltar, retoma no mesmo sentido se o gatilho continuar acionado. Se o sentido foi invertido durante LB, exige soltar os gatilhos antes de prosseguir.
- Inversão direta de sentido inclui uma janela de neutro de 160 ms. LB não alterna frente/ré e não bloqueia mecanicamente o eixo.
- O modo sem tela preserva os dois controles e os dois canais BLE.

## Comunicação e diagnóstico

Serial USB a 115200 baud. O banner esperado é `VERSION=1.4.19_USB360_FAST_POLL`. Os eventos USB, Bluetooth, atribuição e estado dos carros ajudam a separar falha de entrada e falha BLE.

A v1.4.18 passou a supervisionar a ligação de rádio do Xbox 360 sem confundir ausência de mudança nos gatilhos com perda de sinal. A v1.4.19 consulta somente o endpoint de entrada do slot pareado e distribui consultas dos demais slots quando procura controle. Consulta de status do slot ativo ocorre aproximadamente a cada segundo; status de rádio sem atualização por mais de 2300 ms desabilita a entrada USB. O roteador possui ainda watchdog de dados de 550 ms, alimentado pelo acompanhamento válido de rádio USB. Esses tempos não são uma garantia de distância ou de parada física do carrinho sem enlace BLE.

## Backup e compilação

Código completo: `esp32_multicar/`, incluindo `src/`, `tests/` e a dependência modificada `vendor/USB_Host_Shield_2.0/`. As cópias de fontes na pasta da versão contêm apenas os arquivos selecionados na ocasião; para recompilar use a árvore completa da tag v1.4.19.

```bash
arduino-cli compile --fqbn esp32-bluepad32:esp32:esp32 \
  --libraries esp32_multicar/vendor esp32_multicar
sha256sum -c firmware/versions/v1.4.19-usb360-fast-poll/SHA256.txt
```

Pacote de placa utilizado: esp32-bluepad32 4.1.0. Binário oficial: `firmware/versions/v1.4.19-usb360-fast-poll/esp32_multicar.ino.bin`. SHA256: `35ba6179b2bd375ac3bf0a6f849374a807113d91d6a9b8819af65c6feeac3e87`.

O binário é um aplicativo para offset `0x10000`, não uma imagem completa com bootloader/partições. Preservar o esquema compatível e os pareamentos; não apagar a flash inteira. A tag v1.4.18 e os pacotes anteriores ficam disponíveis para retorno. Backups intermediários são históricos e não recebem a condição de versão oficial validada por estarem arquivados.
