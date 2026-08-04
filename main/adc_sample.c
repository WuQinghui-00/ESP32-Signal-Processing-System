#include "adc_sample.h"
#include "driver/adc.h"
#include "esp_timer.h"

#define ADC_SAMPLE_PERIOD_US 1000   /* target sample period: 1 kHz => 1000 us */

void adc_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_12);  // GPIO35
}

/**
 * Sample `len` points on a fixed 1 kS/s schedule.
 *
 * Design reason: the old implementation used esp_rom_delay_us(1000) after each
 * read, which did not account for the adc1_get_raw() conversion time, so the
 * effective rate was lower than 1000 S/s. Here every sample is scheduled on an
 * absolute microsecond boundary (t0 + i * 1000us), so the conversion overhead
 * is absorbed and the period stays close to 1 ms. The total elapsed time is
 * measured and converted into the actual sample rate.
 *
 * @return measured sample rate in S/s (rounded), or 0 on invalid input.
 */
int adc_sample(int16_t *buffer, int len)
{
    if (buffer == NULL || len <= 0) {
        return 0;
    }

    int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < len; i++) {
        buffer[i] = adc1_get_raw(ADC1_CHANNEL_7);

        if (i < len - 1) {
            /* busy-wait until the next 1 ms sample boundary (absolute timestamp) */
            int64_t target = t0 + (int64_t)(i + 1) * ADC_SAMPLE_PERIOD_US;
            while (esp_timer_get_time() < target) {
            }
        }
    }

    int64_t elapsed_us = esp_timer_get_time() - t0;
    if (elapsed_us <= 0) {
        return 0;
    }
    return (int)(((int64_t)len * 1000000LL + elapsed_us / 2) / elapsed_us);
}