idf_component_register(SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_netif esp_http_server driver
                  spiffs_handler web_server network_manager sta_manager ap_manager
                  led_strip)

