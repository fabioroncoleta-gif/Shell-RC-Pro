# ESP32 Multicar — oficial v1.4.19

Firmware autônomo para Xbox 360 USB/MAX3421E e Xbox Bluetooth, com dois carrinhos Shell/Ferrari BLE. Validado pelo proprietário em 21/11/2026; documentação revisada em 23/11/2026.

- [Guia completo e comandos atuais](../docs/GUIA_ATUAL.md).
- [Resumo do projeto](../README.md).
- [Binário e checksum oficiais](../firmware/versions/v1.4.19-usb360-fast-poll/).
- [Manifesto da versão validada](../firmware/CURRENT_VALIDATED.json).

O primeiro controle com entrada válida recebe o primeiro canal de carro; o segundo recebe o outro. Carro READY e entrada em neutro por 650 ms permitem armar automaticamente. START não é obrigatório nesse modo. Frente/ré são digitais contínuas, sem pulsos de motor e sem limite de ré de 50%. LB preserva o comando de direção e põe o motor em neutro. A tela não faz parte desta versão.

## Compilar

Arduino CLI e esp32-bluepad32 4.1.0:

```bash
arduino-cli compile --fqbn esp32-bluepad32:esp32:esp32 \
  --libraries esp32_multicar/vendor esp32_multicar
```

Execute na raiz do repositório. `src/` contém a lógica e os enlaces; `vendor/` contém USB Host Shield 2.0 com as alterações necessárias para o Xbox 360; `tests/` contém verificações C++ sem transmissão aos carros. A pasta do pacote de firmware não substitui esta árvore completa para recompilação.

A v1.4.19 preserva a correção de sinal sustentado da v1.4.18 e reduz consultas USB desnecessárias. Os resultados de revisão documental estão em [VERIFICACAO_2026-11-23.md](../docs/VERIFICACAO_2026-11-23.md). Testes nativos não substituem validação física de rádio, alcance ou compatibilidade de outros carrinhos.
