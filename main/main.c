#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "web_server.h"
#include "network_manager.h"
#include "spiffs_handler.h"

#define OUTPUT12 GPIO_NUM_12
#define OUTPUT14 GPIO_NUM_14

static const char *TAG = "APP_MAIN";

// Custom GPIO handler
static esp_err_t gpio_handler(httpd_req_t *req) {
    if (strstr(req->uri, "/12/on")) {
        gpio_set_level(OUTPUT12, 1);
    } else if (strstr(req->uri, "/12/off")) {
        gpio_set_level(OUTPUT12, 0);
    } else if (strstr(req->uri, "/14/on")) {
        gpio_set_level(OUTPUT14, 1);
    } else if (strstr(req->uri, "/14/off")) {
        gpio_set_level(OUTPUT14, 0);
    }
    httpd_resp_send(req, "GPIO updated", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t mode_handler(httpd_req_t *req) {
    network_manager_switch_mode();
    httpd_resp_send(req, "Switching mode", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t creds_get_handler(httpd_req_t *req) {
    char *json = network_manager_get_credentials();
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ESP_OK;
}

static esp_err_t creds_post_handler(httpd_req_t *req) {
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad request");
        return ESP_FAIL;
    }
    buf[len] = '\0';
    char ssid[32] = {0};
    char password[64] = {0};
    sscanf(buf, "ssid=%31[^&]&password=%63s", ssid, password);
    network_manager_add_credential(ssid, password);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t creds_delete_handler(httpd_req_t *req) {
    char ssid[32] = {0};
    size_t qs_len = httpd_req_get_url_query_len(req);
    if (qs_len) {
        char *qs = malloc(qs_len + 1);
        if (qs) {
            httpd_req_get_url_query_str(req, qs, qs_len + 1);
            httpd_query_key_value(qs, "ssid", ssid, sizeof(ssid));
            free(qs);
        }
    }
    network_manager_delete_credential(ssid);
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void app_main(void) {
    ESP_LOGI(TAG, "Initializing...");

    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize SPIFFS
    spiffs_init();

    // Configure GPIOs
    gpio_reset_pin(OUTPUT12);
    gpio_reset_pin(OUTPUT14);
    gpio_set_direction(OUTPUT12, GPIO_MODE_OUTPUT);
    gpio_set_direction(OUTPUT14, GPIO_MODE_OUTPUT);
    gpio_set_level(OUTPUT12, 0);
    gpio_set_level(OUTPUT14, 0);

    // Initialize network (STA or AP depending on flag)
    network_manager_init();

    // Start the web server
    httpd_handle_t server = start_webserver();
    if (server) {
        // Dynamically register a GET route
        register_get_route(server, "/gpio/*", gpio_handler, NULL);

        // Register route to toggle network mode
        // Register route to toggle network mode
        register_get_route(server, "/mode", mode_handler, NULL);

        // Credential management routes
        register_get_route(server, "/credentials", creds_get_handler, NULL);
        register_post_route(server, "/credentials", creds_post_handler, NULL);
        register_get_route(server, "/credentials/delete", creds_delete_handler, NULL);
    }
}
