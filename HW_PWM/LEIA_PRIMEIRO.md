# SHELL RC PRO — ETAPA DE HARDWARE PWM
Data: 30/10/2026. ESTADO: INVESTIGACAO + PROTOTIPO VIRTUAL VALIDADO.
Esta pasta NAO modifica a aplicacao de controle ja instalada.
Nenhum GPIO, Bluetooth ou motor do carrinho foi acionado por estes programas.

## Fato confirmado
O protocolo BLE 0xFFF0/0xFFF1 conhecido em projetos GitHub
('csabigee/shell-rc', 'mesailde/ShellRacingRemote', 'whilefalse841/SRL-Car-Controller')
transmite apenas bits ON/OFF para frente/re. Nao temos comando de velocidade
eletrica real. Curva de RT sem pulsos externos nao e possivel nesse protocolo.

## Estudo de hardware
Uma desmontagem de OUTRA Ferrari Bluetooth Bburago/Shell, de 2021,
registrou PCB 'KCE210119', microcontrolador 'YC1063' e duas possiveis pontes H.
Referência: https://www.androidvt.com/blog/index.php?category=35&d=25&entry=entry240325-091926&m=03&y=24
NAO e prova do hardware da 488 CHALLENGE EVO do usuario.

## Componente candidato para estudo (NAO COMPRAR SEM MEDIR)
Texas Instruments DRV8833 (ponte H dupla) ou DRV8837 (ponte H unica).
A tabela IN1/IN2 do DRV8833 admite:
 - 00 -> COAST (motor livre)
 - 10 -> FRENTE
 - 01 -> RE
 - 11 -> BRAKE (frenagem dinamica por curto controlado dos terminais)
 - PWM,0 -> frente com PWM real na ponte, nao pacotes BLE a cada 50 ms
Fonte: https://www.ti.com/lit/ds/symlink/drv8833.pdf
ATENCAO: NAO tente aplicar esses estados na placa atual: pinagem desconhecida.
Suprimento de bateria, corrente de pico, dissipacao e dimensoes AINDA desconhecidos.

## Inspecao visual para identificar a placa real
1. DESLIGAR a Ferrari, desconectar USB, manter longe de materiais condutores.
2. Fotografar exterior e etiqueta/modelo; depois retirar a tampa por parafusos
   sem forcar carenagem nem torcer fios.
3. FOTOGRAFAR FRENTE DA PCB em foco, marcacoes legiveis de todos os CI
   e conectores (especialmente CI junto aos DOIS fios do motor traseiro).
4. FOTOGRAFAR VERSO DA PCB, mesmo que sem componentes: seguir trilhas.
5. FOTOGRAFAR bateria e inscricoes: nao furar, pressionar ou dessoldar celula.
6. FOTOGRAFAR eixo traseiro/caixa de engrenagem e fios do motor traseiro.
7. Fotografar placa ao lado de regua para saber espaco disponivel.
8. Se fios ou bateria estiverem soldados diretamente, NAO desligue com alicate;
   so registre. Nenhuma medicao energizada ainda.

## Decisao apos fotos
A) Reutilizacao: se driver atual permitir PWM e pinos forem identificados,
   investigar isolacao de sinais originais + nova entrada PWM sem conflito.
B) Nova controladora: BLE MCU compacto + ponte H adequada, motor DC e demais
   funcoes de direcao/iluminacao. Pode exigir substituir placa completa.
NAO ligar nova ponte H em paralelo aos terminais do driver antigo.
Somente depois dos dados de bateria/motor/corrente/ponte H projetar ligacao.

## Bancada digital instalada
pwm_motor_core.py = logica apenas, sem acesso a motores.
test_hw_pwm.py = testes de seguranca logica sem eletricidade.
simular_pwm.py = demonstra comandos no tempo em CSV. Nao modela corrente/RPM.
Executar:
  cd ~/Shell_RC_Pro_Xbox360/HW_PWM
  python3 -m unittest -v test_hw_pwm
  python3 simular_pwm.py
  cat RESULTADO_BANCADA.txt

## IMPORTANTE
Frenagem eletrica dinamica pode causar corrente elevada mesmo sem reversao.
O estado 11 funciona PARA O DRV8833 conforme datasheet, mas NAO garante
seguranca termica ou mecanica para a Ferrari.
Proibido qualquer teste real de PWM/freio antes de caracterizar eletrica.
