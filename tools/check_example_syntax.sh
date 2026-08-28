#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cp "$ROOT/main_kullanim_ornegi.c" "$ROOT/include/motor_ui.h" \
   "$ROOT/include/motor_ui_config.h" "$ROOT/tools/stubs/main.h" "$TMP/"

gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
    -I"$ROOT/tools/stubs" -I"$TMP" \
    "$TMP/main_kullanim_ornegi.c"
echo "CubeMX kullanim ornegi host sozdizimi kontrolu basarili."
