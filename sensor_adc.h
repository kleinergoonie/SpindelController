#ifndef SENSOR_ADC_H
#define SENSOR_ADC_H

#include <stdint.h>

void sensor_adc_init(void);
uint16_t sensor_adc_read_current_raw(void);
float sensor_adc_calibrate_zero(uint32_t samples, uint32_t sample_delay_us);
float sensor_adc_read_current_amps(void);

#endif // SENSOR_ADC_H
