#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

// Struct to hold Wi-Fi status
typedef struct {
    bool sta_enabled;
    bool sta_connected;
    bool ap_enabled;
} wifi_status_t;

// Function declarations
void wifi_init_sta(const char *ssid, const char *password);
// void wifi_disable_sta(void);
// void wifi_init_ap(const char *ssid, const char *password);
// void wifi_disable_ap(void);
// wifi_status_t wifi_check_status(void);

#endif

