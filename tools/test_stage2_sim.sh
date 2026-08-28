#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cp "$ROOT/src/motor_ui.c" "$ROOT/include/motor_ui.h" \
   "$ROOT/tools/stubs/main.h" "$TMP/"
sed -e "s/\(#define MOTOR_UI_STAGE[[:space:]]*\)[0-9]*U/\12U/" \
    "$ROOT/include/motor_ui_config.h" > "$TMP/motor_ui_config.h"
cp "$ROOT/tests/test_stage2_sim.c" "$TMP/test_stage2_sim.c"

gcc -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$ROOT/tools/stubs" -I"$TMP" \
    "$TMP/test_stage2_sim.c" -Wl,--gc-sections -lm -o "$TMP/test_stage2_sim"
"$TMP/test_stage2_sim"
echo "Aşama 1/2 benzetimli değer yerel testi başarıyla geçti."
