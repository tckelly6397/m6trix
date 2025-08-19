#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "ap_manager.h"
#include "sta_manager.h"

static const char *TAG = "NET_MANAGER";
#define MODE_FILE "/public/net_mode"

typedef enum {
    NETWORK_MODE_STA,
    NETWORK_MODE_AP
} network_mode_t;

static network_mode_t network_manager_get_mode(void) {
    FILE *f = fopen(MODE_FILE, "r");
    if (!f) {
        return NETWORK_MODE_STA;
    }
    char buf[4] = {0};
    fgets(buf, sizeof(buf), f);
    fclose(f);
    if (strncmp(buf, "AP", 2) == 0) {
        return NETWORK_MODE_AP;
    }
    return NETWORK_MODE_STA;
}

static void network_manager_set_mode(network_mode_t mode) {
    FILE *f = fopen(MODE_FILE, "w");
    if (f) {
        fputs(mode == NETWORK_MODE_AP ? "AP" : "STA", f);
        fclose(f);
    }
}

void network_manager_init(const char *sta_ssid, const char *sta_password) {
    network_mode_t mode = network_manager_get_mode();
    if (mode == NETWORK_MODE_AP) {
        ESP_LOGI(TAG, "Starting in Access Point mode");
        ap_manager_start();
    } else {
        ESP_LOGI(TAG, "Starting in Station mode");
        sta_manager_start(sta_ssid, sta_password);
    }
}

void network_manager_switch_mode(void) {
    network_mode_t current = network_manager_get_mode();
    network_mode_t next = current == NETWORK_MODE_AP ? NETWORK_MODE_STA : NETWORK_MODE_AP;
    network_manager_set_mode(next);
    ESP_LOGI(TAG, "Switching to %s mode and rebooting", next == NETWORK_MODE_AP ? "AP" : "STA");
    esp_restart();
}
