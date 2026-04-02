#ifndef ADC_SAMPLE_H
#define ADC_SAMPLE_H

#include <stdint.h>

#define ADC_SAMPLES 256

void adc_init(void);
void adc_sample(int16_t *buffer, int len);

#endif