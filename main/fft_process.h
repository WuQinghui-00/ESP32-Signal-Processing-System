#ifndef FFT_PROCESS_H
#define FFT_PROCESS_H

#include <stdint.h>

/* Run the full spectrum pipeline: Hann window, 128-point radix-2 FFT
 * (ESP-DSP), magnitude spectrum (bins 0..len/2) and peak detection.
 *
 * DC removal must be done before calling this (see remove_dc_offset).
 *
 * @param data       128 raw ADC samples (after DC removal).
 * @param len        must be 128 (FFT_SIZE).
 * @param sample_rate actual sampling rate in Hz, used to convert bin to Hz.
 * @param magnitude  output array, len/2+1 entries (bins 0..64).
 * @param peak_freq  [out] peak frequency in Hz (optional, may be NULL).
 * @param peak_amp   [out] peak magnitude (optional, may be NULL).
 * @return peak bin (>= 1) on success, or -1 on invalid input/error.
 */
int fft_analyze(int16_t *data, int len, int sample_rate,
                float *magnitude, float *peak_freq, float *peak_amp);

/* Remove the average (DC) level from a sampled block; returns the removed level. */
int remove_dc_offset(int16_t *data, int len);

#endif