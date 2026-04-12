#ifndef ENCODER_MT6835_H
#define ENCODER_MT6835_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t angle_raw;  // 21-bit value
    uint8_t status;      // STATUS[2:0]
    uint8_t crc;
    bool crc_valid;
} mt6835_sample_t;

void mt6835_init(void);
bool mt6835_read_sample(mt6835_sample_t* out);
bool mt6835_read_register(uint16_t address, uint8_t* out_value);
void mt6835_report_registers(void);
float mt6835_raw_to_degrees(uint32_t raw);
float mt6835_compute_rpm(uint32_t prev_raw, uint32_t curr_raw, float dt_seconds);

#endif // ENCODER_MT6835_H
