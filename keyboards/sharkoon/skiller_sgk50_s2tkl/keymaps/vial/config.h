// Copyright 2024 JoyLee (@itarze)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define VIAL_KEYBOARD_UID {0x3B, 0xE0, 0x9B, 0x6A, 0x76, 0x7E, 0x2B, 0x12}

/*
 * Dynamic Vial storage limits.
 *
 * Keep all dynamic feature pools at 8 entries so enough EEPROM remains
 * available for the shared dynamic macro buffer.
 */
#define VIAL_TAP_DANCE_ENTRIES 8
#define VIAL_COMBO_ENTRIES 8
#define VIAL_KEY_OVERRIDE_ENTRIES 8
#define VIAL_ALT_REPEAT_KEY_ENTRIES 8

/*
 * Eight macro slots (M0-M7).
 * The macro EEPROM buffer itself is NOT reduced by this setting.
 */
#define DYNAMIC_KEYMAP_MACRO_COUNT 8
//for secure vial unlock
#define VIAL_UNLOCK_COMBO_ROWS {0, 4}
#define VIAL_UNLOCK_COMBO_COLS {13, 18}
/*
 * Sharkoon SGK50 S2 TKL:
 *
 * Keep QMK/Vial's official RGB_MATRIX_SOLID_REACTIVE mode ID, but rename
 * QMK's stock renderer while rgb_matrix.c is compiled. rgb_matrix_user.inc
 * then supplies our keyboard-specific SOLID_REACTIVE() implementation.
 *
 * This keeps the official VialRGB "Solid Reactive" slot fully compatible
 * with Vial Desktop and Vial Web without modifying Vial/QMK core files.
 */
#define SOLID_REACTIVE SHARKOON_ORIGINAL_SOLID_REACTIVE