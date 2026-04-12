#include "ws2812_status.h"

#include "config.h"

#if MODULE_WS2812_ENABLE

#include <math.h>

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "ws2812.pio.h"

static PIO g_pio = pio0;
static uint g_sm = 0u;
static bool g_init_done = false;
static uint32_t g_last_pixel = 0u;
static const int g_ws2812_t1 = 2;
static const int g_ws2812_t2 = 5;
static const int g_ws2812_t3 = 3;

#ifdef PICO_DEFAULT_WS2812_PIN
static const uint g_ws2812_pin = PICO_DEFAULT_WS2812_PIN;
#else
static const uint g_ws2812_pin = PIN_WS2812_DATA;
#endif

static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static uint8_t to_u8(float x) {
    if (x <= 0.0f) {
        return 0u;
    }
    if (x >= 255.0f) {
        return 255u;
    }
    return (uint8_t)(x + 0.5f);
}

static uint32_t pack_grb(uint8_t r, uint8_t g, uint8_t b) {
#if WS2812_COLOR_ORDER == WS2812_COLOR_ORDER_RGB
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
#else
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
#endif
}

static uint32_t scale_rgb(uint8_t r, uint8_t g, uint8_t b, float scale) {
    const float s = clampf(scale, 0.0f, 1.0f);
    const uint8_t rs = to_u8((float)r * s);
    const uint8_t gs = to_u8((float)g * s);
    const uint8_t bs = to_u8((float)b * s);
    return pack_grb(rs, gs, bs);
}

static void put_pixel(uint32_t pixel_grb) {
    // WS2812 erwartet top-ausgerichtete 24-Bit-Farbe im TX-FIFO.
    pio_sm_put_blocking(g_pio, g_sm, pixel_grb << 8u);
}

static void show_color_blocking(uint8_t r, uint8_t g, uint8_t b, uint32_t hold_ms) {
    const uint32_t pixel = scale_rgb(r, g, b, WS2812_BRIGHTNESS);
    put_pixel(pixel);
    g_last_pixel = pixel;
    if (hold_ms > 0u) {
        sleep_ms(hold_ms);
    }
}

void ws2812_status_init(void) {
#ifdef PICO_DEFAULT_WS2812_POWER_PIN
    gpio_init(PICO_DEFAULT_WS2812_POWER_PIN);
    gpio_set_dir(PICO_DEFAULT_WS2812_POWER_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_WS2812_POWER_PIN, 1);
#endif

    const uint offset = pio_add_program(g_pio, &ws2812_program);
    ws2812_program_init(g_pio, g_sm, offset, g_ws2812_pin, 800000.0f, (WS2812_IS_RGBW != 0u));

    g_init_done = true;

#if WS2812_STARTUP_TEST_ENABLE
    show_color_blocking(255u, 0u, 0u, WS2812_STARTUP_TEST_MS);
    show_color_blocking(0u, 255u, 0u, WS2812_STARTUP_TEST_MS);
    show_color_blocking(0u, 0u, 255u, WS2812_STARTUP_TEST_MS);
    show_color_blocking(255u, 255u, 255u, WS2812_STARTUP_TEST_MS);
#endif

    g_last_pixel = scale_rgb(0u, 0u, 0u, 1.0f);
    put_pixel(g_last_pixel);
}

void ws2812_status_update(uint32_t status_bits,
                          bool fault_active,
                          bool motor_enabled,
                          uint32_t now_ms,
                          uint32_t remora_rx_age_ms) {
    if (!g_init_done) {
        return;
    }

    uint32_t pixel = 0u;

#if WS2812_FORCE_WHITE_TEST
    (void)status_bits;
    (void)fault_active;
    (void)motor_enabled;
    (void)now_ms;
    (void)remora_rx_age_ms;
    pixel = scale_rgb(255u, 255u, 255u, WS2812_BRIGHTNESS);
#else

    if ((status_bits & (1u << STATUS_BIT_WATCHDOG)) != 0u) {
        // Schnelles rotes Blinken bei Watchdog-Timeout.
        const bool on = ((now_ms / 125u) & 1u) != 0u;
        pixel = on ? scale_rgb(255u, 0u, 0u, WS2812_BRIGHTNESS) : 0u;
    } else if (fault_active) {
        // Konstantes Amber bei jedem nicht-Watchdog-Fehler.
        pixel = scale_rgb(255u, 80u, 0u, WS2812_BRIGHTNESS);
    } else if (remora_rx_age_ms == UINT32_MAX) {
        // LAN-Stack offline/nicht verfuegbar: magenta Puls.
        const bool on = ((now_ms / 400u) & 1u) == 0u;
        pixel = on ? scale_rgb(255u, 0u, 255u, WS2812_BRIGHTNESS) : 0u;
    } else if (remora_rx_age_ms > REMORA_HB_TIMEOUT_MS) {
        // LAN aktiv, aber zuletzt kein Heartbeat: gelbes Blinken.
        const bool on = ((now_ms / 250u) & 1u) == 0u;
        pixel = on ? scale_rgb(255u, 255u, 0u, WS2812_BRIGHTNESS) : 0u;
    } else if (motor_enabled) {
        // Heartbeat aktiv und Motor freigegeben: gruen.
        pixel = scale_rgb(0u, 255u, 0u, WS2812_BRIGHTNESS);
    } else {
        // Heartbeat aktiv, Motor nicht freigegeben: cyan.
        pixel = scale_rgb(0u, 180u, 255u, WS2812_BRIGHTNESS);
    }
#endif

    if (pixel != g_last_pixel) {
        put_pixel(pixel);
        g_last_pixel = pixel;
    }
}

#else

void ws2812_status_init(void) {
}

void ws2812_status_update(uint32_t status_bits,
                          bool fault_active,
                          bool motor_enabled,
                          uint32_t now_ms,
                          uint32_t remora_rx_age_ms) {
    (void)status_bits;
    (void)fault_active;
    (void)motor_enabled;
    (void)now_ms;
    (void)remora_rx_age_ms;
}

#endif
