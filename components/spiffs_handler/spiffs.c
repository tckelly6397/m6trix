#include <esp_spiffs.h>
#include <esp_log.h>
#include <stdbool.h>
#include <stdio.h>

#include "spiffs_handler.h"

static const char *TAG = "SPIFFS";

void spiffs_init(void) {
    // SPIFFS configuration
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/public",     // Mount point for SPIFFS
        .partition_label = NULL,    // NULL means default "spiffs" partition
        .max_files = 5,             // Maximum number of open files
        .format_if_mount_failed = true // Format if mount fails
    };

    // Mount the SPIFFS filesystem
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount SPIFFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    // Get SPIFFS info
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS info (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS Info: Total: %d, Used: %d", total, used);
    }

    // Test SPIFFS by opening a file
    FILE *f = fopen("/public/index.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file");
    } else {
        ESP_LOGI(TAG, "File opened successfully");
        fclose(f);
    }
}
