#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "dac_wave.h"
#include "adc_sample.h"
#include "fft_process.h"
#include "mqtt_report.h"
#include "webserver.h"

static const char *TAG = "MAIN";

static TaskHandle_t s_uart_task = NULL;

#define SAMPLE_RATE 1000     // 1000 Hz 采样率
#define SAMPLE_LEN 128        // 128个点

// 串口命令任务
static void uart_task(void *arg)
{
    char cmd[32];
    int idx = 0;
    int c;
    
    while (1) {
        c = getchar();
        if (c == '\n' || c == '\r') {
            if (idx > 0) {
                cmd[idx] = '\0';
                
                if (strncmp(cmd, "SINE", 4) == 0) {
                    int freq = 1000;
                    if (idx > 5) freq = atoi(cmd + 5);
                    if (freq < 1) freq = 1;
                    if (freq > 5000) freq = 5000;
                    dac_wave_set(WAVE_SINE, freq);
                    ESP_LOGI(TAG, "Switch to SINE wave, freq=%d Hz", freq);
                }
                else if (strncmp(cmd, "SQUARE", 6) == 0) {
                    int freq = 1000;
                    if (idx > 7) freq = atoi(cmd + 7);
                    if (freq < 1) freq = 1;
                    if (freq > 5000) freq = 5000;
                    dac_wave_set(WAVE_SQUARE, freq);
                    ESP_LOGI(TAG, "Switch to SQUARE wave, freq=%d Hz", freq);
                }
                else if (strncmp(cmd, "TRIANGLE", 8) == 0) {
                    int freq = 1000;
                    if (idx > 9) freq = atoi(cmd + 9);
                    if (freq < 1) freq = 1;
                    if (freq > 5000) freq = 5000;
                    dac_wave_set(WAVE_TRIANGLE, freq);
                    ESP_LOGI(TAG, "Switch to TRIANGLE wave, freq=%d Hz", freq);
                }
                else if (strncmp(cmd, "STOP", 4) == 0) {
                    dac_wave_stop();
                    ESP_LOGI(TAG, "DAC stopped");
                }
                else if (strncmp(cmd, "START", 5) == 0) {
                    dac_wave_start(1000);
                    ESP_LOGI(TAG, "DAC started");
                }
                idx = 0;
            }
        } else if (idx < 31) {
            cmd[idx++] = c;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "Signal Processing System");
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, "Commands: SINE [freq], SQUARE [freq], TRIANGLE [freq], STOP, START");
    
    // 初始化模块
    dac_wave_init();
    adc_init();
    mqtt_init();
    webserver_start();
    
    // 启动 DAC 输出 1kHz 正弦波
    dac_wave_start(1000);
    ESP_LOGI(TAG, "DAC: 1kHz sine wave on GPIO25");
    ESP_LOGI(TAG, "ADC: GPIO35 (connect GPIO25 to GPIO35)");
    
    // 创建串口命令任务
    xTaskCreate(uart_task, "uart_cmd", 4096, NULL, 3, &s_uart_task);
    TaskHandle_t main_task = xTaskGetCurrentTaskHandle();
    
    int16_t buffer[SAMPLE_LEN];
    int16_t magnitude[SAMPLE_LEN / 2];
    float spectrum[SAMPLE_LEN / 2 + 1];
    
    while (1) {
        // ADC 采集
        int actual_rate = adc_sample(buffer, SAMPLE_LEN);
        ESP_LOGI(TAG, "ADC: sampled %d points, actual rate %d S/s", SAMPLE_LEN, actual_rate);

        int dc_offset = remove_dc_offset(buffer, SAMPLE_LEN);
        ESP_LOGI(TAG, "ADC: removed DC offset, mean=%d counts", dc_offset);

        // FFT: Hann window -> 128-point radix-2 FFT -> magnitude spectrum
        float peak_freq = 0.0f;
        float peak_amp = 0.0f;
        int peak_bin = fft_analyze(buffer, SAMPLE_LEN, SAMPLE_RATE, spectrum, &peak_freq, &peak_amp);
        int peak_freq_int = (int)(peak_freq + 0.5f);
        ESP_LOGI(TAG, "FFT: peak bin=%d, freq=%d Hz, amp=%.0f", peak_bin, peak_freq_int, peak_amp);

        // Magnitude preview (bins 1..64) for the existing web interface
        for (int i = 0; i < SAMPLE_LEN / 2; i++) {
            float v = spectrum[i + 1] / 64.0f;
            magnitude[i] = (v > 32767.0f) ? (int16_t)32767 : (int16_t)v;
        }

        // MQTT report
        mqtt_publish_spectrum(peak_freq_int, peak_amp, actual_rate);

        // Web spectrum update
        webserver_update_spectrum(magnitude, SAMPLE_LEN / 2, peak_freq_int);

        // Stack high-water mark monitor (every 30 loops = 60 s)
        static int s_monitor_count = 0;
        if (++s_monitor_count % 30 == 0) {
            ESP_LOGI(TAG, "Stack high-water: main=%" PRIu32 "B uart_cmd=%" PRIu32 "B",
                     (uint32_t)uxTaskGetStackHighWaterMark(main_task),
                     (uint32_t)uxTaskGetStackHighWaterMark(s_uart_task));
            ESP_LOGI(TAG, "Heap: free=%" PRIu32 "B min_free=%" PRIu32 "B",
                     (uint32_t)esp_get_free_heap_size(),
                     (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
            wifi_stats_t wifi_stats;
            wifi_get_stats(&wifi_stats);
            ESP_LOGI(TAG, "WiFi: rssi=%d dBm disconnects=%" PRIu32 " attempts=%" PRIu32, wifi_stats.rssi, wifi_stats.disconnect_total, wifi_stats.reconnect_attempt);
            webserver_update_stack((int)uxTaskGetStackHighWaterMark(main_task),
                                   (int)uxTaskGetStackHighWaterMark(s_uart_task));
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}