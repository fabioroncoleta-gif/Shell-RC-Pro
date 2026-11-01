#!/usr/bin/env bash
set -o pipefail
cd "$(dirname "$0")" || exit 1
if [ ! -x .venv/bin/python ]; then
  echo "Dependencias ausentes. Execute ./instalar.sh"
  exit 1
fi
if [ ! -f config.json ]; then
  echo "Criando configuracao local a partir do modelo"
  cp config.example.json config.json
fi
echo "=== Shell RC Pro: $(date '+%Y-%m-%d %H:%M:%S') ===" >> shell_rc_pro.log
.venv/bin/python -u shell_rc_xbox360.py "$@" 2>&1 | tee -a shell_rc_pro.log
exit ${PIPESTATUS[0]}
