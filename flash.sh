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
LOG_DIR="$(mktemp -d)"
trap 'rm -rf "$LOG_DIR"' EXIT

if [ -t 1 ]; then
  C_OK=$'\033[32m'; C_INFO=$'\033[36m'; C_WARN=$'\033[33m'; C_ERR=$'\033[31m'; C_OFF=$'\033[0m'
else
  C_OK=''; C_INFO=''; C_WARN=''; C_ERR=''; C_OFF=''
fi

die() { echo "HATA: $*" >&2; exit 1; }

pad_right() {
  local value="$1" width="$2"
  local length=${#value}
  printf '%s' "$value"
  if [ "$length" -lt "$width" ]; then
    printf '%*s' "$((width - length))" ''
  fi
}

section() {
  local step="$1"
  local title="$2"
  local heading="[$step/3] $title"
  local line_width=54
  local remaining=$((line_width - ${#heading} - 1))
  local separator=''
  local index
  [ "$remaining" -lt 1 ] && remaining=1
  for ((index = 0; index < remaining; index++)); do
    separator+='─'
  done
  printf '\n%b%s ' "$C_INFO" "$heading"
  printf '%s' "$separator"
  printf '%b\n' "$C_OFF"
}
status() {
  local label
  label="$(pad_right "$2" 9)"
  printf '  %b%s%b %s\n' "$1" "$label" "$C_OFF" "$3"
}

progress() {
  local label="$1" percent="$2" detail="${3:-}"
  local filled=$((percent * 20 / 100))
  local empty=$((20 - filled))
  local bar_fill bar_empty prefix suffix
  printf -v bar_fill '%*s' "$filled" ''
  printf -v bar_empty '%*s' "$empty" ''
  bar_fill=${bar_fill// /#}
  bar_empty=${bar_empty// /-}
  if [ -t 1 ]; then
    prefix=$'\r\033[2K'
    suffix=''
  else
    prefix=''
    suffix='\n'
  fi
  local padded_label
  padded_label="$(pad_right "$label" 14)"
  printf '%b  %s [%s%s] %3d%%  %s%b' \
    "$prefix" "$padded_label" "$bar_fill" "$bar_empty" "$percent" "$detail" "$suffix"
}

progress_state() {
  local mode="$1" log="$2"
  local compile_count checks percent label detail
  case "$mode" in
    build)
      compile_count="$(grep -c '^Compiling ' "$log" 2>/dev/null || true)"
      if grep -q '^Linking ' "$log"; then
        percent=90; label="Bağlanıyor"; detail="aygıt yazılımı"
      elif grep -q '^Building .*firmware\.bin' "$log"; then
        percent=96; label="Paketleniyor"; detail="firmware.bin"
      elif [ "$compile_count" -gt 0 ]; then
        percent=$((10 + compile_count * 78 / 195))
        [ "$percent" -gt 88 ] && percent=88
        label="Derleniyor"; detail="${compile_count} kaynak"
      elif grep -q 'Scanning dependencies' "$log"; then
        percent=10; label="Bağımlılıklar"; detail="taranıyor"
      else
        percent=3; label="Hazırlanıyor"; detail=""
      fi
      ;;
    verify)
      checks="$(grep -Ec '^\[KONTROL\]|başarıyla geçti\.|: OK$' "$log" 2>/dev/null || true)"
      percent=$((checks * 94 / 12))
      [ "$percent" -gt 94 ] && percent=94
      label="Doğrulanıyor"; detail="${checks}/12 kontrol"
      ;;
    *)
      percent=5; label="Calisiyor"; detail=""
      ;;
  esac
  printf '%s|%s|%s\n' "$percent" "$label" "$detail"
}

run_checked() {
  local name="$1"
  local mode="$2"
  local log="$LOG_DIR/${name// /_}.log"
  local pid state last_state='' percent label detail
  shift 2
  "$@" >"$log" 2>&1 &
  pid=$!
  while kill -0 "$pid" 2>/dev/null; do
    state="$(progress_state "$mode" "$log")"
    if [ "$state" != "$last_state" ]; then
      IFS='|' read -r percent label detail <<< "$state"
      progress "$label" "$percent" "$detail"
      last_state="$state"
    fi
    sleep 0.1
  done
  if wait "$pid"; then
    progress "Tamamlandı" 100 "$name"
    printf '\n'
    status "$C_OK" "OK" "$name"
    return 0
  fi
  printf '\n'
  status "$C_ERR" "HATA" "$name"
  tail -n 40 "$log" >&2
  return 1
}

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

section 1 "Derleme"
rm -rf .pio/build firmware
run_checked "Temiz STM32F030 derlemesi" build ./build.sh || exit 1
test -f "$BIN" || die "$BIN uretilemedi"

section 2 "Otomatik doğrulama"
run_checked "Yerel testler, aşamalar ve arayüz kontrolleri" verify ./tools/verify_host.sh || exit 1

section 3 "Programcı ve hedef"
HAS_V2=0
HAS_ONBOARD=0
if command -v lsusb >/dev/null 2>&1; then
  STLINK_USB="$(lsusb | grep -Ei '0483:37' || true)"
  [ -n "$STLINK_USB" ] && status "$C_OK" "BULUNDU" "$STLINK_USB"
  usb_has '0483:3748' && HAS_V2=1
  usb_has '0483:(374b|374d|374e|374f|3752|3753|3754|3755|3757)' && HAS_ONBOARD=1
  if [ "$HAS_V2" -eq 0 ] && [ "$HAS_ONBOARD" -eq 0 ]; then
    if ! lsusb | grep -Eiq "$STLINK_USB_RE"; then
      die "ST-Link USB bulunamadı. Bağımsız V2, Nucleo onboard veya V3 takın."
    fi
  fi
fi

if command -v st-info >/dev/null 2>&1; then
  TARGET_PROBE="$(st-info --probe 2>&1 || true)"
  TARGET_CHIPID="$(printf '%s\n' "$TARGET_PROBE" | sed -n 's/.*chipid:[[:space:]]*\(0x[0-9A-Fa-f]*\).*/\1/p')"
  TARGET_VOLTAGE="$(printf '%s\n' "$TARGET_PROBE" | sed -n 's/.*Target voltage:[[:space:]]*\([0-9.]*\).*/\1/p')"
  if [ -n "$TARGET_CHIPID" ] && [ "$TARGET_CHIPID" != "0x0000" ]; then
    status "$C_OK" "BAĞLI" "STM32 hedefi algılandı (kimlik: $TARGET_CHIPID)"
  else
    status "$C_WARN" "BEKLİYOR" "STM32 hedefi algılanmadı${TARGET_VOLTAGE:+ (ST-Link gerilimi: ${TARGET_VOLTAGE} V)}"
    if [ "${FLASH_FORCE_UPLOAD:-0}" != "1" ]; then
      printf '\n%bHedef kart bağlı değil; aygıt yazılımı derlendi ve doğrulandı, yükleme atlandı.%b\n' "$C_WARN" "$C_OFF"
      printf 'Kart bağlandığında ./flash.sh çalıştırın. SWD kurtarma denemesi için: FLASH_FORCE_UPLOAD=1 ./flash.sh\n'
      exit 0
    fi
    status "$C_WARN" "ZORLA" "FLASH_FORCE_UPLOAD=1 ile SWD kurtarma yolları denenecek"
  fi
fi

if [ "$HAS_V2" -eq 1 ] && [ "$HAS_ONBOARD" -eq 0 ]; then
  METHOD=standalone_v2
  status "$C_INFO" "YÖNTEM" "Bağımsız ST-Link V2: OpenOCD / st-flash"
elif [ "$HAS_ONBOARD" -eq 1 ] && [ "$HAS_V2" -eq 0 ]; then
  METHOD=pio_upload
  status "$C_INFO" "YÖNTEM" "Onboard ST-Link: PlatformIO yükleme"
else
  METHOD=auto
  status "$C_INFO" "YÖNTEM" "Belirsiz programcı: yöntemler sırayla denenecek"
fi

openocd_major() {
  "$1" --version 2>&1 | sed -n 's/.*Debugger 0\.\([0-9]*\).*/\1/p' | head -n1
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
        -c "reset_config none; adapter speed 400"
        -c "program ${PWD}/${BIN} ${FLASH_ADDR} verify reset exit")
  "$oocd" "${args[@]}"
}

flash_openocd_under_reset() {
  local oocd="$1"
  local scripts="$2"
  local -a args
  test -x "$oocd" || return 1
  test -d "$scripts" || return 1
  args=(-s "$scripts" -f interface/stlink.cfg
        -c "transport select swd"
        -f target/stm32f0x.cfg
        -c "reset_config srst_only srst_nogate connect_assert_srst"
        -c "adapter speed 100")
  if [ -n "${STLINK_SERIAL:-}" ]; then
    args+=(-c "adapter serial ${STLINK_SERIAL}")
  fi
  args+=(-c "init; reset halt; program ${PWD}/${BIN} ${FLASH_ADDR} verify reset exit")
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
  status "$C_INFO" "DENE" "PlatformIO yükleme"
  if run_checked "PlatformIO ile yükleme" upload flash_pio; then
    status "$C_OK" "YÜKLENDİ" "Aygıt yazılımı hedefe yazıldı"
    exit 0
  fi
}

try_openocd() {
  status "$C_INFO" "DENE" "PlatformIO OpenOCD"
  if run_checked "PlatformIO OpenOCD ile yükleme" upload flash_openocd "$PIO_OOCD" "$PIO_OOCD_SCRIPTS"; then
    status "$C_OK" "YÜKLENDİ" "Aygıt yazılımı hedefe yazıldı"
    exit 0
  fi
  status "$C_INFO" "DENE" "Sistem OpenOCD"
  if command -v openocd >/dev/null 2>&1 && run_checked "Sistem OpenOCD ile yükleme" upload flash_openocd "$(command -v openocd)" /usr/share/openocd/scripts; then
    status "$C_OK" "YÜKLENDİ" "Aygıt yazılımı hedefe yazıldı"
    exit 0
  fi
}

try_stflash() {
  status "$C_INFO" "DENE" "st-flash"
  if run_checked "st-flash ile yükleme" upload flash_stflash; then
    status "$C_OK" "YÜKLENDİ" "Aygıt yazılımı hedefe yazıldı"
    exit 0
  fi
  return 0
}

try_connect_under_reset() {
  status "$C_INFO" "DENE" "Reset altında düşük hızlı SWD"
  if run_checked "Reset altında OpenOCD ile yükleme" upload flash_openocd_under_reset "$PIO_OOCD" "$PIO_OOCD_SCRIPTS"; then
    status "$C_OK" "YÜKLENDİ" "Aygıt yazılımı hedefe yazıldı"
    exit 0
  fi
  if command -v st-flash >/dev/null 2>&1; then
    if run_checked "Reset altında st-flash ile yükleme" upload st-flash --connect-under-reset --freq=100 --flash=64k --reset write "$BIN" "$FLASH_ADDR"; then
      status "$C_OK" "YÜKLENDİ" "Aygıt yazılımı hedefe yazıldı"
      exit 0
    fi
  fi
  return 0
}

case "$METHOD" in
  pio_upload)
    try_pio
    try_openocd
    try_stflash
    try_connect_under_reset
    ;;
  standalone_v2)
    try_openocd
    try_stflash
    try_pio
    try_connect_under_reset
    ;;
  *)
    try_pio
    try_openocd
    try_stflash
    try_connect_under_reset
    ;;
esac

cat >&2 <<'EOF'

ST-Link USB goruluyor ama MCU SWD yanit vermiyor.
Kontrol edin:
  - SWDIO -> PA13
  - SWCLK -> PA14
  - GND   -> GND
  - 3.3V  -> hedef 3.3V
  - NRST  -> MCU NRST (reset altinda baglanti icin gerekli)
  - Hedef gerilimi 3.3V olmali; ST-Link tarafinda ~2.9V gorunmesi dusuk gerilimdir.
Iki ST-Link takiliysa birini cikarin veya:
  STLINK_SERIAL=<seri> ./flash.sh
EOF
die "Yukleme basarisiz."
