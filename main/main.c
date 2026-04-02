#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "dac_wave.h"
#include "adc_sample.h"
#include "fft_process.h"
#include "mqtt_report.h"
#include "webserver.h"

static const char *TAG = "MAIN";

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
    xTaskCreate(uart_task, "uart_cmd", 4096, NULL, 3, NULL);
    
    int16_t buffer[SAMPLE_LEN];
    int16_t magnitude[SAMPLE_LEN / 2];
    
    while (1) {
        // ADC 采集
        adc_sample(buffer, SAMPLE_LEN);
        
        // 计算幅值（简化版）
        for (int i = 0; i < SAMPLE_LEN / 2; i++) {
            magnitude[i] = buffer[i] > 0 ? buffer[i] : -buffer[i];
        }
        
        // 峰值频率
        int peak_freq = find_peak_frequency(buffer, SAMPLE_LEN, SAMPLE_RATE);
        ESP_LOGI(TAG, "Peak frequency: %d Hz", peak_freq);
        
        // MQTT 上报
        mqtt_publish_freq(peak_freq);
        
        // Web 更新频谱
       webserver_update_spectrum((int16_t*)magnitude, SAMPLE_LEN / 2, peak_freq);
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}