#include "webserver.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "WEB";
static httpd_handle_t server = NULL;
#define SPECTRUM_MAX_LEN 128

static SemaphoreHandle_t s_spectrum_lock = NULL;
static int16_t s_spectrum[SPECTRUM_MAX_LEN];
static int s_spectrum_len = 0;
static int s_peak_freq = 0;

// 只显示数据的 HTML
static const char *index_html = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<title>ESP32 Spectrum</title>"
"<meta charset=\"UTF-8\">"
"<style>"
"body{font-family:Arial;margin:20px;background:#1a1a2e;color:#eee}"
".card{max-width:800px;margin:auto;background:#16213e;padding:20px;border-radius:10px}"
"h1{text-align:center;color:#00ffcc}"
".freq{font-size:48px;text-align:center;color:#00ffcc;margin:20px 0}"
".data{background:#0f3460;padding:15px;border-radius:5px;overflow-x:auto}"
"pre{white-space:pre-wrap;word-wrap:break-word}"
".footer{text-align:center;color:#888;margin-top:20px}"
"</style>"
"</head>"
"<body>"
"<div class=\"card\">"
"<h1>📊 ESP32 Spectrum Monitor</h1>"
"<div class=\"freq\">🎵 <span id=\"freq\">---</span> Hz</div>"
"<div class=\"data\">"
"<pre id=\"data\">loading...</pre>"
"</div>"
"<div class=\"footer\">Auto-refresh every 2 seconds</div>"
"</div>"
"<script>"
"async function loadData(){"
"try{"
"let r=await fetch('/spectrum/data');"
"let d=await r.json();"
"document.getElementById('freq').innerText=d.peak_freq;"
"document.getElementById('data').innerHTML=JSON.stringify(d);"
"}catch(e){"
"document.getElementById('data').innerHTML='Error: '+e;"
"}"
"}"
"setInterval(loadData,2000);"
"loadData();"
"</script>"
"</body>"
"</html>";

// 数据接口
static esp_err_t spectrum_data_handler(httpd_req_t *req)
{
    int16_t snapshot[SPECTRUM_MAX_LEN];
    int len = 0;
    int peak_freq = 0;

    /* Copy the latest spectrum to a local snapshot under the lock, so the
     * response below is never built from a buffer that app_main is still
     * writing (the old code shared a pointer to the caller's stack array). */
    if (s_spectrum_lock != NULL && xSemaphoreTake(s_spectrum_lock, portMAX_DELAY) == pdTRUE) {
        len = s_spectrum_len;
        peak_freq = s_peak_freq;
        if (len > 0) {
            memcpy(snapshot, s_spectrum, len * sizeof(int16_t));
        }
        xSemaphoreGive(s_spectrum_lock);
    }

    if (len <= 0) {
        httpd_resp_send(req, "{\"peak_freq\":0,\"x\":[],\"y\":[]}", 32);
        return ESP_OK;
    }

    char buffer[2048];
    snprintf(buffer, sizeof(buffer), "{\"peak_freq\":%d,\"x\":[0,1,2,3],\"y\":[0,0,0,0]}", peak_freq);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));
    return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, strlen(index_html));
    return ESP_OK;
}

void webserver_start(void)
{
    s_spectrum_lock = xSemaphoreCreateMutex();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_index = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
        httpd_uri_t uri_data = { .uri = "/spectrum/data", .method = HTTP_GET, .handler = spectrum_data_handler };
        httpd_register_uri_handler(server, &uri_index);
        httpd_register_uri_handler(server, &uri_data);
        ESP_LOGI(TAG, "Web server started");
    }
}

void webserver_update_spectrum(int16_t *data, int len, int peak_freq)
{
    if (data == NULL || len <= 0 || s_spectrum_lock == NULL) {
        return;
    }
    if (len > SPECTRUM_MAX_LEN) {
        len = SPECTRUM_MAX_LEN;
    }

    if (xSemaphoreTake(s_spectrum_lock, portMAX_DELAY) == pdTRUE) {
        memcpy(s_spectrum, data, len * sizeof(int16_t));
        s_spectrum_len = len;
        s_peak_freq = peak_freq;
        xSemaphoreGive(s_spectrum_lock);
    }
}