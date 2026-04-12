#ifndef SPINDLE_CTRL_H
#define SPINDLE_CTRL_H

#include <stdint.h>

// Spindel-Steuer-PWM initialisieren (idempotent)
void spindle_ctrl_init(void);

// Normalisierten Sollwert setzen 0.0 .. 1.0
void spindle_ctrl_set_normalized(float n01);

// Steuerspannung im Bereich 0 .. SPINDLE_CTRL_VMAX_V setzen
void spindle_ctrl_set_voltage(float volts);

#endif // SPINDLE_CTRL_H
