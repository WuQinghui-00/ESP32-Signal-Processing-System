idf_component_register(SRCS 
    "main.c" 
    "dac_wave.c" 
    "adc_sample.c" 
    "fft_process.c"
    "mqtt_report.c"
    "webserver.c"
    INCLUDE_DIRS "."
    REQUIRES driver nvs_flash freertos mqtt esp_wifi esp_event esp_netif esp_http_server)

target_compile_options(${COMPONENT_TARGET} PRIVATE -Wno-error)