#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

if command -v pio >/dev/null 2>&1; then
  PIO=pio
elif command -v platformio >/dev/null 2>&1; then
  PIO=platformio
else
  if ! command -v python3 >/dev/null 2>&1; then
    echo "HATA: python3 bulunamadi. sudo apt install python3 python3-venv" >&2
    exit 1
  fi
  if [ ! -x .venv/bin/pio ]; then
    python3 -m venv .venv
    .venv/bin/python -m pip install --upgrade pip
    .venv/bin/pip install platformio
  fi
  PIO=.venv/bin/pio
fi

# U8g2 paketini indir, C cekirdegini yerel kutuphane olarak otomatik hazirla.
"$PIO" pkg install -e motor_kontrol >/dev/null
U8SRC="$(find .pio/libdeps/motor_kontrol -type f -path '*/U8g2/src/clib/u8g2.h' -printf '%h\n' 2>/dev/null | head -n1 || true)"
if [ -n "$U8SRC" ]; then
  mkdir -p lib/u8g2/src
  cp -f "$U8SRC"/*.c "$U8SRC"/*.h lib/u8g2/src/
fi

test -f lib/u8g2/src/u8g2.h || { echo "HATA: U8g2 C kutuphanesi hazirlanamadi" >&2; exit 1; }
rm -rf .pio/build
"$PIO" run -e motor_kontrol
mkdir -p firmware
cp .pio/build/motor_kontrol/firmware.elf firmware/MotorKontrol.elf
cp .pio/build/motor_kontrol/firmware.bin firmware/MotorKontrol.bin
[ ! -f .pio/build/motor_kontrol/firmware.hex ] || cp .pio/build/motor_kontrol/firmware.hex firmware/MotorKontrol.hex
"$PIO" run -e motor_kontrol -t size
printf '\nHazir: firmware/MotorKontrol.elf ve firmware/MotorKontrol.bin\n'
