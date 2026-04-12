#ifndef BOARD_LED_H
#define BOARD_LED_H

#include <stdint.h>

// Board-LED-PWM initialisieren (idempotent).
void board_led_init(void);

// LED-Helligkeit setzen 0.0 .. 1.0
void board_led_set_brightness(float duty01);

#endif // BOARD_LED_H
