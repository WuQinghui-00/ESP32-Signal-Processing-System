#ifndef FFT_PROCESS_H
#define FFT_PROCESS_H

#include <stdint.h>

int find_peak_frequency(int16_t *data, int len, int sample_rate);

/* Remove the average (DC) level from a sampled block; returns the removed level. */
int remove_dc_offset(int16_t *data, int len);

#endif