#include "mqtt_report.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_timer.h"

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client = NULL;

// WiFi 配置（改成你的）
#define WIFI_SSID "Boo"
#define WIFI_PASS "wqh2005828"

#define MQTT_BROKER "mqtt://broker.emqx.io"
#define MQTT_TOPIC_FREQ "/esp32/signal/freq"

// WiFi 事件处理
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// WiFi 初始化
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi initialized");
}

// MQTT 事件处理
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected to broker");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            break;
        default:
            break;
    }
}

// MQTT 初始化
void mqtt_init(void)
{
    wifi_init();
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER,
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    
    ESP_LOGI(TAG, "MQTT initialized, broker: %s", MQTT_BROKER);
}

// 上报峰值频率
void mqtt_publish_freq(int freq)
{
    if (client == NULL) return;
    
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"peak_frequency\":%d}", freq);
    
    int msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC_FREQ, payload, 0, 1, 0);
    ESP_LOGI(TAG, "Published freq: %s, msg_id=%d", payload, msg_id);
}

// 上报波形数据（可选）
void mqtt_publish_waveform(int16_t *data, int len)
{
    if (client == NULL) return;
    
    int send_len = len > 64 ? 64 : len;
    char payload[512];
    int pos = snprintf(payload, sizeof(payload), "[");
    for (int i = 0; i < send_len && pos < sizeof(payload) - 10; i++) {
        pos += snprintf(payload + pos, sizeof(payload) - pos, "%d,", data[i]);
    }
    if (pos > 1) payload[pos-1] = ']';
    
    esp_mqtt_client_publish(client, "/esp32/signal/waveform", payload, 0, 0, 0);
}