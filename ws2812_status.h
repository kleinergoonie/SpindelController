#ifndef WS2812_STATUS_H
#define WS2812_STATUS_H

#include <stdbool.h>
#include <stdint.h>

void ws2812_status_init(void);
void ws2812_status_update(uint32_t status_bits,
						  bool fault_active,
						  bool motor_enabled,
						  uint32_t now_ms,
						  uint32_t remora_rx_age_ms);

#endif // WS2812_STATUS_H
