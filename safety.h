#ifndef SAFETY_H
#define SAFETY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float overcurrent_threshold_a;
    float overtemp_threshold_adc;
    uint32_t watchdog_timeout_ms;
} safety_limits_t;

typedef struct {
    bool estop_active;
    bool overcurrent;
    bool overtemp;
    bool watchdog_timeout;
} safety_state_t;

void safety_init(void);
void safety_set_limits(const safety_limits_t* limits);
void safety_kick_watchdog(uint32_t now_ms);
void safety_update(float current_a, float adc_temp, uint32_t now_ms);
const safety_state_t* safety_get_state(void);
const safety_limits_t* safety_get_limits(void);

#endif // SAFETY_H
