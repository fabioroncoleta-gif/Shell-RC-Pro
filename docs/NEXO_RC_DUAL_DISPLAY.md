> Documento histórico de uma etapa intermediária. Para o comportamento oficial v1.4.19, consulte [GUIA_ATUAL.md](GUIA_ATUAL.md).

> TELA ABORTADA em 20/11/2026. Documento histórico; o firmware ativo 1.4.3 não utiliza LCD. Consulte NEXO_RC_HEADLESS.md.

# NEXO RC — duas entradas e painel paralelo

Firmware 1.4.2: jogador 1 pelo receptor USB Xbox 360; jogador 2 pelo Xbox Bluetooth. Os jogadores têm roteadores e modelos de condução separados. START em neutro libera cada jogador; perda de conexão exige nova liberação. Não injete comandos de aceleração para validar conexão.

## Estado de validação
Compilação ESP32/Bluepad32: aprovada. A gravação tem verificação independente por esptool. A conexão Bluetooth e a F80 foram observadas ativas antes desta atualização. A enumeração USB estava instável e precisa ser verificada pelo novo campo ENUM_ERR. O LCD ainda não foi validado fisicamente: há alimentação, mas faltam sinais. Este firmware não é marcado como versão estável validada de dois carros.

## Tela
Driver em src/rc_display.h. Perfil write-only ILI9341/DCS, RGB565, paisagem 320x240. O controlador do LCD fotografado ainda não foi identificado eletricamente. Uma tela com controlador diferente pode precisar de outro perfil de inicialização. Sem touch e sem acesso ao SD nesta etapa.

O painel mostra controles e carros separadamente, liberação, gatilhos e direção. Os cartões são CARRO 1 e CARRO 2, não prometem uma identidade que possa mudar durante a seleção. Barras são gatilhos reais, não velocidade medida.

DISPLAY ON inicializa o painel apenas depois da montagem dos sinais. Antes da inicialização, ambos os jogadores são desarmados. DISPLAY OFF libera o barramento. DISPLAY informa configuração e maior tempo de atualização. A saída fica OFF após reiniciar, até validar o hardware. Não há framebuffer de 153 kB; campos mudados são desenhados separadamente.

## Montagem: LCD -> ESP32
LCD_D0 -> GPIO13 / D13
LCD_D1 -> GPIO27 / D27 (pino isolado do shield azul)
LCD_D2 -> GPIO14 / D14
LCD_D3 -> GPIO25 / D25
LCD_D4 -> GPIO22 / D22
LCD_D5 -> GPIO32 / D32
LCD_D6 -> GPIO33 / D33
LCD_D7 -> GPIO21 / D21
LCD_WR -> GPIO26 / D26
LCD_RS -> GPIO4 / D4
LCD_CS -> GPIO15 / D15
LCD_RD -> 3V3 (somente escrita)
LCD_RST -> 3V3 (reset por comando; não ligar ao EN)
SD_SS -> GPIO16 / RX2 (pino isolado do shield azul, mantido alto)
Alimentação e GND existentes pela pilha permanecem. Use nomes dos sinais na tela, não os números Arduino como números da ESP32. Faça a montagem sem alimentação. Nenhum GPIO da ESP32 recebe 5 V.

## Backup
Versão anterior à integração do LCD e logs preservados em /home/fabio-roncoleta/Backups/ESP32_Shell_RC_Pro/20261120_display.
Última versão de um carro validada antes do modo duplo: firmware/versions/v1.3.5-low-latency.
O monitor serial contínuo evita reabrir a porta durante diagnósticos. Comandos podem ser enviados pelo FIFO diagnostics/live_dual_commands.fifo.
