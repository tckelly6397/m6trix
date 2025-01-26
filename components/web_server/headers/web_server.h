#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <esp_http_server.h>

// Function to initialize and start the web server
httpd_handle_t start_webserver(void);

// Function to stop the web server
void stop_webserver(httpd_handle_t server);

// Function to register additional URI handlers
esp_err_t register_route(httpd_handle_t server, const httpd_uri_t *uri_handler);

#endif

