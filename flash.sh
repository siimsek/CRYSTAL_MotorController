#!/usr/bin/env bash
# STM32F030C8T6 flash: PC'ye takili programciya gore yol secer.
#   - Nucleo / onboard ST-Link V2-1 / V3  -> eski yol: pio run -t upload
#   - Bagimsiz ST-Link V2 USB (0483:3748) -> OpenOCD SWD, sonra st-flash
# Ikisi de takiliysa veya tespit belirsizse sirayla dener.
set -euo pipefail
cd "$(dirname "$0")"

FLASH_ADDR="0x08000000"
BIN="firmware/MotorKontrol.bin"
STLINK_USB_RE='0483:(3744|3748|374b|374d|374e|374f|3752|3753|3754|3755|3757)'
PIO_OOCD="${HOME}/.platformio/packages/tool-openocd/bin/openocd"
PIO_OOCD_SCRIPTS="${HOME}/.platformio/packages/tool-openocd/openocd/scripts"

die() { echo "HATA: $*" >&2; exit 1; }

find_pio() {
  if command -v pio >/dev/null 2>&1; then
    command -v pio
  elif [ -x .venv/bin/pio ]; then
    echo .venv/bin/pio
  else
    return 1
  fi
}

usb_has() {
  command -v lsusb >/dev/null 2>&1 || return 1
  lsusb | grep -Eiq "$1"
}

echo "=== 1/3: Temiz derleme (Build) ==="
rm -rf .pio/build firmware
./build.sh
test -f "$BIN" || die "$BIN uretilemedi"

echo "=== 2/3: Otomatik Dogrulama Kontrolleri (Host + Stage 1-4 + Arayuz Kilidi) ==="
./tools/verify_host.sh

echo "=== 3/3: Programci tespiti ve yukleme ==="
HAS_V2=0
HAS_ONBOARD=0
if command -v lsusb >/dev/null 2>&1; then
  echo "--- USB ---"
  lsusb | grep -Ei '0483:37' || true
  usb_has '0483:3748' && HAS_V2=1
  usb_has '0483:(374b|374d|374e|374f|3752|3753|3754|3755|3757)' && HAS_ONBOARD=1
  if [ "$HAS_V2" -eq 0 ] && [ "$HAS_ONBOARD" -eq 0 ]; then
    if ! lsusb | grep -Eiq "$STLINK_USB_RE"; then
      die "ST-Link USB bulunamadi. Bagimsiz V2, Nucleo onboard veya V3 takin."
    fi
  fi
fi

if command -v st-info >/dev/null 2>&1; then
  echo "--- ST-Link probe ---"
  st-info --probe || true
fi

if [ "$HAS_V2" -eq 1 ] && [ "$HAS_ONBOARD" -eq 0 ]; then
  METHOD=standalone_v2
  echo "Tespit: bagimsiz ST-Link V2 USB -> OpenOCD / st-flash"
elif [ "$HAS_ONBOARD" -eq 1 ] && [ "$HAS_V2" -eq 0 ]; then
  METHOD=pio_upload
  echo "Tespit: Nucleo/onboard ST-Link -> pio upload (eski yol)"
else
  METHOD=auto
  echo "Tespit belirsiz veya birden fazla programci -> sirayla denenecek"
fi

openocd_major() {
  "$1" --version 2>&1 | sed -n 's/.*Open On-Chip Debugger \([0-9]*\)\..*/\1/p' | head -n1
}

flash_pio() {
  local pio
  pio="$(find_pio)" || return 1
  echo "pio run -e motor_kontrol -t upload"
  "$pio" run -e motor_kontrol -t upload
}

flash_openocd() {
  local oocd="$1"
  local scripts="$2"
  local major
  local -a args
  test -x "$oocd" || return 1
  test -d "$scripts" || return 1
  major="$(openocd_major "$oocd")"
  args=(-s "$scripts" -f interface/stlink.cfg)
  if [ "${major:-0}" -ge 12 ]; then
    args+=(-c "transport select swd")
    echo "OpenOCD: $oocd (0.12+ SWD, reset_config none)"
  else
    echo "OpenOCD: $oocd (0.11 HLA, reset_config none)"
  fi
  if [ -n "${STLINK_SERIAL:-}" ]; then
    args+=(-c "adapter serial ${STLINK_SERIAL}")
    echo "STLINK_SERIAL=${STLINK_SERIAL}"
  fi
  args+=(-f target/stm32f0x.cfg
        -c "reset_config none; adapter speed 1000"
        -c "program ${PWD}/${BIN} ${FLASH_ADDR} verify reset exit")
  "$oocd" "${args[@]}"
}

flash_stflash() {
  local -a args
  command -v st-flash >/dev/null 2>&1 || return 1
  args=(--flash=64k --reset)
  if [ -n "${STLINK_SERIAL:-}" ]; then
    args+=(--serial "${STLINK_SERIAL}")
  fi
  echo "st-flash ${args[*]} write ${BIN} ${FLASH_ADDR}"
  st-flash "${args[@]}" write "$BIN" "$FLASH_ADDR"
}

try_pio() {
  echo "--- Deneniyor: PlatformIO upload (eski yol) ---"
  flash_pio && { echo "Yukleme tamam (pio upload)."; exit 0; }
  echo "pio upload basarisiz."
}

try_openocd() {
  echo "--- Deneniyor: PlatformIO OpenOCD (ST-Link V2 USB) ---"
  if flash_openocd "$PIO_OOCD" "$PIO_OOCD_SCRIPTS"; then
    echo "Yukleme tamam (PlatformIO OpenOCD)."
    exit 0
  fi
  echo "PlatformIO OpenOCD basarisiz; sistem OpenOCD deneniyor..."
  if command -v openocd >/dev/null 2>&1 && flash_openocd "$(command -v openocd)" /usr/share/openocd/scripts; then
    echo "Yukleme tamam (sistem OpenOCD)."
    exit 0
  fi
}

try_stflash() {
  echo "--- Deneniyor: st-flash ---"
  flash_stflash && { echo "Yukleme tamam (st-flash)."; exit 0; }
}

case "$METHOD" in
  pio_upload)
    try_pio
    try_openocd
    try_stflash
    ;;
  standalone_v2)
    try_openocd
    try_stflash
    try_pio
    ;;
  *)
    try_pio
    try_openocd
    try_stflash
    ;;
esac

cat >&2 <<'EOF'

ST-Link USB goruluyor ama MCU SWD yanit vermiyor.
Kontrol edin:
  - SWDIO -> PA13
  - SWCLK -> PA14
  - GND   -> GND
  - 3.3V  -> hedef 3.3V
  - NRST  -> MCU NRST (opsiyonel)
Iki ST-Link takiliysa birini cikarin veya:
  STLINK_SERIAL=<seri> ./flash.sh
EOF
die "Yukleme basarisiz."
