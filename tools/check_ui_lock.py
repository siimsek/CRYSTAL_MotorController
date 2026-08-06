#!/usr/bin/env python3
"""OLED nihai arayüzündeki kritik çizim çağrılarının korunduğunu kontrol eder."""

from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
source = (root / "src" / "motor_ui.c").read_text(encoding="utf-8")

required = [
    'ACILIS_BITMAP_X',
    'ACILIS_BITMAP_Y',
    'ACILIS_BITMAP_WIDTH',
    'ACILIS_BITMAP_HEIGHT',
    'acilis_bitmap_bits',
    'column_centered_x',
    'column_aligned_left_x',
    'u8g2_DrawLine(&g_u8g2, 0U, 13U, 127U, 13U)',
    'u8g2_DrawLine(&g_u8g2, 65U, 14U, 65U, 64U)',
    'u8g2_DrawXBM(&g_u8g2, 17U, 24U, 30U, 33U, tempalert_bits)',
    'u8g2_DrawXBM(&g_u8g2, 85U, 28U, 24U, 24U, currentalert_bits)',
    'static const uint8_t arrow_y[4] = {16U, 28U, 41U, 53U}',
    'u8g2_DrawUTF8(&g_u8g2, 12U, 61U, "Ana Ekrana Don")',
    'static const char title[] = "SICAKLIK AYARI"',
    'static const char title[] = "AKIM AYARI"',
    'u8g2_DrawXBM(&g_u8g2, 0U, arrow_y[g_menu_index], 9U, 7U, arrow_bits)',
    'UI_SCREEN_SPLASH',
    'UI_SPLASH_MS',
]

missing = [item for item in required if item not in source]
if missing:
    print("ARAYUZ KILIDI HATASI: Kritik cizim ifadeleri eksik veya degismis:")
    for item in missing:
        print(f"  - {item}")
    sys.exit(1)

print("Arayuz kilidi kontrolu basarili.")
