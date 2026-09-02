// Copyright 2022-2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "layer_lock.h"
#include "process_layer_lock.h"
#include "quantum_keycodes.h"
#include "action_util.h"

// The current lock state. The kth bit is on if layer k is locked.
extern layer_state_t locked_layers;

static layer_state_t unlock_on_release_layers;
#ifndef NO_ACTION_TAPPING
static layer_state_t tt_expected_on_layers;
#endif

// Handles an event on an `MO` or `TT` layer switch key.
static inline bool handle_mo_or_tt(uint8_t layer, keyrecord_t* record, bool is_tt) {
#ifdef NO_ACTION_TAPPING
#    ifdef NO_ACTION_TAPPING_TAP_TOGGLE_MO
    is_tt = false;
#    else
    if (is_tt) {
        if (is_layer_locked(layer)) {
            if (record->event.pressed) {
                layer_lock_invert(layer);
            }
            return false;
        }
        return true;
    }
#    endif
#endif

    const layer_state_t bit = (layer_state_t)1 << layer;

    if (record->event.pressed) {
        if (is_layer_locked(layer)) {
            unlock_on_release_layers |= bit;
            return !is_tt;
        }
        unlock_on_release_layers &= ~bit;
#ifndef NO_ACTION_TAPPING
        if (is_tt && record->tap.count == 0) {
            tt_expected_on_layers = layer_state_is(layer) ? tt_expected_on_layers & ~bit : tt_expected_on_layers | bit;
        }
#endif
        return true;
    }

    if (unlock_on_release_layers & bit) {
        unlock_on_release_layers &= ~bit;
        if (is_layer_locked(layer)) {
            layer_lock_off(layer);
        }
        return !is_tt;
    }

    if (is_layer_locked(layer)) {
        return false;
    }
#ifndef NO_ACTION_TAPPING
    if (is_tt && record->tap.count == 0 && layer_state_is(layer) != ((tt_expected_on_layers & bit) != 0)) {
        return false;
    }
#endif
    return true;
}

bool process_layer_lock(uint16_t keycode, keyrecord_t* record) {
#ifndef NO_ACTION_LAYER
    layer_lock_activity_trigger();

    // The intention is that locked layers remain on. If something outside of
    // this feature turned any locked layers off, unlock them.
    if ((locked_layers & ~layer_state) != 0) {
        layer_lock_set_kb(locked_layers &= layer_state);
    }

    if (keycode == QK_LAYER_LOCK) {
        if (record->event.pressed) { // The layer lock key was pressed.
            layer_lock_invert(get_highest_layer(layer_state));
        }
        return false;
    }

    switch (keycode) {
        case QK_MOMENTARY ... QK_MOMENTARY_MAX: // `MO(layer)` keys.
            return handle_mo_or_tt(QK_MOMENTARY_GET_LAYER(keycode), record, false);

        case QK_LAYER_TAP_TOGGLE ... QK_LAYER_TAP_TOGGLE_MAX: // `TT(layer)`.
            return handle_mo_or_tt(QK_LAYER_TAP_TOGGLE_GET_LAYER(keycode), record, true);

        case QK_LAYER_MOD ... QK_LAYER_MOD_MAX: { // `LM(layer, mod)`.
            uint8_t layer = QK_LAYER_MOD_GET_LAYER(keycode);
            if (is_layer_locked(layer)) {
                if (record->event.pressed) { // On press, unlock the layer.
                    layer_lock_invert(layer);
                } else { // On release, clear the mods.
                    clear_mods();
                    send_keyboard_report();
                }
                return false; // Skip default handling.
            }
        } break;

#    ifndef NO_ACTION_TAPPING
        case QK_LAYER_TAP ... QK_LAYER_TAP_MAX: // `LT(layer, key)` keys.
            if (record->tap.count == 0 && !record->event.pressed && is_layer_locked(QK_LAYER_TAP_GET_LAYER(keycode))) {
                // Release event on a held layer-tap key where the layer is locked.
                return false; // Skip default handling so that layer stays on.
            }
            break;
#    endif // NO_ACTION_TAPPING
    }
#endif // NO_ACTION_LAYER
    return true;
}
