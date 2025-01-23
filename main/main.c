#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

#define WIFI_SSID "The Station"
#define WIFI_PASS "RZMJ964QB5HE7CAV"
#define OUTPUT12 GPIO_NUM_12
#define OUTPUT14 GPIO_NUM_14

static const char *TAG = "ESP32_Web_Server";
static char output12State[4] = "off";
static char output14State[4] = "off";

// Event group for Wi-Fi connection
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

// Event handler for Wi-Fi events
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi disconnected. Reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Function to initialize Wi-Fi
static void wifi_init(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for Wi-Fi to connect
    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

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
        "<p>GPIO 12 - State: %s</p>"
        "<p><a href=\"/12/on\">ON</a> | <a href=\"/12/off\">OFF</a></p>"
        "<p>GPIO 14 - State: %s</p>"
        "<p><a href=\"/14/on\">ON</a> | <a href=\"/14/off\">OFF</a></p>"
        "</body></html>",
        output12State, output14State);

    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    free(response);

    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Register handler for root ("/")
        httpd_uri_t uri_root = {
            .uri = "/", // Root URI
            .method = HTTP_GET,
            .handler = http_get_handler,
            .user_ctx = NULL,
        };
        httpd_register_uri_handler(server, &uri_root);

        // Register handlers for GPIO controls
        httpd_uri_t uri_gpio12_on = {
            .uri = "/12/on",
            .method = HTTP_GET,
            .handler = http_get_handler,
            .user_ctx = NULL,
        };
        httpd_register_uri_handler(server, &uri_gpio12_on);

        httpd_uri_t uri_gpio12_off = {
            .uri = "/12/off",
            .method = HTTP_GET,
            .handler = http_get_handler,
            .user_ctx = NULL,
        };
        httpd_register_uri_handler(server, &uri_gpio12_off);

        httpd_uri_t uri_gpio14_on = {
            .uri = "/14/on",
            .method = HTTP_GET,
            .handler = http_get_handler,
            .user_ctx = NULL,
        };
        httpd_register_uri_handler(server, &uri_gpio14_on);

        httpd_uri_t uri_gpio14_off = {
            .uri = "/14/off",
            .method = HTTP_GET,
            .handler = http_get_handler,
            .user_ctx = NULL,
        };
        httpd_register_uri_handler(server, &uri_gpio14_off);
    }

    return server;
}



// Main application entry point
void app_main(void) {
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

