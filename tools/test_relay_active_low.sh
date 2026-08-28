#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cp "$ROOT/src/motor_ui.c" "$ROOT/include/motor_ui.h" \
   "$ROOT/include/motor_ui_config.h" "$ROOT/tools/stubs/main.h" "$TMP/"
cp "$ROOT/tests/test_safety_host.c" "$TMP/test_safety_host.c"
sed -i 's|"../src/motor_ui.c"|"motor_ui.c"|' "$TMP/test_safety_host.c"

# Simulate an active-low relay driver and NC motor path.  The same safety
# assertions must still command the configured CUT level at startup/alarm.
sed -i \
    -e 's/#define RELAY_COIL_ACTIVE_LEVEL                     GPIO_PIN_SET/#define RELAY_COIL_ACTIVE_LEVEL                     GPIO_PIN_RESET/' \
    -e 's/#define RELAY_COIL_INACTIVE_LEVEL                   GPIO_PIN_RESET/#define RELAY_COIL_INACTIVE_LEVEL                   GPIO_PIN_SET/' \
    -e 's/#define MOTOR_POWER_ALLOW_RELAY_LEVEL               RELAY_COIL_ACTIVE_LEVEL/#define MOTOR_POWER_ALLOW_RELAY_LEVEL               RELAY_COIL_INACTIVE_LEVEL/' \
    -e 's/#define MOTOR_POWER_CUT_RELAY_LEVEL                 RELAY_COIL_INACTIVE_LEVEL/#define MOTOR_POWER_CUT_RELAY_LEVEL                 RELAY_COIL_ACTIVE_LEVEL/' \
    "$TMP/motor_ui_config.h"

gcc -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$ROOT/tools/stubs" -I"$TMP" \
    "$TMP/test_safety_host.c" -Wl,--gc-sections -lm -o "$TMP/test_safety_host"
"$TMP/test_safety_host"
echo "Aktif-low/NC role fail-safe host testi basarili."
