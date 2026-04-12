#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdbool.h>

typedef enum {
    MOTOR_MODE_COAST = 0,
    MOTOR_MODE_FORWARD,
    MOTOR_MODE_REVERSE,
    MOTOR_MODE_BRAKE
} motor_mode_t;

void motor_control_init(void);
void motor_control_set_mode(motor_mode_t mode);
void motor_control_set_duty(float duty_0_to_1);
void motor_control_set_enable(bool enabled);
bool motor_control_get_enable(void);
float motor_control_get_duty(void);
motor_mode_t motor_control_get_mode(void);

#endif // MOTOR_CONTROL_H
