idf_component_register(SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_netif esp_http_server driver spiffs_handler wifi_manager web_server)
