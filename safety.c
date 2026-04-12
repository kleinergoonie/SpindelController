#include "safety.h"

#include <math.h>

#include "config.h"
#include "hardware/gpio.h"

static safety_limits_t g_limits;
static safety_state_t g_state;
static uint32_t g_last_watchdog_kick_ms;

void safety_init(void) {
    gpio_init(PIN_ESTOP);
    gpio_set_dir(PIN_ESTOP, GPIO_IN);
    gpio_pull_up(PIN_ESTOP);

    g_limits.overcurrent_threshold_a = 20.0f;
    g_limits.overtemp_threshold_adc = 3000.0f;
    g_limits.watchdog_timeout_ms = 100u;

    g_state.estop_active = false;
    g_state.overcurrent = false;
    g_state.overtemp = false;
    g_state.watchdog_timeout = false;

    g_last_watchdog_kick_ms = 0u;
}

void safety_set_limits(const safety_limits_t* limits) {
    if (!limits) {
        return;
    }
    g_limits = *limits;
}

void safety_kick_watchdog(uint32_t now_ms) {
    g_last_watchdog_kick_ms = now_ms;
}

void safety_update(float current_a, float adc_temp, uint32_t now_ms) {
    g_state.estop_active = (gpio_get(PIN_ESTOP) == 0);
    g_state.overcurrent = (fabsf(current_a) > g_limits.overcurrent_threshold_a);
    g_state.overtemp = (adc_temp > g_limits.overtemp_threshold_adc);

    g_state.watchdog_timeout = ((now_ms - g_last_watchdog_kick_ms) > g_limits.watchdog_timeout_ms);
}

const safety_state_t* safety_get_state(void) {
    return &g_state;
}

const safety_limits_t* safety_get_limits(void) {
    return &g_limits;
}
