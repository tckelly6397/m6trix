#ifndef WIFI_H
#define WIFI_H

// Wi-Fi credentials
#define WIFI_SSID "The Station"
#define WIFI_PASS "RZMJ964QB5HE7CAV"

// Buffer to hold the IP address
extern char ip_address[16];

// Function to initialize Wi-Fi
void wifi_init(void);

#endif
