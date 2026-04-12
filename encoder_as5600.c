#include "encoder_as5600.h"

#include <stdio.h>

#include "config.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#define AS5600_I2C_HZ 400000u
#define AS5600_REG_STATUS 0x0Bu
#define AS5600_REG_ANGLE_MSB 0x0Eu

static bool as5600_probe_at_addr(uint8_t addr, uint8_t* status_out) {
    const uint8_t reg_status = AS5600_REG_STATUS;
    uint8_t status = 0u;

    const int wrote = i2c_write_blocking(i2c0, addr, &reg_status, 1, true);
    if (wrote != 1) {
        return false;
    }

    const int read = i2c_read_blocking(i2c0, addr, &status, 1, false);
    if (read != 1) {
        return false;
    }

    if (status_out) {
        *status_out = status;
    }
    return true;
}

void as5600_init(void) {
    i2c_init(i2c0, AS5600_I2C_HZ);

    gpio_set_function(PIN_AS5600_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_AS5600_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_AS5600_SDA);
    gpio_pull_up(PIN_AS5600_SCL);
}

void as5600_log_i2c_scan(void) {
    bool found_any = false;
    bool found_target = false;

    printf("AS5600 I2C scan start (SDA=%u SCL=%u expected=0x%02X)\n",
           (unsigned)PIN_AS5600_SDA,
           (unsigned)PIN_AS5600_SCL,
           (unsigned)AS5600_I2C_ADDR);

    for (uint8_t addr = 0x08u; addr <= 0x77u; ++addr) {
        uint8_t status = 0u;
        if (as5600_probe_at_addr(addr, &status)) {
            const bool md = (status & (1u << 5)) != 0u;
            const bool ml = (status & (1u << 4)) != 0u;
            const bool mh = (status & (1u << 3)) != 0u;
            printf("I2C ACK 0x%02X status=0x%02X md=%u ml=%u mh=%u%s\n",
                   (unsigned)addr,
                   (unsigned)status,
                   md ? 1u : 0u,
                   ml ? 1u : 0u,
                   mh ? 1u : 0u,
                   (addr == AS5600_I2C_ADDR) ? " <- configured AS5600" : "");
            found_any = true;
            if (addr == AS5600_I2C_ADDR) {
                found_target = true;
            }
        }
    }

    if (!found_any) {
        printf("I2C scan: no ACK in 0x08..0x77\n");
    } else if (!found_target) {
        printf("I2C scan: configured AS5600 address 0x%02X not found\n", (unsigned)AS5600_I2C_ADDR);
    }
}

bool as5600_read_magnet_status(bool* md, bool* ml, bool* mh) {
    uint8_t status = 0u;
    if (!as5600_probe_at_addr(AS5600_I2C_ADDR, &status)) {
        return false;
    }

    if (md) {
        *md = (status & (1u << 5)) != 0u;
    }
    if (ml) {
        *ml = (status & (1u << 4)) != 0u;
    }
    if (mh) {
        *mh = (status & (1u << 3)) != 0u;
    }
    return true;
}

bool as5600_read_sample(as5600_sample_t* out) {
    if (!out) {
        return false;
    }

    const uint8_t reg = AS5600_REG_ANGLE_MSB;
    uint8_t data[2] = {0u, 0u};

    const int wrote = i2c_write_blocking(i2c0, AS5600_I2C_ADDR, &reg, 1, true);
    if (wrote != 1) {
        return false;
    }

    const int read = i2c_read_blocking(i2c0, AS5600_I2C_ADDR, data, 2, false);
    if (read != 2) {
        return false;
    }

    out->angle_raw = (uint16_t)((((uint16_t)data[0]) << 8) | data[1]);
    out->angle_raw &= (uint16_t)(AS5600_ANGLE_MAX - 1u);

    // Magnetdiagnose lesen (Best Effort):
    // MD Bit5 = Magnet erkannt, ML Bit4 = zu schwach, MH Bit3 = zu stark.
    out->magnet_detected = false;
    out->magnet_too_weak = false;
    out->magnet_too_strong = false;
    (void)as5600_read_magnet_status(&out->magnet_detected, &out->magnet_too_weak, &out->magnet_too_strong);

    return true;
}

float as5600_raw_to_degrees(uint16_t raw) {
    raw &= (uint16_t)(AS5600_ANGLE_MAX - 1u);
    return ((float)raw * 360.0f) / (float)AS5600_ANGLE_MAX;
}

float as5600_compute_rpm(uint16_t prev_raw, uint16_t curr_raw, float dt_seconds) {
    if (dt_seconds <= 0.0f) {
        return 0.0f;
    }

    int32_t delta = (int32_t)curr_raw - (int32_t)prev_raw;
    const int32_t half = (int32_t)(AS5600_ANGLE_MAX / 2u);

    if (delta > half) {
        delta -= (int32_t)AS5600_ANGLE_MAX;
    } else if (delta < -half) {
        delta += (int32_t)AS5600_ANGLE_MAX;
    }

    const float rev_per_sec = ((float)delta / (float)AS5600_ANGLE_MAX) / dt_seconds;
    return rev_per_sec * 60.0f;
}
