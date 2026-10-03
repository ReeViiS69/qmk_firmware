# Quantum Mechanical Keyboard Firmware

[![Current Version](https://img.shields.io/github/tag/qmk/qmk_firmware.svg)](https://github.com/qmk/qmk_firmware/tags)
[![Discord](https://img.shields.io/discord/440868230475677696.svg)](https://discord.gg/qmk)
[![Docs Status](https://img.shields.io/badge/docs-ready-orange.svg)](https://docs.qmk.fm)
[![GitHub contributors](https://img.shields.io/github/contributors/qmk/qmk_firmware.svg)](https://github.com/qmk/qmk_firmware/pulse/monthly)
[![GitHub forks](https://img.shields.io/github/forks/qmk/qmk_firmware.svg?style=social&label=Fork)](https://github.com/qmk/qmk_firmware/)

This is a keyboard firmware based on the [tmk\_keyboard firmware](https://github.com/tmk/tmk_keyboard) with some useful features for Atmel AVR and ARM controllers, and more specifically, the [OLKB product line](https://olkb.com), the [ErgoDox EZ](https://ergodox-ez.com) keyboard, and the Clueboard product line.

## Documentation

* [See the official documentation on docs.qmk.fm](https://docs.qmk.fm)

The docs are powered by [VitePress](https://vitepress.dev/). They are also viewable offline; see [Previewing the Documentation](https://docs.qmk.fm/#/contributing?id=previewing-the-documentation) for more details.

You can request changes by making a fork and opening a [pull request](https://github.com/qmk/qmk_firmware/pulls).

## Supported Keyboards

* [Planck](/keyboards/planck/)
* [Preonic](/keyboards/preonic/)
* [ErgoDox EZ](/keyboards/ergodox_ez/)
* [Clueboard](/keyboards/clueboard/)
* [Cluepad](/keyboards/clueboard/17/)
* [Atreus](/keyboards/atreus/)

The project also includes community support for [lots of other keyboards](/keyboards/).

## Maintainers

QMK is developed and maintained by Jack Humbert of OLKB with contributions from the community, and of course, [Hasu](https://github.com/tmk). The OLKB product firmwares are maintained by [Jack Humbert](https://github.com/jackhumbert), the Ergodox EZ by [ZSA Technology Labs](https://github.com/zsa), the Clueboard by [Zach White](https://github.com/skullydazed), and the Atreus by [Phil Hagelberg](https://github.com/technomancy).

## Official Website

[qmk.fm](https://qmk.fm) is the official website of QMK, where you can find links to this page, the documentation, and the keyboards supported by QMK.

# QMK Firmware with Modern Vial Support

This repository contains a forward-ported implementation of **Vial** running on top of a **modern QMK codebase**.

Instead of using the legacy `vial-qmk` tree as a foundation, this port adapts Vial directly to current QMK APIs while preserving the modern QMK core, current ChibiOS stack, modern RGB Matrix engine, current NVM/EEPROM infrastructure, and the keyboard-specific WS2812 driver.

---

## 💡 Key Features & Forward-Port Highlights

* **Modern Core Architecture:** Uses up-to-date QMK subsystems without degrading ChibiOS or core APIs.
* **Full Dynamic Features:** Supports dynamic Keymap (4 layers), Tap Dance (8), Combos (8), Key Overrides (8), Alt Repeat (8), Repeat Key, Caps Word, Layer Lock, and Extended Macros (767-byte buffer, 16-bit keycodes, delays).
* **Vial QMK Settings:** Dynamic control mapped to current QMK callbacks (Auto Shift, One Shot, Tapping Terms, Grave Escape, Mouse Keys).
* **VialRGB & Direct Mode:** Integrated via modern `RGB_MATRIX_CUSTOM_USER` custom effects with support for 45 modes and full-frame `LED_FLAG_NONE` masking.
* **Non-Destructive Routing:** Includes a lightweight USB HID compatibility router for VialRGB without breaking standard VIA builds.
* **BUILD_ID Persistence:** Uses a generated `BUILD_ID` to safely invalidate dynamic configuration upon firmware changes.

---

## 🛠️ Build Targets & Regression Isolation

This repository maintains two distinct keymap targets to ensure complete separation between standard QMK VIA and Vial functionality:

| Keymap Target | Protocol / Version | Features & Intended Use |
| :--- | :--- | :--- |
| `keymap: via` | Standard VIA (`0x000D`) | Standard modern QMK VIA build. Retains vendor-specific RGB reactive modes and LED indicators. Compatible with `usevia.app`. **Remains byte-identical during Vial updates.** |
| `keymap: vial` | Vial Protocol (`0x0009`) | Modern QMK core with Vial extensions, dynamic settings, VialRGB Direct control, and full-frame LED protection. Compatible with Vial GUI (`vial.rocks`). |

---

## 🧪 Hardware Verification Status

The forward-port has been extensively tested and verified on physical hardware:

- [x] **Core & Layout:** Vial detection, layout, UID, and `BUILD_ID` invalidation.
- [x] **Dynamic NVM:** EEPROM persistence for keymaps, dynamic macros, Tap Dance, Combos, Key Overrides, and Alt Repeat.
- [x] **QMK Settings:** Dynamic runtime application for Auto Shift, One Shot, Tapping Terms, Grave Escape, and Mouse Keys.
- [x] **VialRGB & Direct Mode:** 45 modes, per-LED Direct streaming, and interactive test point rendering.
- [x] **LED Protection:** Full-frame `LED_FLAG_NONE` masking verified under heavy effects like Digital Rain.
- [x] **Regression Safety:** Verified that `keymap: via` builds remain byte-identical after Vial-only changes.
- [x] Vial Secure building and testing
---

## ⚠️ Usage & Security Notes

> [!NOTE]
> **Raw-HID Client Concurrency:** Do not run high-frequency VialRGB Direct streaming clients and the Vial GUI (Matrix Tester traffic) simultaneously. Running both concurrently can cause contention while accessing the same Raw-HID interface. Each client operates smoothly when run independently.

> [!WARNING]
> **Bring-up Security State:** This firmware was set to `VIAL_INSECURE = yes` for bring-up and physical verification. 
> Now for final release we have:
> 1. Configured physical Vial unlock matrix positions in `config.h`.
> 2. Removed `VIAL_INSECURE = yes`.
> 3. Build secure firmware and verified locked behavior vs. physical unlock sequence.
---

## 📖 Architecture & Technical Breakdown

For deep-dive documentation on the USB compatibility router, NVM storage offsets, RGB Matrix custom effect registration, and future maintenance guidelines:

👉 **[Read the Full Forward-Port Architecture Guide](docs/architecture.md)**
