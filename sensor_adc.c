#include "sensor_adc.h"

#include <math.h>

#include "config.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"

static float g_zero_current_adc = ACS712_ZERO_CURRENT_ADC;

static inline float adc_counts_to_voltage(float counts) {
    return (counts / ADC_MAX_COUNT) * ADC_REF_VOLTAGE;
}

void sensor_adc_init(void) {
    adc_init();
    adc_gpio_init(PIN_ADC_CURRENT);
}

uint16_t sensor_adc_read_current_raw(void) {
    adc_select_input(PIN_ADC_CURRENT - 26u);
    return adc_read();
}

float sensor_adc_calibrate_zero(uint32_t samples, uint32_t sample_delay_us) {
    if (samples == 0u) {
        return g_zero_current_adc;
    }

    uint64_t sum = 0u;
    for (uint32_t i = 0; i < samples; ++i) {
        sum += sensor_adc_read_current_raw();
        if (sample_delay_us > 0u) {
            sleep_us(sample_delay_us);
        }
    }

    g_zero_current_adc = (float)sum / (float)samples;
    return g_zero_current_adc;
}

float sensor_adc_read_current_amps(void) {
    const float raw = (float)sensor_adc_read_current_raw();
    const float zero_v = adc_counts_to_voltage(g_zero_current_adc);
    const float measured_v = adc_counts_to_voltage(raw);

    // Sensorspannung vor der Teiler-Skalierung zurueckrechnen.
    const float zero_sensor_v = zero_v / ACS712_ADC_DIVIDER_RATIO;
    const float measured_sensor_v = measured_v / ACS712_ADC_DIVIDER_RATIO;

    const float delta_v = measured_sensor_v - zero_sensor_v;
    return delta_v / ACS712_SENSITIVITY_V_PER_A;
}
