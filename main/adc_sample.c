#include "adc_sample.h"
#include "driver/adc.h"
#include "esp_rom_sys.h"

void adc_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_12);  // GPIO35
}

void adc_sample(int16_t *buffer, int len)
{
    for (int i = 0; i < len; i++) {
        buffer[i] = adc1_get_raw(ADC1_CHANNEL_7);
        esp_rom_delay_us(1000);  // 1000Hz 采样率 = 1ms
    }
}