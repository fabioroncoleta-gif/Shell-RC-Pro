> Documento histórico de uma etapa intermediária. Para o comportamento oficial v1.4.19, consulte [GUIA_ATUAL.md](GUIA_ATUAL.md).

# NEXO RC 1.4.4 — motor contínuo, sem pulsos

Removida a modulação por densidade de pulsos da tração, tanto em frente quanto em ré. RB não altera mais o modo do motor. A limitação de ré a 50% e seu desbloqueio por três toques foram removidos, porque dependiam dos pulsos. Os bytes de frente e ré neste protocolo são binários; um gatilho mantido acima do limiar envia comando contínuo, sem controle de potência por amplitude.

A rampa interna permanece somente como temporização do turbo; não representa controle proporcional de potência do motor. A telemetria duty agora indica o comando efetivo binário (zero ou um). Turbo B, luz A, fade X e strobo Y foram preservados. O LB mantém a direção do servo, corta o motor e permite retomar no mesmo sentido; não alterna frente e ré. A proteção de troca de sentido, a exigência de START em neutro e o corte por perda de conexão permanecem.

A mudança usa o DriveModel compartilhado dos dois jogadores. A tela permanece removida. Os testes cobrem cinco posições de gatilho por sentido, RB repetido, ausência de zeros intercalados, LB, retomada, perda de dados e perda de conexão com o carro. A validação de firmware fica em firmware/versions/v1.4.4-continuous-motor/validation.json depois da gravação.
