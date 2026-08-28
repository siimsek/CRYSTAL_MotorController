#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cp "$ROOT/src/motor_ui.c" "$ROOT/include/motor_ui.h" \
   "$ROOT/include/motor_ui_config.h" "$ROOT/tools/stubs/main.h" "$TMP/"
cp "$ROOT/tests/test_acs_pp_rms.c" "$TMP/test_acs_pp_rms.c"
sed -i 's|"../src/motor_ui.c"|"motor_ui.c"|' "$TMP/test_acs_pp_rms.c"

gcc -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$ROOT/tools/stubs" -I"$TMP" \
    "$TMP/test_acs_pp_rms.c" -Wl,--gc-sections -lm -o "$TMP/test_acs_pp_rms"
"$TMP/test_acs_pp_rms"
echo "ACS tepe-tepe/sinüs RMS yerel testi başarıyla geçti."
