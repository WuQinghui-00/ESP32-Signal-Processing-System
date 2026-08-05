#ifndef MQTT_REPORT_H
#define MQTT_REPORT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool connected;
    uint32_t publish_total;
    uint32_t ack_total;
    uint32_t disconnect_total;
    uint32_t error_total;
} mqtt_stats_t;

void mqtt_init(void);
void mqtt_get_stats(mqtt_stats_t *stats);
void mqtt_publish_spectrum(int peak_freq, float peak_amp, int sample_rate);
void mqtt_publish_waveform(int16_t *data, int len);

#endif