#include "touch_input.h"

#include <string.h>

#include "config.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"

#if TOUCH_TYPE_SELECT == TOUCH_TYPE_XPT2046

static bool g_prev_pressed = false;

static uint16_t xpt2046_read12(uint8_t cmd) {
    uint8_t tx[3] = { cmd, 0x00u, 0x00u };
    uint8_t rx[3] = { 0u, 0u, 0u };

    gpio_put(PIN_TOUCH_CS, 0);
    spi_write_read_blocking(spi0, tx, rx, 3u);
    gpio_put(PIN_TOUCH_CS, 1);

    return (uint16_t)(((uint16_t)rx[1] << 8) | rx[2]) >> 3;
}

static uint16_t clamp_u16(uint16_t v, uint16_t lo, uint16_t hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static uint16_t map_raw(uint16_t raw, uint16_t in_min, uint16_t in_max, uint16_t out_max) {
    if (in_max <= in_min) {
        return 0u;
    }

    uint32_t num = (uint32_t)(raw - in_min) * (uint32_t)out_max;
    uint32_t den = (uint32_t)(in_max - in_min);
    return (uint16_t)(num / den);
}

#endif

void touch_input_init(void) {
#if TOUCH_TYPE_SELECT == TOUCH_TYPE_XPT2046
    gpio_init(PIN_TOUCH_CS);
    gpio_set_dir(PIN_TOUCH_CS, GPIO_OUT);
    gpio_put(PIN_TOUCH_CS, 1);

    gpio_init(PIN_TOUCH_IRQ);
    gpio_set_dir(PIN_TOUCH_IRQ, GPIO_IN);
    gpio_pull_up(PIN_TOUCH_IRQ);

    g_prev_pressed = false;
#endif
}

bool touch_input_poll(touch_event_t* out_event) {
    if (!out_event) {
        return false;
    }

    memset(out_event, 0, sizeof(*out_event));

#if TOUCH_TYPE_SELECT == TOUCH_TYPE_XPT2046
    const bool pressed = (gpio_get(PIN_TOUCH_IRQ) == 0);
    if (!pressed) {
        g_prev_pressed = false;
        return false;
    }

    uint32_t raw_x_acc = 0u;
    uint32_t raw_y_acc = 0u;
    const uint8_t samples = 4u;
    for (uint8_t i = 0u; i < samples; ++i) {
        raw_x_acc += xpt2046_read12(0xD0u);
        raw_y_acc += xpt2046_read12(0x90u);
    }

    uint16_t raw_x = (uint16_t)(raw_x_acc / samples);
    uint16_t raw_y = (uint16_t)(raw_y_acc / samples);

#if TOUCH_SWAP_XY
    const uint16_t tmp = raw_x;
    raw_x = raw_y;
    raw_y = tmp;
#endif

    raw_x = clamp_u16(raw_x, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX);
    raw_y = clamp_u16(raw_y, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX);

    uint16_t x = map_raw(raw_x, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, (uint16_t)(DISPLAY_WIDTH - 1u));
    uint16_t y = map_raw(raw_y, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, (uint16_t)(DISPLAY_HEIGHT - 1u));

#if TOUCH_INVERT_X
    x = (uint16_t)((DISPLAY_WIDTH - 1u) - x);
#endif
#if TOUCH_INVERT_Y
    y = (uint16_t)((DISPLAY_HEIGHT - 1u) - y);
#endif

    out_event->pressed = true;
    out_event->just_pressed = !g_prev_pressed;
    out_event->x = x;
    out_event->y = y;
    g_prev_pressed = true;
    return true;
#endif

    return false;
}
