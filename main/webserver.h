#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <stdint.h>

void webserver_start(void);
void webserver_update_spectrum(int16_t *data, int len, int peak_freq);

#endif