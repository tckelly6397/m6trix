#pragma once

#include <stdbool.h>

// Attempt to connect to the provided Wi-Fi credentials.
// Returns true on successful connection, false otherwise.
bool sta_manager_start(const char *ssid, const char *password);
