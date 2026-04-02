#include "dac_wave.h"
#include "driver/dac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include <math.h>

static const char *TAG = "DAC_WAVE";

#define TABLE_SIZE 256
static uint8_t wave_table[TABLE_SIZE];
static int current_freq = 1000;
static int running = 0;
static wave_type_t current_type = WAVE_SINE;
static TaskHandle_t dac_task_handle = NULL;

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

static void dac_output_task(void *arg)
{
    dac_output_enable(DAC_CHANNEL_1);
    
    while (running) {
        int delay_us = 1000000 / current_freq / TABLE_SIZE;
        for (int i = 0; i < TABLE_SIZE; i++) {
            dac_output_voltage(DAC_CHANNEL_1, wave_table[i]);
            esp_rom_delay_us(delay_us);
        }
    }
    
    dac_output_voltage(DAC_CHANNEL_1, 0);
    dac_task_handle = NULL;
    vTaskDelete(NULL);
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
    
    xTaskCreate(dac_output_task, "dac_out", 2048, NULL, 5, &dac_task_handle);
    ESP_LOGI(TAG, "DAC started, freq=%d Hz", freq_hz);
}

void dac_wave_stop(void)
{
    if (!running) return;
    
    running = 0;
    if (dac_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100));
        dac_task_handle = NULL;
    }
    dac_output_voltage(DAC_CHANNEL_1, 0);
    ESP_LOGI(TAG, "DAC stopped");
}

void dac_wave_set(wave_type_t type, int freq_hz)
{
    if (type != current_type) {
        current_type = type;
        update_wave_table();
    }
    
    if (freq_hz != current_freq && freq_hz > 0) {
        current_freq = freq_hz;
    }
    
    ESP_LOGI(TAG, "Wave set: type=%d, freq=%d Hz", type, freq_hz);
}