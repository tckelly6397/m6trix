#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include "spiffs_handler.h"
#include "wifi_manager.h"

#define OUTPUT12 GPIO_NUM_12
#define OUTPUT14 GPIO_NUM_14

static const char *TAG = "ESP32_Web_Server";
static char output12State[4] = "off";
static char output14State[4] = "off";

// HTTP GET handler
esp_err_t http_get_handler(httpd_req_t *req) {
    char *response = NULL;

    esp_log_level_set(TAG, ESP_LOG_INFO); // Enable info-level logging if not already set
    ESP_LOGI(TAG, "Received request for URI: %s", req->uri);


    // Handle GPIO control
    if (strstr(req->uri, "/12/on")) {
        gpio_set_level(OUTPUT12, 1);
        snprintf(output12State, sizeof(output12State), "on");
    } else if (strstr(req->uri, "/12/off")) {
        gpio_set_level(OUTPUT12, 0);
        snprintf(output12State, sizeof(output12State), "off");
    } else if (strstr(req->uri, "/14/on")) {
        gpio_set_level(OUTPUT14, 1);
        snprintf(output14State, sizeof(output14State), "on");
    } else if (strstr(req->uri, "/14/off")) {
        gpio_set_level(OUTPUT14, 0);
        snprintf(output14State, sizeof(output14State), "off");
    }

    // Generate the HTML response
    asprintf(&response,
        "<!DOCTYPE html><html>"
        "<head><title>ESP32 Web Server</title></head>"
        "<body><h1>ESP32 Web Server</h1>"
        "<style>body { background-color: #c1c1c1; }</style>"
        "<p>GPIO 12 - State: %s</p>"
        "<p><a href=\"/12/on\">ON</a> | <a href=\"/12/off\">OFF</a></p>"
        "<p>GPIO 14 - State: %s</p>"
        "<p><a href=\"/14/on\">ON</a> | <a href=\"/14/off\">OFF</a></p>"
        "%s" 
        "</body></html>",
        output12State, output14State, ip_address);

    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    free(response);

    return ESP_OK;
}

esp_err_t public_handler(httpd_req_t *req) {
    char filepath[128]; // Use a smaller buffer
    struct stat file_stat;

    // Construct file path, skipping "/public"
    snprintf(filepath, sizeof(filepath), "/public/%s", req->uri + 1);

    // Check if file exists
    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE("PUBLIC_HANDLER", "File not found: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Open the file
    FILE *file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGE("PUBLIC_HANDLER", "Failed to open file: %s", filepath);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Send the file contents
    char *buffer = malloc(1024); // Dynamically allocate memory
    if (!buffer) {
        fclose(file);
        ESP_LOGE("PUBLIC_HANDLER", "Memory allocation failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, 1024, file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
            fclose(file);
            free(buffer);
            ESP_LOGE("PUBLIC_HANDLER", "File sending failed");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    }

    free(buffer);
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0); // End response

    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Enable wildcard URI matching
    config.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {


        // Register handler for GPIOS ("/gpio/*")
        httpd_uri_t uri_gpio = {
            .uri = "/gpio/*",
            .method = HTTP_GET,
            .handler = http_get_handler,
            .user_ctx = NULL,
        };
        httpd_register_uri_handler(server, &uri_gpio);

        // Register handler for everything else ("/*")
        httpd_uri_t uri_root = {
            .uri = "/api/*",
            .method = HTTP_GET,
            .handler = http_get_handler,
            .user_ctx = NULL,
        };
        httpd_register_uri_handler(server, &uri_root);

        // Register handler for root ("/public/js/*")
        httpd_uri_t uri_public = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = public_handler,
            .user_ctx = NULL,
        };
        httpd_register_uri_handler(server, &uri_public);
    }

    return server;
}

// Main application entry point
void app_main(void) {
    spiffs_init();
    
    // Initialize NVS
    ESP_ERROR_CHECK(nvs_flash_init());

    // Configure GPIO
    gpio_reset_pin(OUTPUT12);
    gpio_reset_pin(OUTPUT14);
    gpio_set_direction(OUTPUT12, GPIO_MODE_OUTPUT);
    gpio_set_direction(OUTPUT14, GPIO_MODE_OUTPUT);
    gpio_set_level(OUTPUT12, 0);
    gpio_set_level(OUTPUT14, 0);

    // Connect to Wi-Fi
    wifi_init();

    // Start web server
    start_webserver();
}

