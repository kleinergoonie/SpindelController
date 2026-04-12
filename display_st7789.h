#ifndef DISPLAY_ST7789_H
#define DISPLAY_ST7789_H

#include <stdbool.h>
#include <stdint.h>

void display_st7789_init(void);
void display_st7789_set_backlight(bool on);
void display_st7789_update_status(float setpoint_rpm,
                                  float measured_rpm,
                                  float duty,
                                  float torque_nm,
                                  bool enabled,
                                  bool fault_active,
                                  uint32_t status_bits,
                                  uint8_t motor_mode);
void display_st7789_update_network(const uint8_t ip[4],
                                   uint16_t port,
                                   bool edit_mode,
                                   uint8_t edit_octet);

#endif // DISPLAY_ST7789_H
