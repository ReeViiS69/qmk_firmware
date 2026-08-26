/* Copyright 2017 Jason Williams (Wilba)
 * Copyright 2024-2025 Nick Brassel (@tzarc)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dynamic_keymap.h"
#include "keymap_introspection.h"
#include "action.h"
#include "send_string.h"
#include "keycodes.h"
#include "wait.h"
#include <string.h>
#include "nvm_dynamic_keymap.h"

#ifdef ENCODER_ENABLE
#    include "encoder.h"
#else
#    define NUM_ENCODERS 0
#endif

#ifndef DYNAMIC_KEYMAP_MACRO_DELAY
#    define DYNAMIC_KEYMAP_MACRO_DELAY TAP_CODE_DELAY
#endif

uint8_t dynamic_keymap_get_layer_count(void) {
    return DYNAMIC_KEYMAP_LAYER_COUNT;
}

uint16_t dynamic_keymap_get_keycode(uint8_t layer, uint8_t row, uint8_t column) {
    return nvm_dynamic_keymap_read_keycode(layer, row, column);
}

void dynamic_keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t column, uint16_t keycode) {
    nvm_dynamic_keymap_update_keycode(layer, row, column, keycode);
}

#ifdef ENCODER_MAP_ENABLE
uint16_t dynamic_keymap_get_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise) {
    return nvm_dynamic_keymap_read_encoder(layer, encoder_id, clockwise);
}

void dynamic_keymap_set_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise, uint16_t keycode) {
    nvm_dynamic_keymap_update_encoder(layer, encoder_id, clockwise, keycode);
}
#endif // ENCODER_MAP_ENABLE

#ifdef VIAL_ENABLE
#    include "vial.h"
#endif

void dynamic_keymap_reset(void) {
#ifdef VIAL_ENABLE
    /*
     * Temporarily unlock Vial while restoring the compiled-in keymap.
     * This allows trusted firmware defaults such as QK_BOOT to be restored.
     */
    int vial_unlocked_prev = vial_unlocked;
    vial_unlocked = 1;
#endif

    // Erase the keymaps, if necessary.
    nvm_dynamic_keymap_erase();

    // Reset the keymaps in EEPROM to what is in flash.
    for (int layer = 0; layer < DYNAMIC_KEYMAP_LAYER_COUNT; layer++) {
        for (int row = 0; row < MATRIX_ROWS; row++) {
            for (int column = 0; column < MATRIX_COLS; column++) {
                dynamic_keymap_set_keycode(layer, row, column, keycode_at_keymap_location_raw(layer, row, column));
            }
        }
#ifdef ENCODER_MAP_ENABLE
        for (int encoder = 0; encoder < NUM_ENCODERS; encoder++) {
            dynamic_keymap_set_encoder(layer, encoder, true, keycode_at_encodermap_location_raw(layer, encoder, true));
            dynamic_keymap_set_encoder(layer, encoder, false, keycode_at_encodermap_location_raw(layer, encoder, false));
        }
#endif // ENCODER_MAP_ENABLE
    }

#ifdef VIAL_ENABLE
    /* Restore the previous Vial lock state. */
    vial_unlocked = vial_unlocked_prev;
#endif
}

void dynamic_keymap_get_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    nvm_dynamic_keymap_read_buffer(offset, size, data);
}

void dynamic_keymap_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    nvm_dynamic_keymap_update_buffer(offset, size, data);
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    if (layer_num < DYNAMIC_KEYMAP_LAYER_COUNT && row < MATRIX_ROWS && column < MATRIX_COLS) {
        return dynamic_keymap_get_keycode(layer_num, row, column);
    }
    return KC_NO;
}

#ifdef ENCODER_MAP_ENABLE
uint16_t keycode_at_encodermap_location(uint8_t layer_num, uint8_t encoder_idx, bool clockwise) {
    if (layer_num < DYNAMIC_KEYMAP_LAYER_COUNT && encoder_idx < NUM_ENCODERS) {
        return dynamic_keymap_get_encoder(layer_num, encoder_idx, clockwise);
    }
    return KC_NO;
}
#endif // ENCODER_MAP_ENABLE

uint8_t dynamic_keymap_macro_get_count(void) {
    return DYNAMIC_KEYMAP_MACRO_COUNT;
}

uint16_t dynamic_keymap_macro_get_buffer_size(void) {
    return (uint16_t)nvm_dynamic_keymap_macro_size();
}

void dynamic_keymap_macro_get_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    nvm_dynamic_keymap_macro_read_buffer(offset, size, data);
}

void dynamic_keymap_macro_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    nvm_dynamic_keymap_macro_update_buffer(offset, size, data);
}

static uint8_t dynamic_keymap_read_byte(uint32_t offset) {
    uint8_t d;
    nvm_dynamic_keymap_macro_read_buffer(offset, 1, &d);
    return d;
}

static uint16_t decode_keycode(uint16_t keycode) {
    /*
     * Vial encodes keycodes with a zero low-byte specially because
     * zero terminates a dynamic macro.
     *
     * 0xFF01 -> 0x0100
     * 0xFF02 -> 0x0200
     * ...
     */
    if (keycode > 0xFF00) {
        return (keycode & 0xFF) << 8;
    }

    return keycode;
}

void dynamic_keymap_macro_reset(void) {
    // Erase the macros, if necessary.
    nvm_dynamic_keymap_macro_erase();
    nvm_dynamic_keymap_macro_reset();
}

void dynamic_keymap_macro_send(uint8_t id) {
    if (id >= DYNAMIC_KEYMAP_MACRO_COUNT) {
        return;
    }

    /*
     * The final byte is the valid flag.
     * A non-zero value means a macro-buffer write may still be
     * in progress or may have been interrupted.
     */
    if (dynamic_keymap_read_byte(nvm_dynamic_keymap_macro_size() - 1) != 0) {
        return;
    }

    /*
     * Skip N null terminators.
     * offset will then point to the requested macro.
     */
    uint32_t offset = 0;
    uint32_t end    = nvm_dynamic_keymap_macro_size();

    while (id > 0) {
        if (offset == end) {
            return;
        }

        if (dynamic_keymap_read_byte(offset) == 0) {
            --id;
        }

        ++offset;
    }

    /*
     * Four bytes are enough for:
     *
     *   SS_QMK_PREFIX
     *   action
     *   keycode byte 1
     *   keycode byte 2
     */
    char data[4] = {0, 0, 0, 0};

    /*
     * The valid-flag check above guarantees there is a terminating
     * zero before the end of the usable macro buffer.
     */
    while (1) {
        memset(data, 0, sizeof(data));

        data[0] = dynamic_keymap_read_byte(offset++);

        if (data[0] == 0) {
            break;
        }

        if (data[0] == SS_QMK_PREFIX) {
            data[1] = dynamic_keymap_read_byte(offset++);

            if (data[1] == 0) {
                break;
            }

            /*
             * Standard QMK/VIA 8-bit macro actions.
             * This is also the format used by the existing
             * keyboard-side macro recorder.
             */
            if (data[1] == SS_TAP_CODE ||
                data[1] == SS_DOWN_CODE ||
                data[1] == SS_UP_CODE) {

                data[2] = dynamic_keymap_read_byte(offset++);

                if (data[2] != 0) {
                    send_string(data);
                }

            /*
             * Vial extended actions with full 16-bit QMK keycodes.
             */
            } else if (data[1] == VIAL_MACRO_EXT_TAP ||
                       data[1] == VIAL_MACRO_EXT_DOWN ||
                       data[1] == VIAL_MACRO_EXT_UP) {

                data[2] = dynamic_keymap_read_byte(offset++);

                if (data[2] != 0) {
                    data[3] = dynamic_keymap_read_byte(offset++);

                    if (data[3] != 0) {
                        uint16_t keycode;

                        memcpy(&keycode, &data[2], sizeof(keycode));
                        keycode = decode_keycode(keycode);

                        switch (data[1]) {
                            case VIAL_MACRO_EXT_TAP:
                                vial_keycode_tap(keycode);
                                break;

                            case VIAL_MACRO_EXT_DOWN:
                                vial_keycode_down(keycode);
                                break;

                            case VIAL_MACRO_EXT_UP:
                                vial_keycode_up(keycode);
                                break;
                        }
                    }
                }

            /*
             * Vial delay encoding.
             */
            } else if (data[1] == SS_DELAY_CODE) {
                uint8_t d0 = dynamic_keymap_read_byte(offset++);
                uint8_t d1 = dynamic_keymap_read_byte(offset++);

                if (d0 == 0 || d1 == 0) {
                    break;
                }

                /*
                 * Zero cannot appear inside a dynamic macro command,
                 * therefore values are stored offset by one and use
                 * a base of 255.
                 */
                int ms = (d0 - 1) + (d1 - 1) * 255;

                while (ms--) {
                    wait_ms(1);
                }
            }

        } else {
            /*
             * Ordinary text remains compatible with VIA/QMK macros.
             */
            send_string_with_delay(data, DYNAMIC_KEYMAP_MACRO_DELAY);
        }
    }
}
