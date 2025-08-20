#pragma once

void network_manager_init(void);
void network_manager_switch_mode(void);

// Credential management helpers
char *network_manager_get_credentials(void); // returns malloc'd JSON string
bool network_manager_add_credential(const char *ssid, const char *password);
bool network_manager_delete_credential(const char *ssid);
