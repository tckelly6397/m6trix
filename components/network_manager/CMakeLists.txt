idf_component_register(SRCS "network_manager.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES spiffs_handler ap_manager sta_manager esp_system)
