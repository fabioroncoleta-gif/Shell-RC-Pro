#!/bin/bash
set -euo pipefail
cd "$HOME/Shell_RC_Pro_Xbox360/HW_PWM"
echo "SHELL RC PRO - TESTES OFFLINE (NENHUM BLUETOOTH OU MOTOR)"
PYTHONPATH=.. ../.venv/bin/python -m unittest -q test_hw_pwm test_hardware_profile test_hybrid_protocol
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -o /tmp/shell_pwm_safety_test firmware_esp32/test_motor_safety.cpp
/tmp/shell_pwm_safety_test
../.venv/bin/python simular_pwm.py
echo "PASS: simulado; hardware real nao habilitado."
