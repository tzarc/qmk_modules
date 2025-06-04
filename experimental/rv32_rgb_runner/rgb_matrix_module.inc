// Copyright 2024 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
RGB_MATRIX_EFFECT(rv32_effect)

#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

extern void rv32vm_effect_init_impl(effect_params_t* params);
extern void rv32vm_effect_begin_iter_impl(effect_params_t* params, uint8_t led_min, uint8_t led_max);
extern void rv32vm_effect_led_impl(effect_params_t* params, uint8_t led_index);
extern void rv32vm_effect_end_iter_impl(effect_params_t* params);

static void rv32_effect_init(effect_params_t* params) {
    rv32vm_effect_init_impl(params);
}
static bool rv32_effect_run(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    rv32vm_effect_begin_iter_impl(params, led_min, led_max);
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        rv32vm_effect_led_impl(params, i);
    }
    rv32vm_effect_end_iter_impl(params);
    return rgb_matrix_check_finished_leds(led_max);
}
static bool rv32_effect(effect_params_t* params) {
    if (params->init && params->iter == 0) rv32_effect_init(params);
    return rv32_effect_run(params);
}

#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
