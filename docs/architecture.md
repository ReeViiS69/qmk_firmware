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

### Raw-HID Host Concurrency Note
Running the Vial GUI (Matrix Tester traffic) and a high-frequency VialRGB Direct streaming demo client simultaneously can cause contention while accessing the same Raw-HID interface (evidenced by matrix tester flicker or input delay). When running either client individually, operation is completely smooth and normal keyboard typing is unaffected.

> **Recommendation:** Do not stream high-frequency VialRGB Direct data while actively configuring anything in the Vial GUI.

---

## Security Configuration

During bring-up and verification, the firmware is compiled with `VIAL_INSECURE = yes`.
Before final release:
1. Configure physical Vial unlock matrix positions in `config.h`.
2. Remove `VIAL_INSECURE = yes`.
3. Build secure firmware and verify locked behavior vs. physical unlock sequence.

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
