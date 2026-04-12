#ifndef TOUCH_INPUT_H
#define TOUCH_INPUT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool pressed;
    bool just_pressed;
    uint16_t x;
    uint16_t y;
} touch_event_t;

void touch_input_init(void);
bool touch_input_poll(touch_event_t* out_event);

#endif // TOUCH_INPUT_H
