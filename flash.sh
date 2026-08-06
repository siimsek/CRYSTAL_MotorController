#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
echo "=== 1/3: Temiz derleme (Build) ==="
rm -rf .pio/build firmware
./build.sh

echo "=== 2/3: Otomatik Dogrulama Kontrolleri (Stage 1-4 & Arayuz Kilidi) ==="
./tools/check_all_stages.sh
python3 tools/check_ui_lock.py

echo "=== 3/3: Karta Yükleme (Flash) ==="
if command -v pio >/dev/null 2>&1; then
  PIO=pio
elif [ -x .venv/bin/pio ]; then
  PIO=.venv/bin/pio
else
  echo "HATA: pio bulunamadi" >&2
  exit 1
fi
"$PIO" run -e motor_kontrol -t upload
