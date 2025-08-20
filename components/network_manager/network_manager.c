#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_system.h"
#include "ap_manager.h"
#include "sta_manager.h"

static const char *TAG = "NET_MANAGER";
#define MODE_FILE "/public/net_mode"
#define CRED_FILE "/public/credentials.txt"

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

void network_manager_init(void) {
    network_mode_t mode = network_manager_get_mode();
    if (mode == NETWORK_MODE_AP) {
        ESP_LOGI(TAG, "Starting in Access Point mode");
        ap_manager_start();
        return;
    }

    ESP_LOGI(TAG, "Starting in Station mode");

    FILE *f = fopen(CRED_FILE, "r");
    if (!f) {
        ESP_LOGW(TAG, "No credential file found, starting AP");
        ap_manager_start();
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char ssid[32] = {0};
        char password[64] = {0};
        if (sscanf(line, "%31[^,],%63[^\n]", ssid, password) == 2) {
            ESP_LOGI(TAG, "Trying SSID %s", ssid);
            if (sta_manager_start(ssid, password)) {
                fclose(f);
                return; // Connected successfully
            }
        }
    }
    fclose(f);

    ESP_LOGW(TAG, "No known networks connected, starting AP");
    ap_manager_start();
}

void network_manager_switch_mode(void) {
    network_mode_t current = network_manager_get_mode();
    network_mode_t next = current == NETWORK_MODE_AP ? NETWORK_MODE_STA : NETWORK_MODE_AP;
    network_manager_set_mode(next);
    ESP_LOGI(TAG, "Switching to %s mode and rebooting", next == NETWORK_MODE_AP ? "AP" : "STA");
    esp_restart();
}

char *network_manager_get_credentials(void) {
    FILE *f = fopen(CRED_FILE, "r");
    if (!f) {
        return strdup("[]");
    }
    char *buf = malloc(1024);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    buf[0] = '['; buf[1] = '\0';
    char line[128];
    int first = 1;
    while (fgets(line, sizeof(line), f)) {
        char ssid[32] = {0};
        char password[64] = {0};
        if (sscanf(line, "%31[^,],%63[^\n]", ssid, password) == 2) {
            char entry[160];
            snprintf(entry, sizeof(entry), "%s{\"ssid\":\"%s\",\"password\":\"%s\"}",
                     first ? "" : ",", ssid, password);
            strncat(buf, entry, 1024 - strlen(buf) - 1);
            first = 0;
        }
    }
    fclose(f);
    strncat(buf, "]", 1024 - strlen(buf) - 1);
    return buf;
}

bool network_manager_add_credential(const char *ssid, const char *password) {
    FILE *f = fopen(CRED_FILE, "a");
    if (!f) {
        return false;
    }
    fprintf(f, "%s,%s\n", ssid, password);
    fclose(f);
    return true;
}

bool network_manager_delete_credential(const char *ssid) {
    FILE *f = fopen(CRED_FILE, "r");
    if (!f) {
        return false;
    }
    char tmp_path[] = "/public/cred_tmp.txt";
    FILE *tmp = fopen(tmp_path, "w");
    if (!tmp) {
        fclose(f);
        return false;
    }
    char line[128];
    bool removed = false;
    while (fgets(line, sizeof(line), f)) {
        char ssid_read[32] = {0};
        char password[64] = {0};
        if (sscanf(line, "%31[^,],%63[^\n]", ssid_read, password) == 2) {
            if (strcmp(ssid_read, ssid) != 0) {
                fprintf(tmp, "%s,%s\n", ssid_read, password);
            } else {
                removed = true;
            }
        }
    }
    fclose(f);
    fclose(tmp);
    remove(CRED_FILE);
    rename(tmp_path, CRED_FILE);
    return removed;
}
