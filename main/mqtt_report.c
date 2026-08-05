#include "mqtt_report.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <inttypes.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_random.h"

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client = NULL;
// MQTT statistics (connection state + QoS 1 ACK counters)
static bool s_mqtt_connected = false;
static uint32_t s_mqtt_publish_total = 0;
static uint32_t s_mqtt_ack_total = 0;
static uint32_t s_mqtt_disconnect_total = 0;
static uint32_t s_mqtt_error_total = 0;
// Latest state cache: kept during disconnects, resent on reconnect
static char s_last_payload[160];
static bool s_has_last_payload = false;

// WiFi 配置（改成你的）
#define WIFI_SSID "Boo"
#define WIFI_PASS "wqh2005828"

#define MQTT_BROKER "mqtt://broker.emqx.io"
#define MQTT_TOPIC_FREQ "/esp32/signal/freq"

/* Resend the latest cached state once after (re)connecting. */
static void mqtt_resend_cached(void)
{
    if (client == NULL || !s_has_last_payload) {
        return;
    }

    int msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC_FREQ, s_last_payload, 0, 1, 0);
    if (msg_id >= 0) {
        s_mqtt_publish_total++;
        ESP_LOGI(TAG, "Resent cached state: %s, msg_id=%d", s_last_payload, msg_id);
    } else {
        s_mqtt_error_total++;
        ESP_LOGE(TAG, "Resend cached state failed, msg_id=%d", msg_id);
    }
}

// WiFi reconnect: exponential backoff with random jitter
#define WIFI_RECONNECT_BASE_MS 1000
#define WIFI_RECONNECT_MAX_MS  30000

static esp_timer_handle_t s_reconnect_timer = NULL;
static int s_reconnect_attempt = 0;
static uint32_t s_wifi_disconnect_total = 0;
static bool s_wifi_connected = false;

/* Delay for the next reconnect: base x 2^attempt, capped, plus/minus 20%
 * random jitter so devices do not reconnect in lockstep. */
static int wifi_backoff_delay_ms(void)
{
    int attempt = s_reconnect_attempt;
    if (attempt > 5) attempt = 5;
    int delay = WIFI_RECONNECT_BASE_MS * (1 << attempt);
    if (delay > WIFI_RECONNECT_MAX_MS) delay = WIFI_RECONNECT_MAX_MS;
    int jitter = delay / 5;
    int offset = (int)(esp_random() % (2u * (unsigned)jitter + 1u)) - jitter;
    delay += offset;
    if (delay < 100) delay = 100;
    return delay;
}

static void wifi_reconnect_timer_cb(void *arg)
{
    s_reconnect_attempt++;
    ESP_LOGI(TAG, "WiFi reconnect attempt %d", s_reconnect_attempt);
    esp_wifi_connect();
}

// WiFi 事件处理
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        s_wifi_connected = false;
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)event_data;
        int delay_ms = wifi_backoff_delay_ms();
        s_wifi_disconnect_total++;
        ESP_LOGW(TAG, "WiFi disconnected (total=%" PRIu32 ", reason=%d), reconnect #%d in %d ms",
                 s_wifi_disconnect_total, disc != NULL ? disc->reason : -1, s_reconnect_attempt + 1, delay_ms);
        esp_timer_stop(s_reconnect_timer);
        if (esp_timer_start_once(s_reconnect_timer, (uint64_t)delay_ms * 1000u) != ESP_OK) {
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_wifi_connected = true;
        s_reconnect_attempt = 0;
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
    esp_timer_create_args_t timer_args = {
        .callback = wifi_reconnect_timer_cb,
        .arg = NULL,
        .name = "wifi_reconnect",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_reconnect_timer));
    
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
            s_mqtt_connected = true;
            ESP_LOGI(TAG, "MQTT connected (published=%" PRIu32 " acked=%" PRIu32 " disconnects=%" PRIu32 ")",
                     s_mqtt_publish_total, s_mqtt_ack_total, s_mqtt_disconnect_total);
            mqtt_resend_cached();
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_mqtt_connected = false;
            s_mqtt_disconnect_total++;
            ESP_LOGW(TAG, "MQTT disconnected (total=%" PRIu32 ")", s_mqtt_disconnect_total);
            break;
        case MQTT_EVENT_PUBLISHED:
            s_mqtt_ack_total++;
            if (s_mqtt_ack_total % 5 == 0) {
                ESP_LOGI(TAG, "MQTT QoS1 acked=%" PRIu32 " published=%" PRIu32, s_mqtt_ack_total, s_mqtt_publish_total);
            }
            break;
        case MQTT_EVENT_ERROR:
            s_mqtt_error_total++;
            ESP_LOGE(TAG, "MQTT error (total=%" PRIu32 ")", s_mqtt_error_total);
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
void mqtt_publish_spectrum(int peak_freq, float peak_amp, int sample_rate)
{
    if (client == NULL) return;

    char payload[160];
    snprintf(payload, sizeof(payload), "{\"peak_frequency\":%d,\"peak_amplitude\":%.0f,\"sample_rate\":%d}", peak_freq, peak_amp, sample_rate);
    /* Always keep the latest state so it can be resent after a reconnect. */
    memcpy(s_last_payload, payload, sizeof(payload));
    s_has_last_payload = true;


    if (!s_mqtt_connected) {
        ESP_LOGW(TAG, "MQTT offline, cached latest state");
        return;
    }
    int msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC_FREQ, payload, 0, 1, 0);
    if (msg_id >= 0) {
        s_mqtt_publish_total++;
    } else {
        s_mqtt_error_total++;
        ESP_LOGE(TAG, "MQTT publish failed, msg_id=%d", msg_id);
    }
    ESP_LOGI(TAG, "Published: %s, msg_id=%d", payload, msg_id);
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

void mqtt_get_stats(mqtt_stats_t *stats)
{
    if (stats == NULL) return;

    stats->connected = s_mqtt_connected;
    stats->publish_total = s_mqtt_publish_total;
    stats->ack_total = s_mqtt_ack_total;
    stats->disconnect_total = s_mqtt_disconnect_total;
    stats->error_total = s_mqtt_error_total;
}

void wifi_get_stats(wifi_stats_t *stats)
{
    if (stats == NULL) return;

    wifi_ap_record_t ap;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap);
    stats->connected = s_wifi_connected;
    stats->rssi = (ret == ESP_OK) ? ap.rssi : 0;
    stats->disconnect_total = s_wifi_disconnect_total;
    stats->reconnect_attempt = (uint32_t)s_reconnect_attempt;
}