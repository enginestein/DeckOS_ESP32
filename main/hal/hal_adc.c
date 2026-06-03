#include "driver/adc.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hal.h"

static bool adc_calibrated = false;
static adc_cali_handle_t adc_cali_hdl;

void hal_adc_init(void) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_WIDTH_BIT_12,
    };
    adc_calibrated = adc_cali_create_scheme_line_fitting(&cali_cfg, &adc_cali_hdl) == ESP_OK;
}

void hal_adc_select_input(uint channel) {
    if (channel <= 5) {
        adc1_config_channel_atten(channel, ADC_ATTEN_DB_11);
    }
}

uint16_t hal_adc_read(void) {
    return adc1_get_raw(ADC1_CHANNEL_0);
}

uint16_t hal_adc_read_channel(uint channel) {
    adc1_config_channel_atten(channel, ADC_ATTEN_DB_11);
    return adc1_get_raw(channel);
}

float hal_adc_read_voltage(uint channel) {
    uint16_t raw = hal_adc_read_channel(channel);
    uint32_t mv = 0;
    if (adc_calibrated) {
        adc_cali_raw_to_voltage(adc_cali_hdl, raw, &mv);
    } else {
        mv = raw * 3300 / 4095;
    }
    return mv / 1000.0f;
}
