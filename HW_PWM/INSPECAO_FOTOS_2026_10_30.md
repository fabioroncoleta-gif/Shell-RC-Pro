# Ferrari Shell RC - Fotos da eletrônica (30/10/2026)

## Confirmado visualmente
- Placa marcada 36732-BT V2.3, chip YC1323 e dois chips U2 e U6 marcados 2008D / BH-053.
- Placa secundária de LEDs, mecanismo de direção separado, motor/redutor traseiro e célula recarregável.
- Novas imagens mostram vistas do chassi e **face com componentes** da placa.
- Não temos ainda traçado elétrico da outra face, datasheet confiável do 2008D/BH-053, ou medições elétricas do exemplar.

## Hipóteses a validar (não usar para solda)
- U6 pode estar associado à tração por proximidade do par vermelho/amarelo.
- U2 pode estar associado à direção por proximidade de outro feixe de fios.
- O fio vermelho/amarelo provavelmente é um dos pares de motor; a função exata deve ser verificada por continuidade com bateria fisicamente desconectada.
- Não assumir pinagem nem equivalência com DRV8833.

## Decisão de engenharia
Priorizar preservação de Bluetooth, direção e LEDs. PWM real exige controlador e driver elétrico caracterizados; o protocolo BLE atual é binário.
Nenhuma modificação/flash/saída elétrica foi realizada neste estágio. O software HW_PWM permanece isolado e OFFLINE.

## Próxima validação segura
1. Desligar carrinho e tirar de qualquer carregador.
2. Se acessível sem forçar fios ou bateria, fotografar a face **oposta à dos CIs**, com trilhas e pontos de solda.
3. Técnico com multímetro no modo continuidade e bateria desligada identifica pares de motor e trilhas para U2/U6.
4. Só então definir se a solução será módulo PWM isolado ou reaproveitamento da ponte original.
5. Não aplicar PWM, tensão ou frenagem elétrica em U2/U6 por tentativa.

## Atualização confirmada pelo proprietário — chicote traseiro
- Fios **amarelo e vermelho à esquerda** pertencem ao **motor traseiro de tração** (CONFIRMADO pelo proprietário).
- Polaridade dos fios e caminho da placa até U2/U6 **não** identificados.
- U6 continua candidato, não confirmado.
- O software PWM não pode usar essa informação para energizar a placa.

## Confirmação posterior do proprietário
- O **U6** é o acionador associado ao motor traseiro (fios amarelo/vermelho), conforme informação do proprietário.
- Esta confirmação NÃO demonstra quais dos seis pinos são entrada, saída, alimentação ou terra; U2 continua hipótese para direção.
- O código exato do CI é '2008D / BH-053'; não atribuir a ele pinagem de DRV8220/DRV8833/PT6208 só pela semelhança de encapsulamento.
- Sem leitura de pinagem e medições de corrente/tensão, PWM físico continua bloqueado.

## Fotos IMG_1405 a IMG_1407: serigrafia de canais de motor
- Visto na placa 36732-BT: **M2- = fio amarelo; M2+ = fio vermelho**. Par M2 é tração traseira conforme proprietário.
- **M1+/M1-** também visíveis, mas a função específica ainda não foi medida.
- Sinal `+` e `-` é marcação da placa, NÃO tensão fixa do motor ao dar marcha à ré.
- U6 foi associado à tração pelo proprietário, mas não está demonstrada conexão pino-a-pino de U6 para M2.
- Não soldar ponte H nova em M2 mantendo a U6 ligada. Motor precisaria ser isolado do driver original por solução revisada em bancada.
- Bateria, pico de corrente, polaridade e fonte regulada do ESP32 ainda não caracterizados.
