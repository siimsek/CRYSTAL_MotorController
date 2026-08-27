#!/usr/bin/env python3
"""Verify every editable OLED frame against its immutable JSON source.

The JSON stores top-left text coordinates while u8g2 uses baselines.  The
source contract below therefore records the corresponding u8g2 calls; dynamic
values are checked by their formatter and their fixed drawing field.  Alert
bitmaps are allowed to blink, but their data, dimensions and coordinates are
not allowed to change.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "motor_ui.c").read_text(encoding="utf-8")
FRAMES = json.loads((ROOT / "reference" / "OLED_Projesi.oled.json").read_text(encoding="utf-8"))["frames"]

FUNCTIONS = {
    "anaekran": "draw_main_screen",
    "sicaklikuyari": "draw_temp_alert_screen",
    "akimuyari": "draw_current_alert_screen",
    "akimvesicaklikuyari": "draw_both_alert_screen",
    "ayarlar": "draw_settings_screen",
    "sicaklikayarekrani": "draw_temp_set_screen",
    "akimayar": "draw_current_set_screen",
    "varsayilanonay": "draw_default_confirm_screen",
    "onay1": "draw_confirm_1_screen",
    "onay2": "draw_confirm_2_screen",
}

# One source operation per JSON element.  These exact operations make font,
# geometry and text drift visible in review and CI, while dynamic strings keep
# their approved runtime bindings.
OPERATIONS = {
    "anaekran": [
        '47U, 9U, "CRYSTAL"', '0U, 13U, 127U, 13U', '65U, 14U, 65U, 64U',
        '9U, 32U, "Sicaklik"', '4U, 52U, temp_text',
        '88U, 32U, "Akim"', '68U, 52U, current_text',
    ],
    "sicaklikuyari": [
        '47U, 9U, "CRYSTAL"', '0U, 13U, 127U, 13U', '65U, 14U, 65U, 64U',
        '88U, 32U, "Akim"', '68U, 52U, current_text',
        '16U, 34U, 30U, 33U, tempalert_bits',
        '7U, 29U, "Sicaklik"', '16U, 40U, temp_text',
    ],
    "akimuyari": [
        '47U, 9U, "CRYSTAL"', '0U, 13U, 127U, 13U', '68U, 14U, 68U, 64U',
        '9U, 32U, "Sicaklik"', '4U, 52U, temp_text',
        '89U, 38U, 24U, 24U, currentalert_bits',
        '89U, 28U, "Akim"', '86U, 38U, current_text',
    ],
    "akimvesicaklikuyari": [
        '47U, 9U, "CRYSTAL"', '0U, 13U, 127U, 13U', '68U, 14U, 68U, 64U',
        '18U, 33U, 30U, 33U, tempalert_bits',
        '9U, 28U, "Sicaklik"', '18U, 39U, temp_text',
        '89U, 38U, 24U, 24U, currentalert_bits',
        '89U, 28U, "Akim"', '86U, 38U, current_text',
    ],
    "ayarlar": [
        '46U, 9U, "AYARLAR"', '0U, 13U, 127U, 13U',
        '27U, 24U, "Sicaklik"', '27U, 36U, "Akim"',
        '27U, 49U, "Varsayilan"', '27U, 61U, "Ana Ekrana Don"',
        '3U, arrow_y[g_menu_index], 17U, 7U, arrow_bits',
        '92U, 24U, temp_set_text', '92U, 36U, current_set_text',
    ],
    "sicaklikayarekrani": ['25U,', '34U,', '19U,'],
    "akimayar": ['34U,', '28U,', '19U,'],
    "varsayilanonay": [
        '5U, 11U, "VARSAYILAN DEGERLERI"', '10U, 24U, "ONAYLIYOR MUSUNUZ?"',
        '29U, 40U, "EVET"', '29U, 55U, "HAYIR"', '5U, arrow_y, 17U, 7U, arrow_bits',
    ],
    "onay1": [
        '22U, 11U, "DEGISIKLIKLERI"', '10U, 24U, "ONAYLIYOR MUSUNUZ?"',
        '29U, 40U, "EVET"', '29U, 55U, "HAYIR"', '5U, arrow_y, 17U, 7U, arrow_bits',
    ],
    "onay2": [
        '16U, 12U, "DEGISIKLIKLERDEN"', '25U, 24U, "EMIN MISINIZ?"',
        '29U, 40U, "EVET"', '29U, 55U, "HAYIR"', '5U, arrow_y, 17U, 7U, arrow_bits',
    ],
}


def function_body(name: str) -> str:
    match = re.search(rf"static void {name}\(void\)\n{{", SOURCE)
    if not match:
        raise ValueError(f"drawing function missing: {name}")
    start = match.end()
    depth = 1
    for index in range(start, len(SOURCE)):
        if SOURCE[index] == "{":
            depth += 1
        elif SOURCE[index] == "}":
            depth -= 1
            if depth == 0:
                return SOURCE[start:index]
    raise ValueError(f"unterminated drawing function: {name}")


def bit_reverse(value: int) -> int:
    return int(f"{value:08b}"[::-1], 2)


def bitmap_bytes(symbol: str) -> list[int]:
    match = re.search(rf"{symbol}\[\]\s*=\s*{{(.*?)}};", SOURCE, re.S)
    if not match:
        raise ValueError(f"bitmap missing: {symbol}")
    return [int(value, 0) for value in re.findall(r"0x[0-9a-fA-F]+|\b\d+\b", match.group(1))]


def firmware_text(text: str) -> str:
    """Map Turkish source-label glyphs to the project's ASCII OLED font form."""
    return text.translate(str.maketrans("ıİşŞğĞüÜöÖçÇ", "iIsSgGuUoOcC"))


def main() -> int:
    errors: list[str] = []
    frame_names = [frame["name"] for frame in FRAMES]
    if frame_names != list(FUNCTIONS):
        errors.append(f"unexpected JSON frame set/order: {frame_names}")

    for frame in FRAMES:
        name = frame["name"]
        if name not in FUNCTIONS:
            continue
        try:
            body = function_body(FUNCTIONS[name])
        except ValueError as error:
            errors.append(str(error))
            continue
        for operation in OPERATIONS[name]:
            if operation not in body:
                errors.append(f"{name}: missing reference operation {operation}")
        for element in frame["elements"]:
            if element["type"] == "text" and "variable" not in element:
                text = firmware_text(element["text"])
                if f'"{text}"' not in body:
                    errors.append(f"{name}: reference text missing: {element['text']}")
        if "u8g2_font_6x10_tf" not in body:
            errors.append(f"{name}: reference size-1 font missing")
        if any(element.get("size") == 2 for element in frame["elements"]):
            if "u8g2_font_9x15_tf" not in body:
                errors.append(f"{name}: reference size-2 font missing")

    bitmap_symbols = {"tempalert": "tempalert_bits", "akimalert": "currentalert_bits", "ok_0": "arrow_bits"}
    for frame in FRAMES:
        for element in frame["elements"]:
            if element["type"] != "bitmap":
                continue
            variable = element["variable"]
            symbol = bitmap_symbols.get(variable, "arrow_bits" if variable.startswith("ok_") else None)
            if symbol is None:
                errors.append(f"{frame['name']}: unknown bitmap binding {variable}")
                continue
            expected = [bit_reverse(value) for value in element["data"]]
            try:
                actual = bitmap_bytes(symbol)
            except ValueError as error:
                errors.append(str(error))
                continue
            if actual != expected:
                errors.append(f"{frame['name']}: {symbol} data differs from JSON bitmap")

    if errors:
        print("ARAYUZ REFERANS HATASI:")
        print("\n".join(f"  - {error}" for error in errors))
        return 1
    print("OLED JSON referansinin tum ekran/geometri/font/bitmap kontrolleri basarili.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
