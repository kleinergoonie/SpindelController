#include "spindle_ctrl.h"

#include "config.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include <stdbool.h>
#include <math.h>

static uint g_slice = 0u;
static uint16_t g_wrap = 0u;
static bool g_init = false;

void spindle_ctrl_init(void) {
    if (g_init) return;

    gpio_set_function(PIN_SPINDLE_CTRL, GPIO_FUNC_PWM);
    g_slice = pwm_gpio_to_slice_num(PIN_SPINDLE_CTRL);

    const uint32_t sys_hz = clock_get_hz(clk_sys);
    g_wrap = (uint16_t)(sys_hz / SPINDLE_CTRL_FREQ_HZ - 1u);

    pwm_set_wrap(g_slice, g_wrap);
    pwm_set_enabled(g_slice, true);

    pwm_set_gpio_level(PIN_SPINDLE_CTRL, 0u);
    g_init = true;
}

void spindle_ctrl_set_normalized(float n01) {
    if (!g_init) spindle_ctrl_init();

    if (n01 <= 0.0f) {
        pwm_set_gpio_level(PIN_SPINDLE_CTRL, 0u);
        return;
    }
    if (n01 >= 1.0f) {
        pwm_set_gpio_level(PIN_SPINDLE_CTRL, g_wrap);
        return;
    }

    const uint16_t level = (uint16_t)(n01 * (float)g_wrap + 0.5f);
    pwm_set_gpio_level(PIN_SPINDLE_CTRL, level);
}

void spindle_ctrl_set_voltage(float volts) {
    // 0..SPINDLE_CTRL_VMAX_V auf 0..1 abbilden
    float n = volts / SPINDLE_CTRL_VMAX_V;
    if (n < 0.0f) n = 0.0f;
    if (n > 1.0f) n = 1.0f;
    spindle_ctrl_set_normalized(n);
}
