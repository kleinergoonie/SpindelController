#ifndef ENCODER_AS5600_H
#define ENCODER_AS5600_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t angle_raw; // 12-bit value
    bool magnet_detected;
    bool magnet_too_weak;
    bool magnet_too_strong;
} as5600_sample_t;

void as5600_init(void);
void as5600_log_i2c_scan(void);
bool as5600_read_magnet_status(bool* md, bool* ml, bool* mh);
bool as5600_read_sample(as5600_sample_t* out);
float as5600_raw_to_degrees(uint16_t raw);
float as5600_compute_rpm(uint16_t prev_raw, uint16_t curr_raw, float dt_seconds);

#endif // ENCODER_AS5600_H
