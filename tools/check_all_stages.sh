#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STUBS="$ROOT/tools/stubs"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

for stage in 1 2 3 4; do
    DIR="$TMP/stage$stage"
    mkdir -p "$DIR"

    cp "$ROOT/src/motor_ui.c" "$DIR/"
    cp "$ROOT/include/motor_ui.h" "$DIR/"
    cp "$ROOT/src/u8g2_stm32_port.c" "$DIR/"

    sed \
      -e "s/\(#define MOTOR_UI_STAGE[[:space:]]*\)[0-9]*U/\1${stage}U/" \
      -e "s/\(#define EEPROM_ENABLE[[:space:]]*\)[0-9]*U/\11U/" \
      "$ROOT/include/motor_ui_config.h" > "$DIR/motor_ui_config.h"

    echo "[CHECK] Stage $stage"
    gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
        -I"$STUBS" -I"$DIR" \
        "$DIR/motor_ui.c" "$DIR/u8g2_stm32_port.c"
done

echo "Tum Stage 1-4 kosullu derleme kontrolleri basarili."
