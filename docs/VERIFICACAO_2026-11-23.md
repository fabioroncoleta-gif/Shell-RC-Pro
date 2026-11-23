# Verificação documental — 23/11/2026

Firmware oficial preservado: v1.4.19, fonte de referência `17e513c0b398c7deb92272d99246dd1a10b5ef9e`. Não houve nova compilação Arduino, gravação nem teste físico nesta revisão. A validação física de 21/11/2026 está registrada no pacote oficial.

Revisados: README principal, guia atual, instruções iniciais, manifesto oficial, documentação da árvore ESP32 e índice dos backups. Instruções antigas do PC foram preservadas como histórico. Arquivados binários/fontes intermediários disponíveis, sem logs brutos ou diagnósticos locais.

SHA256 do binário oficial confere com o manifesto. Os checksums de todos os binários do arquivo `firmware/versions/SHA256SUMS` foram verificados.

Testes C++ existentes compilados com `g++ -std=c++17 -Wall -Wextra`, em diretório temporário, sem enviar comandos aos carrinhos:

| Teste | Resultado |
| --- | --- |
| auto_arm_motor_end_to_end_test.cpp | PASS |
| auto_neutral_arm_test.cpp | PASS |
| drive_test.cpp | PASS |
| dual_pairing_stress_test.cpp | PASS |
| dual_pairing_test.cpp | PASS |
| dual_rc_runtime_test.cpp | PASS |
| fleet_test.cpp | PASS |
| held_input_test.cpp | PASS |
| no_motor_pulse_test.cpp | PASS |
| router_test.cpp | PASS |
| usb360_hold_policy_test.cpp | PASS |
| usb360_polling_latency_test.cpp | PASS |
| virtual_end_to_end.cpp | PASS |
| xbox_receiver_report_gate_test.cpp | PASS |

Testes nativos verificam lógica de controle, pareamento e supervisão; não comprovam alcance, latência física de ponta a ponta ou compatibilidade de hardware adicional.
