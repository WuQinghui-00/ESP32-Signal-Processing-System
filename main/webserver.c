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
#define SPECTRUM_BIN_HZ 7.8125f   /* sample_rate / FFT_SIZE = 1000 / 128 */

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
"canvas{width:100%;height:240px;background:#0f3460;border-radius:5px}"
".info{text-align:center;color:#888;margin-top:10px}"
"</style>"
"</head>"
"<body>"
"<div class=\"card\">"
"<h1>ESP32 Spectrum Monitor</h1>"
"<div class=\"freq\"><span id=\"freq\">---</span> Hz</div>"
"<canvas id=\"spectrum\" width=\"760\" height=\"240\"></canvas>"
"<div class=\"info\" id=\"info\">loading...</div>"
"</div>"
"<script>"
"const canvas=document.getElementById('spectrum');"
"const ctx=canvas.getContext('2d');"
"async function loadData(){"
"try{"
"let r=await fetch('/spectrum/data');"
"let d=await r.json();"
"document.getElementById('freq').innerText=d.peak_freq;"
"document.getElementById('info').innerText='peak '+d.peak_freq+' Hz, bins '+d.y.length;"
"draw(d);"
"}catch(e){"
"document.getElementById('info').innerText='Error: '+e;"
"}"
"}"
"function draw(d){"
"const w=canvas.width,h=canvas.height;"
"ctx.clearRect(0,0,w,h);"
"const n=d.y.length;"
"if(n===0)return;"
"let max=1;"
"for(let i=0;i<n;i++){if(d.y[i]>max)max=d.y[i];}"
"// horizontal grid"
"ctx.strokeStyle='#1f4d7a';"
"ctx.lineWidth=1;"
"for(let g=0;g<5;g++){"
"const gy=h-(g/4)*(h-20)-10;"
"ctx.beginPath();ctx.moveTo(0,gy);ctx.lineTo(w,gy);ctx.stroke();"
"}"
"// spectrum line"
"ctx.strokeStyle='#00ffcc';"
"ctx.lineWidth=2;"
"ctx.beginPath();"
"for(let i=0;i<n;i++){"
"const x=(n>1)?i/(n-1)*w:0;"
"const y=h-(d.y[i]/max)*(h-20)-10;"
"if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);"
"}"
"ctx.stroke();"
"// peak marker"
"let pi=0;"
"for(let i=1;i<n;i++){if(d.y[i]>d.y[pi])pi=i;}"
"const px=(n>1)?pi/(n-1)*w:0;"
"ctx.strokeStyle='#ff6b6b';"
"ctx.lineWidth=1;"
"ctx.beginPath();ctx.moveTo(px,0);ctx.lineTo(px,h);ctx.stroke();"
"ctx.fillStyle='#ff6b6b';"
"ctx.font='13px Arial';"
"ctx.fillText(d.peak_freq+' Hz',px+6,18);"
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
    int off = 0;
    off += snprintf(buffer + off, sizeof(buffer) - off, "{\"peak_freq\":%d,\"x\":[", peak_freq);
    for (int i = 0; i < len; i++) {
        off += snprintf(buffer + off, sizeof(buffer) - off, "%s%.1f", i ? "," : "", (i + 1) * SPECTRUM_BIN_HZ);
    }
    off += snprintf(buffer + off, sizeof(buffer) - off, "],\"y\":[");
    for (int i = 0; i < len; i++) {
        off += snprintf(buffer + off, sizeof(buffer) - off, "%s%d", i ? "," : "", snapshot[i]);
    }
    off += snprintf(buffer + off, sizeof(buffer) - off, "]}");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, off);
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