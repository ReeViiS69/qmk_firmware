RGB_MATRIX_EFFECT(NEGATIVE_SOLID_REACTIVE)

#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

#include "matrix.h"
#include "sync_timer.h"
#include <string.h>

#define NSR_IDLE 0
#define NSR_HELD UINT32_MAX

/*
 * Pro LED nur noch ein Zustand:
 *
 * 0          = nichts aktiv
 * UINT32_MAX = Taste wird gehalten
 * sonst      = Zeitpunkt des Loslassens
 */
static uint32_t nsr_state[RGB_MATRIX_LED_COUNT];

static bool nsr_led_is_held(uint8_t led) {
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            if (g_led_config.matrix_co[row][col] == led &&
                matrix_is_on(row, col)) {
                return true;
            }
        }
    }

    return false;
}

static bool NEGATIVE_SOLID_REACTIVE(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    if (params->init && params->iter == 0) {
        memset(nsr_state, 0, sizeof(nsr_state));
    }

    uint8_t  speed    = qadd8(rgb_matrix_config.speed, 1);
    uint16_t max_tick = 65535 / speed;

    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();

        hsv_t hsv  = rgb_matrix_config.hsv;
        bool  held = nsr_led_is_held(i);

        /*
         * Taste gedrückt:
         * dauerhaft schwarz.
         */
        if (held) {
            nsr_state[i] = NSR_HELD;
            hsv.v        = 0;
        }

        /*
         * Vorher gehalten, jetzt losgelassen:
         * Fade beginnt JETZT.
         */
        else if (nsr_state[i] == NSR_HELD) {
            uint32_t now = sync_timer_read32();

            /*
             * 0 und UINT32_MAX sind unsere reservierten Zustände.
             */
            if (now == NSR_IDLE) {
                now = 1;
            } else if (now == NSR_HELD) {
                now = UINT32_MAX - 1;
            }

            nsr_state[i] = now;
            hsv.v        = 0;
        }

        /*
         * Fade läuft.
         */
        else if (nsr_state[i] != NSR_IDLE) {
            uint32_t elapsed = sync_timer_elapsed32(nsr_state[i]);

            uint16_t tick = elapsed >= max_tick
                                ? max_tick
                                : (uint16_t)elapsed;

            uint16_t offset = scale16by8(tick, speed);

            if (offset >= 255) {
                nsr_state[i] = NSR_IDLE;
                hsv.v        = rgb_matrix_config.hsv.v;
            } else {
                hsv.v = scale8((uint8_t)offset, hsv.v);
            }
        }

        rgb_t rgb = rgb_matrix_hsv_to_rgb(hsv);

        rgb_matrix_set_color(
            i,
            rgb.r,
            rgb.g,
            rgb.b
        );
    }

    return rgb_matrix_check_finished_leds(led_max);
}

#endif