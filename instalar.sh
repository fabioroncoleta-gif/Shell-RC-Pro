#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
python3 -m venv .venv
.venv/bin/python -m pip install --disable-pip-version-check -r requirements.txt
if [ ! -f config.json ]; then cp config.example.json config.json; fi
.venv/bin/python -m unittest discover -p 'test_*.py' -q
echo "Instalacao concluida; rode ./run.sh ou ./run.sh --live"
