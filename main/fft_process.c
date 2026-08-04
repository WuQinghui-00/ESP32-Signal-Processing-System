#include "fft_process.h"

#include <math.h>

#include "esp_dsp.h"

#define FFT_SIZE 128

/* Workspace for the ESP-DSP FFT. Kept static (not on the caller's stack) so
 * the buffer keeps the 16-byte alignment the optimized ESP32 FFT expects. */
static float s_fft_buf[2 * FFT_SIZE] __attribute__((aligned(16)));
static float s_hann[FFT_SIZE];
static int s_dsp_ready = 0;

static int fft_ensure_init(void)
{
    if (s_dsp_ready) {
        return ESP_OK;
    }
    if (dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE) != ESP_OK) {
        return -1;
    }
    dsps_wind_hann_f32(s_hann, FFT_SIZE);
    s_dsp_ready = 1;
    return ESP_OK;
}

int fft_analyze(int16_t *data, int len, int sample_rate,
                float *magnitude, float *peak_freq, float *peak_amp)
{
    if (data == NULL || magnitude == NULL || len != FFT_SIZE || sample_rate <= 0) {
        return -1;
    }
    if (fft_ensure_init() != ESP_OK) {
        return -1;
    }

    /* Window the real samples, then pack them as complex with a zero
     * imaginary part. dsps_fft2r_fc32 is a plain radix-2 complex FFT of
     * FFT_SIZE complex points, so this yields the correct FFT of the real
     * signal without any extra repacking step. */
    for (int i = 0; i < FFT_SIZE; i++) {
        s_fft_buf[2 * i] = (float)data[i] * s_hann[i];
        s_fft_buf[2 * i + 1] = 0.0f;
    }

    if (dsps_fft2r_fc32(s_fft_buf, FFT_SIZE) != ESP_OK) {
        return -1;
    }
    if (dsps_bit_rev_fc32(s_fft_buf, FFT_SIZE) != ESP_OK) {
        return -1;
    }

    /* Magnitude spectrum for bins 0..N/2 (bin N/2 is Nyquist). The DC bin
     * (k == 0) is excluded from the peak search on purpose. */
    int half = FFT_SIZE / 2;
    float best = -1.0f;
    int best_bin = 0;
    for (int k = 0; k <= half; k++) {
        float re = s_fft_buf[2 * k];
        float im = s_fft_buf[2 * k + 1];
        float mag = sqrtf(re * re + im * im);
        magnitude[k] = mag;
        if (k > 0 && mag > best) {
            best = mag;
            best_bin = k;
        }
    }

    if (peak_freq != NULL) {
        *peak_freq = (float)best_bin * (float)sample_rate / (float)FFT_SIZE;
    }
    if (peak_amp != NULL) {
        *peak_amp = best;
    }
    return best_bin;
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