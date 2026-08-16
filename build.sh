#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
  echo "ERROR: x86_64-w64-mingw32-g++ not found"
  exit 1
fi

python3 gen_def.py

x86_64-w64-mingw32-g++ -shared -o EOSSDK-Win64-Shipping.dll \
  main.cpp proxy.def -static
