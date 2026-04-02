#ifndef DAC_WAVE_H
#define DAC_WAVE_H

#include <stdint.h>

typedef enum {
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_TRIANGLE
} wave_type_t;

void dac_wave_init(void);
void dac_wave_start(int freq_hz);
void dac_wave_stop(void);
void dac_wave_set(wave_type_t type, int freq_hz);

#endif