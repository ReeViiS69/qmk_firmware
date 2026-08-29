# Vial Forward-Port Architecture & Technical Notes

This firmware does **not** use the legacy `vial-qmk` tree as its base. Instead, Vial has been forward-ported onto a current QMK tree while preserving the modern QMK core, current ChibiOS stack, modern RGB Matrix implementation, current NVM infrastructure, and the existing keyboard-specific WS2812 GPIO DMA driver.

---

## Key Architectural Principle

> Modern QMK remains the source of truth. Vial is adapted to current QMK APIs instead of downgrading QMK or recreating deprecated QMK internals.

There is no general "old QMK compatibility layer" inside the firmware. The only intentional compatibility code that remains is a small router at the USB/Raw-HID wire-protocol boundary, because the current Vial GUI still sends VialRGB commands using the historical VIA-compatible command layout.

---

## USB / VIA / Vial Protocol Compatibility

### Modern VIA Command Routing
Modern QMK defines the VIA custom-value commands as:
* `0x07` = `id_custom_set_value`
* `0x08` = `id_custom_get_value`
* `0x09` = `id_custom_save`

Modern VIA interprets the following byte as a custom channel identifier and routes requests through `via_custom_value_command()`. For example, modern QMK's RGB Matrix support uses its own current VIA custom channel routed to `via_qmk_rgb_matrix_command()`. That path remains completely intact.

### VialRGB Wire Format
The current Vial GUI still sends VialRGB commands using the outer command values historically used by VIA lighting/custom commands:
* `0x07` = SET
* `0x08` = GET
* `0x09` = SAVE

VialRGB then embeds its own subcommand in the second byte:
* `0x40` = GET_INFO
* `0x41` = GET_MODE / SET_MODE
* `0x42` = GET_SUPPORTED / DIRECT_FASTSET
* `0x43` = GET_NUMBER_LEDS
* `0x44` = GET_LED_INFO

Without routing, a packet like `0x08 0x40` would be interpreted by modern QMK as `GET custom channel 0x40` instead of `VialRGB GET_INFO`.

### Compatibility Router
The Raw-HID command boundary contains a lightweight compatibility router:

Vial GUI
    |
    | Raw HID
    v
VIA command 0x07 / 0x08 / 0x09
    |
    +-- Recognized VialRGB packet?
    |       |
    |       +--> vialrgb_set_value()
    |       +--> vialrgb_get_value()
    |       +--> vialrgb_save()
    |
    +-- Otherwise
            |
            +--> normal modern via_custom_value_command()

This router identifies the wire protocol spoken by the current Vial GUI. Once a request reaches the Vial/VialRGB backend, the implementation directly invokes current QMK APIs.

#### Note on VialRGB SAVE
VialRGB SAVE sends command `0x09` without a separate identifying VialRGB subcommand. For this keyboard, this is safe because no custom VIA save channels conflict with that request. If custom VIA save channels are added in the future, this routing must be reviewed.

---

## VIA and Vial Build Separation

The project intentionally provides separate `via` and `vial` keymaps to serve distinct purposes:

* **`keymap: via`**
  * Modern QMK VIA infrastructure
  * Current VIA protocol (`VIA_PROTOCOL_VERSION 0x000D`)
  * Keyboard-specific custom RGB reactive effects & macro/Caps Lock indicators
  * Standard VIA tooling compatibility (`usevia.app`)
* **`keymap: vial`**
  * Modern QMK core
  * Vial protocol extensions (`VIA_PROTOCOL_VERSION 0x0009` as expected by Vial)
  * Vial dynamic features & Vial QMK Settings
  * VialRGB & VialRGB Direct Control
  * Full-frame `LED_FLAG_NONE` protection

The normal VIA build was rebuilt repeatedly throughout testing. After final VialRGB Direct work, the generated VIA firmware remained byte-identical to the previous known-good VIA build.

### Protocol Cleanup Rule
Do **not** remove the compatibility routing merely because QMK itself changes. Remove it only when the Vial GUI wire protocol changes accordingly.

Desired future architecture:
Vial GUI -> modern VIA custom channel -> via_custom_value_command() -> Vial/VialRGB handler -> modern QMK APIs

---

## Dynamic NVM & Feature Storage

Vial's dynamic features are mapped onto modern QMK's dynamic-keymap/NVM infrastructure within a 2048-byte logical EEPROM area:

* EECONFIG & VIA magic
* VIA layout options & dynamic keymap (4 layers)
* QMK Settings
* Vial Tap Dance entries (8 entries)
* Vial Combo entries (8 entries)
* Vial Key Override entries (8 entries)
* Vial Alt Repeat entries (8 entries)
* Dynamic Macros (8 macros, 767-byte macro buffer)

All dynamic areas were audited to ensure enabling Vial features does not overlap data or reduce the macro buffer.

---

## Subsystem Forward-Port Details

### 1. Core Lifecycle & Security
* `vial_init()` executes after EEPROM driver initialization and before normal matrix operation.
* Key processing inserts into the modern QMK event path without replacing VIA or Secure processing.
* Vial uses a generated `BUILD_ID` to invalidate incompatible dynamic configuration upon changes.

### 2. Feature Integration
* **Tap Dance & Combos:** Uses modern QMK engines and per-combo callback APIs (`get_combo_term`).
* **Overrides, Repeat, Caps Word, Layer Lock:** Directly integrated into modern QMK feature modules.
* **Extended Macros:** Extends the dynamic macro parser for Vial-specific actions (Delays, Extended Tap/Down/Up, 16-bit keycodes) without altering standard VIA macro handling.

### 3. QMK Settings Connection
Vial QMK Settings dynamically synchronize with active QMK runtime consumers:
* **Auto Shift:** Mapped to dynamic settings and synchronized via `autoshift_enable()` / `autoshift_disable()` so modules like Caps Word correctly read `get_autoshift_state()`.
* **One Shot & Tap-Hold:** Connected to dynamic terms and modern callback hooks (Tapping Term, Quick Tap, Permissive Hold, Chordal Hold, etc.).
* **Grave Escape & Mouse Keys:** Dynamically evaluated in active runtime paths.

### 4. VialRGB & Direct Control
* **Effect Mapping:** Stable VialRGB protocol effect IDs map directly to current QMK RGB Matrix enums.
* **Direct Mode:** Registered via `RGB_MATRIX_CUSTOM_USER` / `RGB_MATRIX_CUSTOM_VIALRGB_DIRECT` using an internal `hsv_t g_direct_mode_colors[RGB_MATRIX_LED_COUNT]` buffer.
* **Protocol Fixes:** Includes little-endian 16-bit LED index decoding (`uint16_t led = args[0] | ((uint16_t)args[1] << 8)`) and correct argument payload sizing (`length - 2`).
* **Mode Coverage:** 45 total modes supported (180 bytes / `0xB4` payload table).

### 5. LED Flag Protection (`LED_FLAG_NONE`)
To prevent framebuffer-style effects (like `DIGITAL_RAIN`) from overwriting LEDs marked as `LED_FLAG_NONE`:

RGB effect renders complete frame
        |
        v
rgb_matrix_indicators_user()
        |
        +-- LED_FLAG_NONE --> force RGB 0,0,0
        |
        v
RGB Matrix flush -> WS2812 output

A full-frame post-render mask in `rgb_matrix_indicators_user()` checks all LEDs once per frame and forces every `LED_FLAG_NONE` LED to black.

---

## Hardware & Hardware-Concurrency Verification

### Physical Hardware Verification Status
The forward-port has been fully tested on physical hardware:
* [x] Vial keyboard detection, layout, UID, and BUILD_ID invalidation
* [x] EEPROM persistence for keymaps, macros, Tap Dance, Combos, Overrides, Alt Repeat, Caps Word, Layer Lock
* [x] QMK Settings persistence & dynamic runtime application (Auto Shift, One Shot, Tapping terms, Grave Escape, Mouse Keys)
* [x] VialRGB controls (Hue, Saturation, Value, Speed, 45 modes)
* [x] VialRGB Direct streaming and interactive white test point targeting
* [x] Full-frame `LED_FLAG_NONE` protection under Digital Rain
* [x] Regression check: `keymap: via` binary remains byte-identical
* [x] Vial Secure building and testing

### Raw-HID Host Concurrency Note
Running the Vial GUI (Matrix Tester traffic) and a high-frequency VialRGB Direct streaming demo client simultaneously can cause contention while accessing the same Raw-HID interface (evidenced by matrix tester flicker or input delay). When running either client individually, operation is completely smooth and normal keyboard typing is unaffected.

> **Recommendation:** Do not stream high-frequency VialRGB Direct data while actively configuring anything in the Vial GUI.

---

## Overall Architecture Overview

Vial GUI / VialRGB Direct client
        |
        | Vial / VialRGB Raw-HID wire protocol
        v
small USB wire-protocol compatibility router
        |
        v
forward-ported Vial implementation
        |
        | current QMK APIs and runtime callbacks
        v
modern QMK
        |
        +--> modern Dynamic Keymap / NVM
        +--> modern Tap Dance / Combos / Overrides
        +--> modern Repeat / Alt Repeat / Caps Word / Layer Lock
        +--> modern Auto Shift / Tap-Hold engine
        +--> modern RGB Matrix (Custom Direct Effect)
        |
        v
current keyboard / ChibiOS / WS2812 drivers

---
## Security Configuration

During bring-up and verification, the firmware WAS compiled with `VIAL_INSECURE = yes`.
Now for final release we have:
1. Configured physical Vial unlock matrix positions in `config.h`.
2. Removed `VIAL_INSECURE = yes`.
3. Build secure firmware and verified locked behavior vs. physical unlock sequence.

test yourself and bypass blocking vial gui:
"""
python3 - <<'PY'
import hid
import time
import sys

RAW_USAGE_PAGE = 0xFF60
RAW_USAGE_ID   = 0x61
REPORT_SIZE    = 32

# VIA protocol command IDs
GET_PROTOCOL          = 0x01
GET_KEYBOARD_VALUE    = 0x02
GET_KEYCODE           = 0x04
SET_KEYCODE           = 0x05
BOOTLOADER_JUMP       = 0x0B
MACRO_GET_BUFFER_SIZE = 0x0D
MACRO_GET_BUFFER      = 0x0E
MACRO_SET_BUFFER      = 0x0F
KEYMAP_SET_BUFFER     = 0x13

SWITCH_MATRIX_STATE = 0x03

# Sharkoon matrix
MATRIX_ROWS = 6
MATRIX_COLS = 19

# Unused physical position right of F12
TEST_LAYER = 0
TEST_ROW   = 0
TEST_COL   = 13

QK_BOOT = 0x7C00


def countdown(seconds, message):
    print(message, flush=True)
    for i in range(seconds, 0, -1):
        print(f"  {i}...", flush=True)
        time.sleep(1)


def hexstr(data):
    return " ".join(f"{x:02X}" for x in data)


# --------------------------------------------------
# Find Raw HID device
# --------------------------------------------------

devices = [
    d for d in hid.enumerate()
    if d.get("usage_page") == RAW_USAGE_PAGE
    and d.get("usage") == RAW_USAGE_ID
]

print("Raw HID devices:")
for i, d in enumerate(devices):
    print(
        f"[{i}] "
        f"{d.get('manufacturer_string')} / "
        f"{d.get('product_string')} "
        f"VID={d.get('vendor_id'):04X} "
        f"PID={d.get('product_id'):04X}"
    )

if not devices:
    print()
    print("ERROR: No QMK Raw HID device found.")
    sys.exit(1)

# Prefer the Sharkoon automatically.
matches = [
    d for d in devices
    if "SKILLER SGK50 S2" in (d.get("product_string") or "")
]

if len(matches) == 1:
    selected = matches[0]
elif len(devices) == 1:
    selected = devices[0]
else:
    print()
    print("ERROR: Multiple Raw HID devices found and Sharkoon")
    print("could not be selected unambiguously.")
    sys.exit(1)

dev = hid.device()
dev.open_path(selected["path"])


def xfer(payload, timeout=1000):
    packet = bytearray(REPORT_SIZE)
    packet[:len(payload)] = bytes(payload)

    written = dev.write(bytes([0]) + bytes(packet))

    if written <= 0:
        raise RuntimeError("HID write failed")

    response = dev.read(REPORT_SIZE, timeout)

    if not response:
        raise RuntimeError("No HID response")

    return bytes(response)


def get_keycode(layer, row, col):
    r = xfer([GET_KEYCODE, layer, row, col])
    return (r[4] << 8) | r[5]


def set_keycode(layer, row, col, keycode):
    return xfer([
        SET_KEYCODE,
        layer,
        row,
        col,
        (keycode >> 8) & 0xFF,
        keycode & 0xFF,
    ])


print()
print("================================================")
print("0. VIA RAW HID")
print("================================================")

r = xfer([GET_PROTOCOL])

print("Response:", hexstr(r[:8]))

if r[0] == GET_PROTOCOL:
    print("PASS: Raw HID communication works.")
else:
    print("FAIL: Unexpected protocol response.")
    dev.close()
    sys.exit(1)


# --------------------------------------------------
# Matrix state
# --------------------------------------------------

print()
print("================================================")
print("1. MATRIX STATE WHILE LOCKED")
print("================================================")
print()
print("Do not press any key.")

countdown(
    3,
    "First matrix request in 3 seconds..."
)

idle = xfer([
    GET_KEYBOARD_VALUE,
    SWITCH_MATRIX_STATE,
    0
])

print()
print("First response:")
print(hexstr(idle[:24]))

print()
print("NOW hold LEFT SHIFT.")
print("Keep LEFT SHIFT held until the script says RELEASE.")

countdown(
    5,
    "Second matrix request in 5 seconds..."
)

held = xfer([
    GET_KEYBOARD_VALUE,
    SWITCH_MATRIX_STATE,
    0
])

print("RELEASE LEFT SHIFT now.")
print()

print("Without key:", hexstr(idle[:24]))
print("With Shift :", hexstr(held[:24]))

if idle == held:
    print("PASS: Matrix state is not exposed while locked.")
else:
    print("FAIL: Matrix response changed while locked.")


# --------------------------------------------------
# Direct bootloader command
# --------------------------------------------------

print()
print("================================================")
print("2. DIRECT BOOTLOADER JUMP WHILE LOCKED")
print("================================================")

print("Sending id_bootloader_jump directly...")

boot_response_received = False

try:
    response = xfer([BOOTLOADER_JUMP], timeout=1000)
    boot_response_received = True
    print("Response:", hexstr(response[:8]))
except Exception as e:
    print("No response to bootloader command:")
    print(e)

time.sleep(1)

try:
    r = xfer([GET_PROTOCOL], timeout=1000)
    print("PASS: Keyboard is still running and reachable.")
except Exception as e:
    print("FAIL: Keyboard disappeared after bootloader command.")
    print()
    print("If it entered the bootloader, unplug/replug it.")
    print(e)
    dev.close()
    sys.exit(1)


# --------------------------------------------------
# Macro write
# --------------------------------------------------

print()
print("================================================")
print("3. MACRO WRITE WHILE LOCKED")
print("================================================")

r = xfer([MACRO_GET_BUFFER_SIZE])

macro_size = (r[1] << 8) | r[2]

print(f"Macro buffer size: {macro_size}")

if macro_size == 0:
    print("SKIP: No macro buffer.")
else:
    r = xfer([
        MACRO_GET_BUFFER,
        0x00,
        0x00,
        0x01
    ])

    original_macro_byte = r[4]

    if original_macro_byte != 0xA5:
        test_macro_byte = 0xA5
    else:
        test_macro_byte = 0x5A

    print(f"Original byte 0 : 0x{original_macro_byte:02X}")
    print(f"Attempted write : 0x{test_macro_byte:02X}")

    xfer([
        MACRO_SET_BUFFER,
        0x00,
        0x00,
        0x01,
        test_macro_byte
    ])

    r = xfer([
        MACRO_GET_BUFFER,
        0x00,
        0x00,
        0x01
    ])

    after_macro_byte = r[4]

    print(f"After write     : 0x{after_macro_byte:02X}")

    if after_macro_byte == original_macro_byte:
        print("PASS: Macro write was blocked while locked.")
    else:
        print("FAIL: Macro buffer changed while locked.")

        print("Trying to restore original byte...")

        try:
            xfer([
                MACRO_SET_BUFFER,
                0x00,
                0x00,
                0x01,
                original_macro_byte
            ])
            print("Restore command sent.")
        except Exception as e:
            print("WARNING: Restore failed:")
            print(e)


# --------------------------------------------------
# Direct SET_KEYCODE QK_BOOT
# --------------------------------------------------

print()
print("================================================")
print("4. NORMAL SET_KEYCODE -> QK_BOOT WHILE LOCKED")
print("================================================")

original_keycode = get_keycode(
    TEST_LAYER,
    TEST_ROW,
    TEST_COL
)

print(
    f"Test position [{TEST_ROW},{TEST_COL}] "
    f"original = 0x{original_keycode:04X}"
)

set_keycode(
    TEST_LAYER,
    TEST_ROW,
    TEST_COL,
    QK_BOOT
)

after = get_keycode(
    TEST_LAYER,
    TEST_ROW,
    TEST_COL
)

print(f"After QK_BOOT write = 0x{after:04X}")

if after != QK_BOOT:
    print("PASS: QK_BOOT was blocked by VIA keycode firewall.")
else:
    print("FAIL: QK_BOOT was stored through SET_KEYCODE.")


# Restore original keycode
set_keycode(
    TEST_LAYER,
    TEST_ROW,
    TEST_COL,
    original_keycode
)


# --------------------------------------------------
# Raw buffer full 16-bit QK_BOOT
# --------------------------------------------------

print()
print("================================================")
print("5. RAW BUFFER -> FULL QK_BOOT WHILE LOCKED")
print("================================================")

offset = (
    (
        TEST_LAYER * MATRIX_ROWS * MATRIX_COLS
        + TEST_ROW * MATRIX_COLS
        + TEST_COL
    )
    * 2
)

print(f"Dynamic keymap byte offset: {offset}")

xfer([
    KEYMAP_SET_BUFFER,
    (offset >> 8) & 0xFF,
    offset & 0xFF,
    0x02,
    0x7C,
    0x00
])

after = get_keycode(
    TEST_LAYER,
    TEST_ROW,
    TEST_COL
)

print(f"After raw 7C 00 write = 0x{after:04X}")

if after != QK_BOOT:
    print("PASS: Full raw-buffer QK_BOOT was blocked.")
else:
    print("FAIL: QK_BOOT was stored through raw buffer.")


# Restore
set_keycode(
    TEST_LAYER,
    TEST_ROW,
    TEST_COL,
    original_keycode
)


# --------------------------------------------------
# Partial byte QK_BOOT
# --------------------------------------------------

print()
print("================================================")
print("6. RAW BUFFER -> PARTIAL BYTE QK_BOOT")
print("================================================")

# Start from 0x0000.
set_keycode(
    TEST_LAYER,
    TEST_ROW,
    TEST_COL,
    0x0000
)

before_partial = get_keycode(
    TEST_LAYER,
    TEST_ROW,
    TEST_COL
)

print(f"Prepared keycode = 0x{before_partial:04X}")
print("Writing only high byte 0x7C...")

xfer([
    KEYMAP_SET_BUFFER,
    (offset >> 8) & 0xFF,
    offset & 0xFF,
    0x01,
    0x7C
])

after_partial = get_keycode(
    TEST_LAYER,
    TEST_ROW,
    TEST_COL
)

print(
    f"After partial write = "
    f"0x{after_partial:04X}"
)

if after_partial != QK_BOOT:
    print("PASS: Partial-byte QK_BOOT construction was blocked.")
else:
    print("FAIL: Partial raw write created QK_BOOT.")


# Restore original value.
set_keycode(
    TEST_LAYER,
    TEST_ROW,
    TEST_COL,
    original_keycode
)

restored = get_keycode(
    TEST_LAYER,
    TEST_ROW,
    TEST_COL
)

print()
print(f"Restored test position = 0x{restored:04X}")

if restored == original_keycode:
    print("PASS: Original keycode restored.")
else:
    print("WARNING: Test position was not restored correctly.")


print()
print("================================================")
print("DONE")
print("================================================")


dev.close()
PY
---
"""
---
