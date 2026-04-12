/*
 * Kopie von encoder_mt6835.c
 * Version: 2026-04-12 v1
 * Hinweis: Backup mit Versionsnummer erstellt.
 */

#include "encoder_mt6835.h"

#include <math.h>
#include <stdio.h>

#include "config.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"

#define MT6835_CMD_BURST_READ 0xAu
#define MT6835_CMD_READ_BYTE 0x3u
#define MT6835_ANGLE_START_ADDR 0x003u
#define MT6835_SPI_HZ 16000000u

static inline uint8_t mt6835_crc8(const uint8_t* data, uint8_t len) {
    // Polynom x^8 + x^2 + x + 1 => 0x07, MSB zuerst.
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

void mt6835_init(void) {
    spi_init(MT6835_SPI_IF, MT6835_SPI_HZ);
    spi_set_format(MT6835_SPI_IF, 8u, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);

    gpio_set_function(PIN_MT6835_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MT6835_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MT6835_MISO, GPIO_FUNC_SPI);

    gpio_init(PIN_MT6835_CSN);
    gpio_set_dir(PIN_MT6835_CSN, GPIO_OUT);
    gpio_put(PIN_MT6835_CSN, 1);
}

bool mt6835_read_sample(mt6835_sample_t* out) {
    if (!out) {
        return false;
    }

    // Kommando-Nibble + 12-Bit-Adresse senden => 2 Bytes plus ein Don't-Care-Byte.
    uint8_t tx[7] = {0};
    uint8_t rx[7] = {0};

    tx[0] = (uint8_t)(MT6835_CMD_BURST_READ << 4);
    tx[1] = (uint8_t)(MT6835_ANGLE_START_ADDR << 4);

    gpio_put(PIN_MT6835_CSN, 0);
    spi_write_read_blocking(MT6835_SPI_IF, tx, rx, sizeof(tx));
    gpio_put(PIN_MT6835_CSN, 1);

    // Der Datenstrom nach dem Kommando entspricht Reg 0x003..0x006.
    const uint8_t reg003 = rx[2];
    const uint8_t reg004 = rx[3];
    const uint8_t reg005 = rx[4];
    const uint8_t reg006 = rx[5];

    const uint32_t angle_raw =
        ((uint32_t)reg003 << 13) |
        ((uint32_t)reg004 << 5) |
        ((uint32_t)(reg005 >> 3) & 0x1Fu);

    const uint8_t status = (uint8_t)(reg005 & 0x07u);
    const uint8_t crc = reg006;

    const uint8_t crc_data[3] = {
        reg003,
        reg004,
        reg005,
    };
    const uint8_t crc_calc = mt6835_crc8(crc_data, 3);

    out->angle_raw = angle_raw;
    out->status = status;
    out->crc = crc;
    out->crc_valid = (crc_calc == crc);

    return true;
}

bool mt6835_read_register(uint16_t address, uint8_t* out_value) {
    if (!out_value || (address > 0x0FFFu)) {
        return false;
    }

    uint8_t tx[3] = {0};
    uint8_t rx[3] = {0};

    tx[0] = (uint8_t)((MT6835_CMD_READ_BYTE << 4) | ((address >> 8) & 0x0Fu));
    tx[1] = (uint8_t)(address & 0xFFu);
    tx[2] = 0u;

    gpio_put(PIN_MT6835_CSN, 0);
    spi_write_read_blocking(MT6835_SPI_IF, tx, rx, sizeof(tx));
    gpio_put(PIN_MT6835_CSN, 1);

    *out_value = rx[2];
    return true;
}

void mt6835_report_registers(void) {
    uint8_t reg001 = 0u;
    uint8_t reg003 = 0u;
    uint8_t reg004 = 0u;
    uint8_t reg005 = 0u;
    uint8_t reg006 = 0u;
    uint8_t reg007 = 0u;
    uint8_t reg008 = 0u;
    uint8_t reg009 = 0u;
    uint8_t reg00A = 0u;
    uint8_t reg00B = 0u;
    uint8_t reg00C = 0u;
    uint8_t reg00D = 0u;
    uint8_t reg00E = 0u;
    uint8_t reg011 = 0u;

    if (!mt6835_read_register(0x001u, &reg001) ||
        !mt6835_read_register(0x003u, &reg003) ||
        !mt6835_read_register(0x004u, &reg004) ||
        !mt6835_read_register(0x005u, &reg005) ||
        !mt6835_read_register(0x006u, &reg006) ||
        !mt6835_read_register(0x007u, &reg007) ||
        !mt6835_read_register(0x008u, &reg008) ||
        !mt6835_read_register(0x009u, &reg009) ||
        !mt6835_read_register(0x00Au, &reg00A) ||
        !mt6835_read_register(0x00Bu, &reg00B) ||
        !mt6835_read_register(0x00Cu, &reg00C) ||
        !mt6835_read_register(0x00Du, &reg00D) ||
        !mt6835_read_register(0x00Eu, &reg00E) ||
        !mt6835_read_register(0x011u, &reg011)) {
        printf("MT6835 REG read failed\n");
        return;
    }

    const uint32_t angle_raw =
        ((uint32_t)reg003 << 13) |
        ((uint32_t)reg004 << 5) |
        ((uint32_t)(reg005 >> 3) & 0x1Fu);
    const float angle_deg = mt6835_raw_to_degrees(angle_raw);
    const uint8_t status = (uint8_t)(reg005 & 0x07u);

    const uint8_t crc_data[3] = { reg003, reg004, reg005 };
    const uint8_t crc_calc = mt6835_crc8(crc_data, 3);
    const uint8_t crc_ok = (crc_calc == reg006) ? 1u : 0u;

    const uint32_t abz_ppr = (((uint32_t)reg007 << 6) | ((uint32_t)(reg008 >> 2) & 0x3Fu)) + 1u;
    const uint8_t abz_off = (uint8_t)((reg008 >> 1) & 0x01u);
    const uint8_t ab_swap = (uint8_t)(reg008 & 0x01u);
    const uint16_t zero_pos = (uint16_t)(((uint16_t)reg009 << 4) | ((uint16_t)reg00A >> 4));
    const uint8_t z_edge = (uint8_t)((reg00A >> 3) & 0x01u);
    const uint8_t z_pul_wid = (uint8_t)(reg00A & 0x07u);
    const uint8_t z_phase = (uint8_t)((reg00B >> 6) & 0x03u);
    const uint8_t uvw_mux = (uint8_t)((reg00B >> 5) & 0x01u);
    const uint8_t uvw_off = (uint8_t)((reg00B >> 4) & 0x01u);
    const uint8_t uvw_pairs = (uint8_t)((reg00B & 0x0Fu) + 1u);
    const uint8_t nlc_en = (uint8_t)((reg00C >> 5) & 0x01u);
    const uint8_t pwm_fq = (uint8_t)((reg00C >> 4) & 0x01u);
    const uint8_t pwm_pol = (uint8_t)((reg00C >> 3) & 0x01u);
    const uint8_t pwm_sel = (uint8_t)(reg00C & 0x07u);
    const uint8_t rot_dir = (uint8_t)((reg00D >> 3) & 0x01u);
    const uint8_t hyst = (uint8_t)(reg00D & 0x07u);
    const uint8_t gpio_ds = (uint8_t)((reg00E >> 7) & 0x01u);
    const uint8_t autocal_freq = (uint8_t)((reg00E >> 4) & 0x07u);
    const uint8_t bw = (uint8_t)(reg011 & 0x07u);

    printf("MT6835 REG cfg uid=0x%02X abz_ppr=%lu abz_off=%u ab_swap=%u zero=0x%03X z_edge=%u z_wid=%u z_phase=%u\n",
           (unsigned)reg001,
           (unsigned long)abz_ppr,
           (unsigned)abz_off,
           (unsigned)ab_swap,
           (unsigned)zero_pos,
           (unsigned)z_edge,
           (unsigned)z_pul_wid,
           (unsigned)z_phase);

    printf("MT6835 REG io uvw_mux=%u uvw_off=%u uvw_pairs=%u nlc=%u pwm_fq=%u pwm_pol=%u pwm_sel=%u rot_dir=%u hyst=%u gpio_ds=%u autocal=%u bw=%u\n",
           (unsigned)uvw_mux,
           (unsigned)uvw_off,
           (unsigned)uvw_pairs,
           (unsigned)nlc_en,
           (unsigned)pwm_fq,
           (unsigned)pwm_pol,
           (unsigned)pwm_sel,
           (unsigned)rot_dir,
           (unsigned)hyst,
           (unsigned)gpio_ds,
           (unsigned)autocal_freq,
           (unsigned)bw);

    printf("MT6835 REG live raw=%lu deg=%.2f st=0x%02X ovspd=%u weak=%u uv=%u crc=0x%02X crc_ok=%u\n",
           (unsigned long)angle_raw,
           (double)angle_deg,
           (unsigned)status,
           (unsigned)(status & 0x01u),
           (unsigned)((status >> 1) & 0x01u),
           (unsigned)((status >> 2) & 0x01u),
           (unsigned)reg006,
           (unsigned)crc_ok);
}

float mt6835_raw_to_degrees(uint32_t raw) {
    raw &= (MT6835_ANGLE_MAX - 1u);
    return ((float)raw * 360.0f) / (float)MT6835_ANGLE_MAX;
}

float mt6835_compute_rpm(uint32_t prev_raw, uint32_t curr_raw, float dt_seconds) {
    if (dt_seconds <= 0.0f) {
        return 0.0f;
    }

    int32_t delta = (int32_t)(curr_raw - prev_raw);
    const int32_t half = (int32_t)(MT6835_ANGLE_MAX / 2u);

    // Auf kuerzeste Winkeldistanz wrappen.
    if (delta > half) {
        delta -= (int32_t)MT6835_ANGLE_MAX;
    } else if (delta < -half) {
        delta += (int32_t)MT6835_ANGLE_MAX;
    }

    const float rev_per_sec = ((float)delta / (float)MT6835_ANGLE_MAX) / dt_seconds;
    return rev_per_sec * 60.0f;
}
