#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

bash "$ROOT/tools/check_all_stages.sh"
python3 "$ROOT/tools/check_ui_lock.py"
bash "$ROOT/tools/test_acs_pp_rms.sh"
bash "$ROOT/tools/test_safety_host.sh"
bash "$ROOT/tools/test_relay_active_low.sh"
bash "$ROOT/tools/test_stage2_sim.sh"
bash "$ROOT/tools/check_example_syntax.sh"
git -C "$ROOT" diff --check

echo "Tum yerel host dogrulamalari basarili."
