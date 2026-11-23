> Documento histórico de uma etapa intermediária. Para o comportamento oficial v1.4.19, consulte [GUIA_ATUAL.md](GUIA_ATUAL.md).

# NEXO RC 1.4.3 — sem tela

A tela foi abortada a pedido do usuário. O driver LCD foi removido do sketch e os GPIO16 e GPIO27 foram liberados como entradas. Os comandos DISPLAY e a biblioteca gráfica não participam deste firmware. A interface do USB Host MAX3421E foi preservada, assim como o suporte ao segundo jogador pelo Bluetooth interno.

Retirar a placa vermelha e os dois fios dela, com o conjunto desligado. Conservar as ligações do USB Host: SCK18, MISO19, MOSI23, SS5 e INT17; alimentação 5V/3V3, RESET3V3 e GND existentes.

O diagnóstico USB limpa ENUM_ERR quando a enumeração tem sucesso, evitando mostrar erro antigo junto de RECEIVER=1. START físico em neutro continua obrigatório para liberar cada jogador. Não há liberação automática de motor após reinicialização.

O backup anterior, os logs de gravação e a verificação ficam no diretório indicado em diagnostics/headless_backup_path.txt. A validação atual fica em firmware/versions/v1.4.3-dual-headless/validation.json após gravação.
