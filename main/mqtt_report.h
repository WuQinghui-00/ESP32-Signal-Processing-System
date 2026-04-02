#ifndef MQTT_REPORT_H
#define MQTT_REPORT_H

#include <stdint.h>

void mqtt_init(void);
void mqtt_publish_freq(int freq);
void mqtt_publish_waveform(int16_t *data, int len);

#endif