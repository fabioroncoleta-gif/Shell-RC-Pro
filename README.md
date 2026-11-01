# Shell RC Pro — Ferrari Shell + controle Xbox 360

Aplicativo experimental em Python para controlar miniaturas **Shell Racing Legends** Bluetooth Low Energy (BLE) pelo controle Xbox 360 conectado ao Linux. Desenvolvido e testado no Ubuntu 24.04.

> **Estado:** versao de referencia de 31/10/2026. Os 60 testes automatizados foram aprovados localmente. A integracao fisica, especialmente com diferentes modelos de Ferrari e recuperacao de falhas BLE, exige testes de bancada.

## Recursos

- Busca e selecao de multiplas Ferraris Shell BLE, com escolha pela interface (teclas 1–9).
- Conexao GATT em caracteristica FFF1 para modelos compativeis.
- RT: acelerar; LT: marcha re, com limite aproximado de 50% e triplo acionamento de LT para liberar o modo completo.
- LB: corta tracao e preserva o comando de direcao; RB: alterna modo de tracao (somente desarmado).
- B: turbo; A/X/Y: modos de farol; START: armar/desarmar.
- BACK/Select: reset rapido e reconexao prioritaria com a mesma Ferrari. Atalho F5.
- Watchdog de entrada Xbox e timeout de transmissoes BLE.
- Modo de simulacao (padrao) sem enviar comandos reais.

**Limites tecnicos:** a placa Shell original oferece comandos digitais. Redução de potencia por pulsos e uma aproximacao temporal, nao controle proporcional eletrico verdadeiro. O projeto PWM ESP32 em `HW_PWM/` e experimental e depende de validacao eletrica; nao esta integrado automaticamente ao carrinho.

## Instalar (Ubuntu 24.04 / Python 3.12)

```bash
git clone https://github.com/fabioroncoleta-gif/Shell-RC-Pro.git
cd Shell-RC-Pro
./instalar.sh
```

## Executar

```bash
./run.sh             # simula, NAO move o carro
./run.sh --live      # envia comandos a uma Ferrari real
```

O instalador cria `config.json` a partir de `config.example.json`. O arquivo de configuracao real, o cadastro `fleet.json`, enderecos BLE pessoais e logs **nao sao versionados**.

Para escolher um carrinho, ligue-o e use as teclas 1–9 na interface; R faz nova busca. O programa so envia comandos depois de conectar, verificar FFF1 e voce armar com START. Cada modelo requer validacao de compatibilidade fisica.

## Testes

```bash
.venv/bin/python -m unittest discover -p 'test_*.py' -q
```

Os testes de BLE usam simulacoes; `test_xbox_buttons.py` pode ser rodado manualmente para exercitar o receptor Xbox real.

## Seguranca

**Teste inicial com as rodas suspensas.** O botao BACK nao faz corte eletrico: se o brinquedo perder o controle, desligue-o fisicamente. Nao teste na rua. Nao use o modo PWM hibrido sem inspecao e isolamento eletrico do driver do motor.

## Estrutura

- `shell_rc_xbox360.py`: programa principal e comunicacao Bluetooth.
- `motor_profile.py`: tratamento de movimento, rampas e limites.
- `reverse_safety.py`: desbloqueio de marcha re via LT x3.
- `car_discovery.py`: descoberta, selecao e historico de carrinhos.
- `light_effects.py`: farois.
- `HW_PWM/`: projeto experimental de motor PWM.
- `test_*.py`: testes de regressao.

Este repositório e uma copia limpa do projeto local. Os backups do computador e os pacotes do Arduino/ESP32 nao foram incluidos.
