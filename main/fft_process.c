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