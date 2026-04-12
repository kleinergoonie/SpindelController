#include "board_led.h"

#include "config.h"

#if BOARD_HAS_GPIO_LED

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include <stdbool.h>

static uint g_slice = 0u;
static uint16_t g_wrap = 0u;
static bool g_init = false;

void board_led_init(void) {
    if (g_init) return;

    gpio_set_function(PIN_BOARD_LED, GPIO_FUNC_PWM);
    g_slice = pwm_gpio_to_slice_num(PIN_BOARD_LED);

    const uint32_t sys_hz = clock_get_hz(clk_sys);
    const uint32_t led_freq_hz = 1000u; // 1 kHz carrier for LED PWM
    g_wrap = (uint16_t)(sys_hz / led_freq_hz - 1u);

    pwm_set_wrap(g_slice, g_wrap);
    pwm_set_enabled(g_slice, true);

    // Startzustand: aus
    pwm_set_gpio_level(PIN_BOARD_LED, 0u);
    g_init = true;
}

void board_led_set_brightness(float duty01) {
    if (!g_init) board_led_init();

    if (duty01 <= 0.0f) {
        pwm_set_gpio_level(PIN_BOARD_LED, 0u);
        return;
    }
    if (duty01 >= 1.0f) {
        pwm_set_gpio_level(PIN_BOARD_LED, g_wrap);
        return;
    }

    const uint16_t level = (uint16_t)(duty01 * (float)g_wrap + 0.5f);
    pwm_set_gpio_level(PIN_BOARD_LED, level);
}

#else // BOARD_HAS_GPIO_LED == 0: kein GPIO-LED (z.B. RP2040 Zero, LED ist WS2812)

void board_led_init(void) { /* no-op: kein GPIO-LED auf diesem Board */ }
void board_led_set_brightness(float duty01) { (void)duty01; /* no-op */ }

#endif // BOARD_HAS_GPIO_LED
