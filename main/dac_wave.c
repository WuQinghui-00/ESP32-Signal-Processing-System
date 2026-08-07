#include "dac_wave.h"
#include "driver/dac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "DAC_WAVE";

#define TABLE_SIZE 256
static uint8_t wave_table[TABLE_SIZE];
static int current_freq = 1000;
static int running = 0;
static wave_type_t current_type = WAVE_SINE;
static esp_timer_handle_t s_dac_timer = NULL;
static int s_timer_idx = 0;

static void generate_sine_wave(uint8_t *table, int len)
{
    for (int i = 0; i < len; i++) {
        table[i] = 128 + (int)(127 * sin(2 * 3.14159 * i / len));
    }
}

static void generate_square_wave(uint8_t *table, int len)
{
    int half = len / 2;
    for (int i = 0; i < len; i++) {
        table[i] = (i < half) ? 255 : 0;
    }
}

static void generate_triangle_wave(uint8_t *table, int len)
{
    int half = len / 2;
    for (int i = 0; i < len; i++) {
        if (i < half) {
            table[i] = (uint8_t)(255 * i / half);
        } else {
            table[i] = (uint8_t)(255 * (len - i) / half);
        }
    }
}

static void update_wave_table(void)
{
    switch (current_type) {
        case WAVE_SINE:
            generate_sine_wave(wave_table, TABLE_SIZE);
            break;
        case WAVE_SQUARE:
            generate_square_wave(wave_table, TABLE_SIZE);
            break;
        case WAVE_TRIANGLE:
            generate_triangle_wave(wave_table, TABLE_SIZE);
            break;
    }
}

/* Sample period for a full table cycle, clamped to a sane minimum. */
static int dac_period_us(int freq_hz)
{
    int period = 1000000 / freq_hz / TABLE_SIZE;
    if (period < 2) period = 2;
    return period;
}

/* esp_timer callback: write the next DAC sample. Runs in the esp_timer task,
 * so the DAC output no longer occupies a core with a busy loop. */
static void dac_timer_cb(void *arg)
{
    dac_output_voltage(DAC_CHANNEL_1, wave_table[s_timer_idx]);
    s_timer_idx++;
    if (s_timer_idx >= TABLE_SIZE) {
        s_timer_idx = 0;
    }
}

void dac_wave_init(void)
{
    dac_output_enable(DAC_CHANNEL_1);
    dac_output_voltage(DAC_CHANNEL_1, 0);
    generate_sine_wave(wave_table, TABLE_SIZE);
    ESP_LOGI(TAG, "DAC initialized on GPIO25");
}

void dac_wave_start(int freq_hz)
{
    if (running) return;

    current_freq = freq_hz;
    running = 1;
    update_wave_table();

    if (s_dac_timer == NULL) {
        esp_timer_create_args_t timer_args = {
            .callback = dac_timer_cb,
            .arg = NULL,
            .name = "dac_wave",
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_dac_timer));
    }

    int period_us = dac_period_us(current_freq);
    ESP_LOGI(TAG, "DAC started, freq=%d Hz (period %d us)", current_freq, period_us);
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_dac_timer, period_us));
}

void dac_wave_stop(void)
{
    if (!running) return;

    running = 0;
    if (s_dac_timer != NULL) {
        esp_timer_stop(s_dac_timer);
    }
    dac_output_voltage(DAC_CHANNEL_1, 0);
    ESP_LOGI(TAG, "DAC stopped");
}

void dac_wave_set(wave_type_t type, int freq_hz)
{
    /* Stop the timer while table/period changes, then restart. */
    if (running && s_dac_timer != NULL) {
        esp_timer_stop(s_dac_timer);
    }

    if (type != current_type) {
        current_type = type;
        update_wave_table();
    }

    if (freq_hz != current_freq && freq_hz > 0) {
        current_freq = freq_hz;
    }

    if (running && s_dac_timer != NULL) {
        esp_timer_start_periodic(s_dac_timer, dac_period_us(current_freq));
    }

    ESP_LOGI(TAG, "Wave set: type=%d, freq=%d Hz", type, freq_hz);
}