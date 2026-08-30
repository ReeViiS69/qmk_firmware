#!/usr/bin/env python3
"""Generate the VialRGB LED_FLAG_NONE lookup used by this keymap.

The generated include is a build artifact only. keyboard.json remains the
single source of truth; nothing in the source tree is rewritten during build.
"""

import json
import sys
from pathlib import Path


def fail(message: str) -> None:
    print(f"generate_disabled_leds.py: {message}", file=sys.stderr)
    raise SystemExit(1)


def generate(keyboard_json: Path) -> str:
    try:
        data = json.loads(keyboard_json.read_text(encoding="utf-8"))
    except FileNotFoundError:
        fail(f"keyboard.json not found: {keyboard_json}")
    except json.JSONDecodeError as exc:
        fail(f"invalid JSON in {keyboard_json}: {exc}")

    try:
        layout = data["rgb_matrix"]["layout"]
    except (KeyError, TypeError):
        fail("keyboard.json has no rgb_matrix.layout array")

    if not isinstance(layout, list):
        fail("rgb_matrix.layout must be an array")

    # Match QMK's generate-keyboard-c semantics exactly: a missing flags field
    # defaults to 0, so it is also an LED_FLAG_NONE entry.
    disabled = [index for index, led in enumerate(layout) if led.get("flags", 0) == 0]

    if disabled and (disabled[-1] > 0xFF or len(disabled) > 0xFF):
        fail("LED_FLAG_NONE lookup no longer fits the uint8_t runtime representation")

    # Keep the declaration valid even if a future keyboard.json has no disabled
    # LEDs. The count is then zero, so the dummy element is never accessed.
    values = disabled if disabled else [0]
    rows = []
    for start in range(0, len(values), 12):
        rows.append("    " + ", ".join(str(value) for value in values[start : start + 12]) + ",")

    return "\n".join(
        [
            "/* Auto-generated from keyboard.json. Do not edit. */",
            "#pragma once",
            "",
            f"#define SHARKOON_VIALRGB_DISABLED_LED_COUNT {len(disabled)}",
            "static const uint8_t sharkoon_vialrgb_disabled_leds[] = {",
            *rows,
            "};",
            "",
        ]
    )


def main() -> None:
    if len(sys.argv) != 3:
        fail("usage: generate_disabled_leds.py <keyboard.json> <output.inc>")

    keyboard_json = Path(sys.argv[1])
    output = Path(sys.argv[2])
    content = generate(keyboard_json)

    output.parent.mkdir(parents=True, exist_ok=True)

    # Avoid touching the timestamp if the generated content did not change.
    if output.exists() and output.read_text(encoding="utf-8") == content:
        return

    temp = output.with_suffix(output.suffix + ".tmp")
    temp.write_text(content, encoding="utf-8")
    temp.replace(output)


if __name__ == "__main__":
    main()
