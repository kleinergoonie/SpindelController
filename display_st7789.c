#include "display_st7789.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#define DISPLAY_SPI_HZ SPI0_BUS_HZ

#define ST7789_CMD_SWRESET 0x01
#define ST7789_CMD_SLPOUT  0x11
#define ST7789_CMD_COLMOD  0x3A
#define ST7789_CMD_MADCTL  0x36
#define ST7789_CMD_CASET   0x2A
#define ST7789_CMD_RASET   0x2B
#define ST7789_CMD_RAMWR   0x2C
#define ST7789_CMD_DISPON  0x29

#define COLOR_BG       0x0000u
#define COLOR_TEXT     0xFFFFu
#define COLOR_ACCENT   0x07E0u
#define COLOR_WARN     0xF800u
#define COLOR_MUTED    0x8410u

static inline void st7789_write_cmd(uint8_t cmd) {
    gpio_put(PIN_ST7789_DC, 0);
    gpio_put(PIN_ST7789_CS, 0);
    spi_write_blocking(spi0, &cmd, 1);
    gpio_put(PIN_ST7789_CS, 1);
}

static inline void st7789_write_data(const uint8_t* data, size_t len) {
    gpio_put(PIN_ST7789_DC, 1);
    gpio_put(PIN_ST7789_CS, 0);
    spi_write_blocking(spi0, data, (size_t)len);
    gpio_put(PIN_ST7789_CS, 1);
}

static inline void st7789_write_u16be(uint16_t v) {
    uint8_t data[2] = {
        (uint8_t)((v >> 8) & 0xFFu),
        (uint8_t)(v & 0xFFu),
    };
    st7789_write_data(data, sizeof(data));
}

static void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t col[4] = {
        (uint8_t)((x0 >> 8) & 0xFFu),
        (uint8_t)(x0 & 0xFFu),
        (uint8_t)((x1 >> 8) & 0xFFu),
        (uint8_t)(x1 & 0xFFu),
    };
    uint8_t row[4] = {
        (uint8_t)((y0 >> 8) & 0xFFu),
        (uint8_t)(y0 & 0xFFu),
        (uint8_t)((y1 >> 8) & 0xFFu),
        (uint8_t)(y1 & 0xFFu),
    };

    st7789_write_cmd(ST7789_CMD_CASET);
    st7789_write_data(col, sizeof(col));
    st7789_write_cmd(ST7789_CMD_RASET);
    st7789_write_data(row, sizeof(row));
    st7789_write_cmd(ST7789_CMD_RAMWR);
}

static void st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (w == 0u || h == 0u) {
        return;
    }
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        return;
    }

    uint16_t x1 = (uint16_t)(x + w - 1u);
    uint16_t y1 = (uint16_t)(y + h - 1u);
    if (x1 >= DISPLAY_WIDTH) {
        x1 = (uint16_t)(DISPLAY_WIDTH - 1u);
    }
    if (y1 >= DISPLAY_HEIGHT) {
        y1 = (uint16_t)(DISPLAY_HEIGHT - 1u);
    }

    st7789_set_window(x, y, x1, y1);

    const uint32_t pixels = (uint32_t)(x1 - x + 1u) * (uint32_t)(y1 - y + 1u);
    uint8_t chunk[128];
    for (size_t i = 0; i < sizeof(chunk); i += 2) {
        chunk[i] = (uint8_t)((color >> 8) & 0xFFu);
        chunk[i + 1] = (uint8_t)(color & 0xFFu);
    }

    uint32_t remaining = pixels;
    while (remaining > 0u) {
        uint32_t n = remaining;
        const uint32_t max_chunk_pixels = (uint32_t)(sizeof(chunk) / 2u);
        if (n > max_chunk_pixels) {
            n = max_chunk_pixels;
        }
        st7789_write_data(chunk, (size_t)(n * 2u));
        remaining -= n;
    }
}

static bool glyph5x7(char c, uint8_t out[5]) {
    memset(out, 0, 5);
    switch (c) {
        case '0': { uint8_t g[5] = {0x3E,0x51,0x49,0x45,0x3E}; memcpy(out,g,5); return true; }
        case '1': { uint8_t g[5] = {0x00,0x42,0x7F,0x40,0x00}; memcpy(out,g,5); return true; }
        case '2': { uint8_t g[5] = {0x42,0x61,0x51,0x49,0x46}; memcpy(out,g,5); return true; }
        case '3': { uint8_t g[5] = {0x21,0x41,0x45,0x4B,0x31}; memcpy(out,g,5); return true; }
        case '4': { uint8_t g[5] = {0x18,0x14,0x12,0x7F,0x10}; memcpy(out,g,5); return true; }
        case '5': { uint8_t g[5] = {0x27,0x45,0x45,0x45,0x39}; memcpy(out,g,5); return true; }
        case '6': { uint8_t g[5] = {0x3C,0x4A,0x49,0x49,0x30}; memcpy(out,g,5); return true; }
        case '7': { uint8_t g[5] = {0x01,0x71,0x09,0x05,0x03}; memcpy(out,g,5); return true; }
        case '8': { uint8_t g[5] = {0x36,0x49,0x49,0x49,0x36}; memcpy(out,g,5); return true; }
        case '9': { uint8_t g[5] = {0x06,0x49,0x49,0x29,0x1E}; memcpy(out,g,5); return true; }
        case 'A': { uint8_t g[5] = {0x7E,0x11,0x11,0x11,0x7E}; memcpy(out,g,5); return true; }
        case 'B': { uint8_t g[5] = {0x7F,0x49,0x49,0x49,0x36}; memcpy(out,g,5); return true; }
        case 'C': { uint8_t g[5] = {0x3E,0x41,0x41,0x41,0x22}; memcpy(out,g,5); return true; }
        case 'D': { uint8_t g[5] = {0x7F,0x41,0x41,0x22,0x1C}; memcpy(out,g,5); return true; }
        case 'E': { uint8_t g[5] = {0x7F,0x49,0x49,0x49,0x41}; memcpy(out,g,5); return true; }
        case 'F': { uint8_t g[5] = {0x7F,0x09,0x09,0x09,0x01}; memcpy(out,g,5); return true; }
        case 'G': { uint8_t g[5] = {0x3E,0x41,0x49,0x49,0x3A}; memcpy(out,g,5); return true; }
        case 'H': { uint8_t g[5] = {0x7F,0x08,0x08,0x08,0x7F}; memcpy(out,g,5); return true; }
        case 'I': { uint8_t g[5] = {0x00,0x41,0x7F,0x41,0x00}; memcpy(out,g,5); return true; }
        case 'J': { uint8_t g[5] = {0x20,0x40,0x41,0x3F,0x01}; memcpy(out,g,5); return true; }
        case 'K': { uint8_t g[5] = {0x7F,0x08,0x14,0x22,0x41}; memcpy(out,g,5); return true; }
        case 'L': { uint8_t g[5] = {0x7F,0x40,0x40,0x40,0x40}; memcpy(out,g,5); return true; }
        case 'M': { uint8_t g[5] = {0x7F,0x02,0x0C,0x02,0x7F}; memcpy(out,g,5); return true; }
        case 'N': { uint8_t g[5] = {0x7F,0x04,0x08,0x10,0x7F}; memcpy(out,g,5); return true; }
        case 'O': { uint8_t g[5] = {0x3E,0x41,0x41,0x41,0x3E}; memcpy(out,g,5); return true; }
        case 'P': { uint8_t g[5] = {0x7F,0x09,0x09,0x09,0x06}; memcpy(out,g,5); return true; }
        case 'Q': { uint8_t g[5] = {0x3E,0x41,0x51,0x21,0x5E}; memcpy(out,g,5); return true; }
        case 'R': { uint8_t g[5] = {0x7F,0x09,0x19,0x29,0x46}; memcpy(out,g,5); return true; }
        case 'S': { uint8_t g[5] = {0x46,0x49,0x49,0x49,0x31}; memcpy(out,g,5); return true; }
        case 'T': { uint8_t g[5] = {0x01,0x01,0x7F,0x01,0x01}; memcpy(out,g,5); return true; }
        case 'U': { uint8_t g[5] = {0x3F,0x40,0x40,0x40,0x3F}; memcpy(out,g,5); return true; }
        case 'V': { uint8_t g[5] = {0x1F,0x20,0x40,0x20,0x1F}; memcpy(out,g,5); return true; }
        case 'W': { uint8_t g[5] = {0x7F,0x20,0x18,0x20,0x7F}; memcpy(out,g,5); return true; }
        case 'X': { uint8_t g[5] = {0x63,0x14,0x08,0x14,0x63}; memcpy(out,g,5); return true; }
        case 'Y': { uint8_t g[5] = {0x03,0x04,0x78,0x04,0x03}; memcpy(out,g,5); return true; }
        case 'Z': { uint8_t g[5] = {0x61,0x51,0x49,0x45,0x43}; memcpy(out,g,5); return true; }
        case ' ': return true;
        case '.': { uint8_t g[5] = {0x00,0x60,0x60,0x00,0x00}; memcpy(out,g,5); return true; }
        case ':': { uint8_t g[5] = {0x00,0x36,0x36,0x00,0x00}; memcpy(out,g,5); return true; }
        case '-': { uint8_t g[5] = {0x08,0x08,0x08,0x08,0x08}; memcpy(out,g,5); return true; }
        case '/': { uint8_t g[5] = {0x20,0x10,0x08,0x04,0x02}; memcpy(out,g,5); return true; }
        case '%': { uint8_t g[5] = {0x62,0x64,0x08,0x13,0x23}; memcpy(out,g,5); return true; }
        case '=': { uint8_t g[5] = {0x14,0x14,0x14,0x14,0x14}; memcpy(out,g,5); return true; }
        case '(': { uint8_t g[5] = {0x00,0x1C,0x22,0x41,0x00}; memcpy(out,g,5); return true; }
        case ')': { uint8_t g[5] = {0x00,0x41,0x22,0x1C,0x00}; memcpy(out,g,5); return true; }
        case '|': { uint8_t g[5] = {0x00,0x00,0x7F,0x00,0x00}; memcpy(out,g,5); return true; }
        default:
            return false;
    }
}

static void st7789_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale) {
    if (scale == 0u) {
        return;
    }

    uint8_t glyph[5];
    if (!glyph5x7(c, glyph)) {
        c = '?';
        if (!glyph5x7(c, glyph)) {
            return;
        }
    }

    const uint16_t char_w = (uint16_t)(6u * scale);
    const uint16_t char_h = (uint16_t)(8u * scale);
    st7789_fill_rect(x, y, char_w, char_h, bg);

    for (uint8_t col = 0; col < 5u; ++col) {
        uint8_t bits = glyph[col];
        for (uint8_t row = 0; row < 7u; ++row) {
            if ((bits & (1u << row)) != 0u) {
                st7789_fill_rect(
                    (uint16_t)(x + col * scale),
                    (uint16_t)(y + row * scale),
                    scale,
                    scale,
                    fg
                );
            }
        }
    }
}

static void st7789_draw_text(uint16_t x, uint16_t y, const char* text, uint16_t fg, uint16_t bg, uint8_t scale) {
    if (!text) {
        return;
    }

    uint16_t cursor_x = x;
    while (*text != '\0') {
        st7789_draw_char(cursor_x, y, *text, fg, bg, scale);
        cursor_x = (uint16_t)(cursor_x + (uint16_t)(6u * scale));
        ++text;
        if (cursor_x >= DISPLAY_WIDTH) {
            break;
        }
    }
}

void display_st7789_init(void) {
    gpio_init(PIN_ST7789_CS);
    gpio_set_dir(PIN_ST7789_CS, GPIO_OUT);
    gpio_put(PIN_ST7789_CS, 1);

    gpio_init(PIN_ST7789_DC);
    gpio_set_dir(PIN_ST7789_DC, GPIO_OUT);
    gpio_put(PIN_ST7789_DC, 0);

    gpio_init(PIN_ST7789_RST);
    gpio_set_dir(PIN_ST7789_RST, GPIO_OUT);

    gpio_init(PIN_ST7789_BL);
    gpio_set_dir(PIN_ST7789_BL, GPIO_OUT);

    spi_init(spi0, DISPLAY_SPI_HZ);
    spi_set_format(spi0, 8u, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(PIN_W5500_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_W5500_MOSI, GPIO_FUNC_SPI);

    // Reset-Sequenz.
    gpio_put(PIN_ST7789_RST, 0);
    sleep_ms(10);
    gpio_put(PIN_ST7789_RST, 1);
    sleep_ms(120);

    st7789_write_cmd(ST7789_CMD_SWRESET);
    sleep_ms(150);

    st7789_write_cmd(ST7789_CMD_SLPOUT);
    sleep_ms(120);

        // 16-Bit-RGB565-Pixelformat.
    st7789_write_cmd(ST7789_CMD_COLMOD);
    const uint8_t colmod = 0x55;
    st7789_write_data(&colmod, 1);

        // Feste Ausrichtung im Querformat.
    st7789_write_cmd(ST7789_CMD_MADCTL);
        const uint8_t madctl =
    #if DISPLAY_MODEL_SELECT == DISPLAY_MODEL_MCUFRIEND397
        0x28; // MX + BGR, common for ILI9486/ILI9488 landscape
    #else
        0x60; // ST7789 landscape
    #endif
    st7789_write_data(&madctl, 1);

    st7789_write_cmd(ST7789_CMD_DISPON);
    sleep_ms(20);

    st7789_fill_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, COLOR_BG);

    display_st7789_set_backlight(true);
}

void display_st7789_set_backlight(bool on) {
    gpio_put(PIN_ST7789_BL, on ? 1 : 0);
}

void display_st7789_update_status(float setpoint_rpm,
                                  float measured_rpm,
                                  float duty,
                                  float torque_nm,
                                  bool enabled,
                                  bool fault_active,
                                  uint32_t status_bits,
                                  uint8_t motor_mode) {
    static bool initialized = false;
    static bool last_enabled = false;
    static bool last_fault = false;
    static uint32_t last_status_bits = 0u;
    static uint8_t last_mode = 0xFFu;
    static int16_t last_setpoint = -1;
    static int16_t last_measured = -1;
    static int16_t last_duty_tenths = -1;
    static int16_t last_torque_centi = -32768;
    static uint32_t last_log_ms = 0u;

    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    const int16_t setpoint_i = (int16_t)(setpoint_rpm + 0.5f);
    const int16_t measured_i = (int16_t)(measured_rpm + 0.5f);
    const int16_t duty_tenths = (int16_t)(duty * 1000.0f + 0.5f);
    const int16_t torque_centi = (int16_t)(torque_nm * 100.0f + (torque_nm >= 0.0f ? 0.5f : -0.5f));

    const bool heartbeat_due = (now_ms - last_log_ms) >= 500u;
    const bool changed =
        !initialized ||
        (enabled != last_enabled) ||
        (fault_active != last_fault) ||
        (status_bits != last_status_bits) ||
        (motor_mode != last_mode) ||
        (setpoint_i != last_setpoint) ||
        (measured_i != last_measured) ||
        (duty_tenths != last_duty_tenths) ||
        (torque_centi != last_torque_centi);

    if (!changed && !heartbeat_due) {
        return;
    }

    const char* mode_str = "COAST";
    if (motor_mode == 1u) {
        mode_str = "FWD";
    } else if (motor_mode == 2u) {
        mode_str = "REV";
    } else if (motor_mode == 3u) {
        mode_str = "BRAKE";
    }

#if ENCODER_TYPE_SELECT == ENCODER_TYPE_MT6835
    const char* encoder_str = "MT6835(SPI)";
#else
    const char* encoder_str = "AS5600(I2C)";
#endif

    char line[64];
    const uint16_t bg = COLOR_BG;
    const uint16_t fg = fault_active ? COLOR_WARN : COLOR_TEXT;

    st7789_fill_rect(0, 0, DISPLAY_WIDTH, 20, bg);
    st7789_draw_text(4, 4, "SPINDLE CONTROLLER", COLOR_ACCENT, bg, 2);

    st7789_fill_rect(0, 24, DISPLAY_WIDTH, 12, bg);
    snprintf(line, sizeof(line), "SET %5d RPM", (int)setpoint_i);
    st7789_draw_text(4, 26, line, fg, bg, 1);

    st7789_fill_rect(0, 38, DISPLAY_WIDTH, 12, bg);
    snprintf(line, sizeof(line), "ACT %5d RPM", (int)measured_i);
    st7789_draw_text(4, 40, line, fg, bg, 1);

    st7789_fill_rect(0, 52, DISPLAY_WIDTH, 12, bg);
    snprintf(line, sizeof(line), "DUTY %4d.%d%%  %s", duty_tenths / 10, duty_tenths % 10, mode_str);
    st7789_draw_text(4, 54, line, fg, bg, 1);

    st7789_fill_rect(0, 66, DISPLAY_WIDTH, 12, bg);
    snprintf(line, sizeof(line), "EN %d  FLT %d  ST 0x%02lX", enabled ? 1 : 0, fault_active ? 1 : 0, (unsigned long)(status_bits & 0xFFu));
    st7789_draw_text(4, 68, line, fg, bg, 1);

    st7789_fill_rect(0, 80, DISPLAY_WIDTH, 12, bg);
    snprintf(line, sizeof(line), "%s%s%s%s%s%s%s",
             ((status_bits & (1u << STATUS_BIT_ESTOP)) != 0u) ? "ESTOP " : "",
             ((status_bits & (1u << STATUS_BIT_OVERCURRENT)) != 0u) ? "OC " : "",
             ((status_bits & (1u << STATUS_BIT_OVERTEMP)) != 0u) ? "OT " : "",
             ((status_bits & (1u << STATUS_BIT_WATCHDOG)) != 0u) ? "WD " : "",
             ((status_bits & (1u << STATUS_BIT_FAULT_LATCHED)) != 0u) ? "LAT " : "",
             ((status_bits & (1u << STATUS_BIT_ENCODER_FAULT)) != 0u) ? "ENC " : "",
             ((status_bits & (1u << STATUS_BIT_POSITION_DISABLED)) != 0u) ? "PDIS" : "");
    st7789_draw_text(4, 82, line[0] ? line : "OK", fg, bg, 1);

    st7789_fill_rect(0, 96, DISPLAY_WIDTH, 12, bg);
    snprintf(line, sizeof(line), "ENCODER %s", encoder_str);
    st7789_draw_text(4, 98, line, COLOR_MUTED, bg, 1);

    st7789_fill_rect(0, 110, DISPLAY_WIDTH, 12, bg);
    const char torque_sign = (torque_centi < 0) ? '-' : '+';
    const int torque_abs_centi = abs((int)torque_centi);
    snprintf(line, sizeof(line), "TORQ %c%d.%02d NM", torque_sign, torque_abs_centi / 100, torque_abs_centi % 100);
    st7789_draw_text(4, 112, line, COLOR_TEXT, bg, 1);

    initialized = true;
    last_enabled = enabled;
    last_fault = fault_active;
    last_status_bits = status_bits;
    last_mode = motor_mode;
    last_setpoint = setpoint_i;
    last_measured = measured_i;
    last_duty_tenths = duty_tenths;
    last_torque_centi = torque_centi;
    last_log_ms = now_ms;
}

void display_st7789_update_network(const uint8_t ip[4],
                                   uint16_t port,
                                   bool edit_mode,
                                   uint8_t edit_octet) {
    if (!ip) {
        return;
    }

    char line[64];
    const uint16_t bg = COLOR_BG;

    st7789_fill_rect(0, 126, DISPLAY_WIDTH, 14, bg);
    snprintf(line,
             sizeof(line),
             "REMORA %u.%u.%u.%u:%u",
             (unsigned)ip[0],
             (unsigned)ip[1],
             (unsigned)ip[2],
             (unsigned)ip[3],
             (unsigned)port);
    st7789_draw_text(4, 128, line, COLOR_TEXT, bg, 1);

    st7789_fill_rect(0, 140, DISPLAY_WIDTH, 14, bg);
    if (edit_mode) {
        snprintf(line, sizeof(line), "EDIT OCTET %u  INC|DEC|APPLY", (unsigned)(edit_octet + 1u));
        st7789_draw_text(4, 142, line, COLOR_ACCENT, bg, 1);
    } else {
        st7789_draw_text(4, 142, "TOUCH NET AREA TO EDIT IP", COLOR_MUTED, bg, 1);
    }
}
