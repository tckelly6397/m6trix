#include <stdio.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "web_server.h"
#include "wifi_manager.h"
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

    // Initialize Wi-Fi
    wifi_init();

    // Start the web server
    httpd_handle_t server = start_webserver();
    if (server) {
        // Register custom GPIO route
        httpd_uri_t uri_gpio = {
            .uri = "/gpio/*",
            .method = HTTP_GET,
            .handler = gpio_handler,
            .user_ctx = NULL,
        };
        register_route(server, &uri_gpio);
    }
}

