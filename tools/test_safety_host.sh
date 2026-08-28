#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cp "$ROOT/src/motor_ui.c" "$ROOT/include/motor_ui.h" \
   "$ROOT/include/motor_ui_config.h" "$ROOT/tools/stubs/main.h" "$TMP/"
cp "$ROOT/tests/test_safety_host.c" "$TMP/test_safety_host.c"
sed -i 's|"../src/motor_ui.c"|"motor_ui.c"|' "$TMP/test_safety_host.c"

gcc -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$ROOT/tools/stubs" -I"$TMP" \
    "$TMP/test_safety_host.c" -Wl,--gc-sections -lm -o "$TMP/test_safety_host"
"$TMP/test_safety_host"
echo "Güvenlik/ADC/IRQ yerel testi başarıyla geçti."
