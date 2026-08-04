#include "fft_process.h"
#include <math.h>

int find_peak_frequency(int16_t *data, int len, int sample_rate)
{
    if (len <= 0 || sample_rate <= 0) return 0;
    
    int max_idx = 0;
    int max_val = data[0];
    for (int i = 1; i < len; i++) {
        if (data[i] > max_val) {
            max_val = data[i];
            max_idx = i;
        }
    }
    return max_idx * sample_rate / len;
}

/**
 * Remove the DC (average) level from a sampled block.
 *
 * Design reason: ADC inputs carry a DC bias (e.g. the DAC sine is centered
 * around ~1.66 V). If that bias is fed into an FFT it appears as a large
 * 0 Hz component that can mask the real peak. Subtracting the block mean in
 * the time domain is the standard, cheap way to remove it.
 *
 * @return the removed DC level in raw ADC counts (0 on invalid input).
 */
int remove_dc_offset(int16_t *data, int len)
{
    if (data == NULL || len <= 0) {
        return 0;
    }

    int32_t sum = 0;
    for (int i = 0; i < len; i++) {
        sum += data[i];
    }
    int mean = (int)((sum + len / 2) / len);

    for (int i = 0; i < len; i++) {
        data[i] = (int16_t)(data[i] - mean);
    }
    return mean;
}