/* SPDX-License-Identifier: GPL-2.0-or-later */

#if defined(VIALRGB_ENABLE) && !defined(VIALRGB_NO_DIRECT) && defined(RGB_MATRIX_CUSTOM_USER)

RGB_MATRIX_EFFECT(VIALRGB_DIRECT)

#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

extern hsv_t g_direct_mode_colors[RGB_MATRIX_LED_COUNT];

static bool VIALRGB_DIRECT(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    for (uint8_t i = led_min; i < led_max; ++i) {
        rgb_t rgb = rgb_matrix_hsv_to_rgb(g_direct_mode_colors[i]);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }

    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif // VIALRGB_ENABLE && !VIALRGB_NO_DIRECT && RGB_MATRIX_CUSTOM_USER
