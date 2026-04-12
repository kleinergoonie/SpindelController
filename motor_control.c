#include "motor_control.h"

#include <math.h>

#include "config.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

static uint slice_a;
static uint slice_b;
static uint16_t pwm_wrap;
static float motor_duty;
static motor_mode_t motor_mode;
static bool motor_enabled;

static uint16_t duty_to_level(float duty) {
    if (duty < 0.0f) {
        duty = 0.0f;
    }
    if (duty > 1.0f) {
        duty = 1.0f;
    }
    return (uint16_t)lrintf(duty * (float)pwm_wrap);
}

void motor_control_init(void) {
    gpio_set_function(PIN_MOTOR_PWM_A, GPIO_FUNC_PWM);
    gpio_set_function(PIN_MOTOR_PWM_B, GPIO_FUNC_PWM);

    gpio_init(PIN_MOTOR_BRAKE);
    gpio_set_dir(PIN_MOTOR_BRAKE, GPIO_OUT);
    gpio_put(PIN_MOTOR_BRAKE, 0);

    gpio_init(PIN_MOTOR_ENABLE);
    gpio_set_dir(PIN_MOTOR_ENABLE, GPIO_OUT);
    gpio_put(PIN_MOTOR_ENABLE, 0);

    slice_a = pwm_gpio_to_slice_num(PIN_MOTOR_PWM_A);
    slice_b = pwm_gpio_to_slice_num(PIN_MOTOR_PWM_B);

    const uint32_t sys_hz = clock_get_hz(clk_sys);
    pwm_wrap = (uint16_t)(sys_hz / MOTOR_PWM_FREQ_HZ - 1u);

    pwm_set_wrap(slice_a, pwm_wrap);
    pwm_set_wrap(slice_b, pwm_wrap);
    pwm_set_enabled(slice_a, true);
    pwm_set_enabled(slice_b, true);

    motor_duty = 0.0f;
    motor_mode = MOTOR_MODE_COAST;
    motor_enabled = false;
    motor_control_set_duty(0.0f);
}

void motor_control_set_mode(motor_mode_t mode) {
    motor_mode = mode;

    switch (mode) {
        case MOTOR_MODE_FORWARD:
            gpio_put(PIN_MOTOR_BRAKE, 0);
            pwm_set_gpio_level(PIN_MOTOR_PWM_A, duty_to_level(motor_duty));
            pwm_set_gpio_level(PIN_MOTOR_PWM_B, 0);
            break;
        case MOTOR_MODE_REVERSE:
            gpio_put(PIN_MOTOR_BRAKE, 0);
            pwm_set_gpio_level(PIN_MOTOR_PWM_A, 0);
            pwm_set_gpio_level(PIN_MOTOR_PWM_B, duty_to_level(motor_duty));
            break;
        case MOTOR_MODE_BRAKE:
            pwm_set_gpio_level(PIN_MOTOR_PWM_A, 0);
            pwm_set_gpio_level(PIN_MOTOR_PWM_B, 0);
            gpio_put(PIN_MOTOR_BRAKE, 1);
            break;
        case MOTOR_MODE_COAST:
        default:
            pwm_set_gpio_level(PIN_MOTOR_PWM_A, 0);
            pwm_set_gpio_level(PIN_MOTOR_PWM_B, 0);
            gpio_put(PIN_MOTOR_BRAKE, 0);
            break;
    }
}

void motor_control_set_duty(float duty_0_to_1) {
    // Signierten Duty im Bereich -1.0 .. 1.0 akzeptieren, um Rueckwaertsbetrieb zu erlauben
    if (duty_0_to_1 > 1.0f) {
        duty_0_to_1 = 1.0f;
    }
    if (duty_0_to_1 < -1.0f) {
        duty_0_to_1 = -1.0f;
    }

    // Absoluten Duty (0..1) speichern und Modus anhand des Vorzeichens waehlen
    float abs_duty = fabsf(duty_0_to_1);
    motor_duty = abs_duty;

    if (abs_duty <= 0.0f) {
        // Null -> Coast (kein aktives Treiben)
        motor_control_set_mode(MOTOR_MODE_COAST);
    } else if (duty_0_to_1 > 0.0f) {
        motor_control_set_mode(MOTOR_MODE_FORWARD);
    } else {
        motor_control_set_mode(MOTOR_MODE_REVERSE);
    }
}

void motor_control_set_enable(bool enabled) {
    motor_enabled = enabled;
    gpio_put(PIN_MOTOR_ENABLE, enabled ? 1 : 0);
}

bool motor_control_get_enable(void) {
    return motor_enabled;
}

float motor_control_get_duty(void) {
    return motor_duty;
}

motor_mode_t motor_control_get_mode(void) {
    return motor_mode;
}
