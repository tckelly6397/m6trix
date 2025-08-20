#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "web_server.h"
#include "network_manager.h"
#include "spiffs_handler.h"
#include "led_strip.h"
#include "driver/rmt_tx.h"
#include "esp_err.h"
#include <stdlib.h>     // strtoul
#include <ctype.h>      // isxdigit
#include <strings.h>    // strcasecmp   (only if you use it)
#include <inttypes.h>   // PRIx32/PRIX32 (only if you use it)


#define OUTPUT12 GPIO_NUM_12
#define OUTPUT14 GPIO_NUM_14

static const char *TAG = "APP_MAIN";

#define LED_GPIO    48
#define LED_RES_HZ  10000000
#define LED_COUNT   1

static led_strip_handle_t led_strip;

void led_init(void) {
    rmt_channel_handle_t chan = NULL;
    rmt_tx_channel_config_t chan_config = {
        .gpio_num = LED_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_RES_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 1,
    };
    rmt_new_tx_channel(&chan_config, &chan);
    rmt_enable(chan);

    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_RES_HZ,
        .mem_block_symbols = 64,
    };
    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
}

// set color with hex 0xRRGGBB
void led_set_color(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8)  & 0xFF;
    uint8_t b = (rgb >> 0)  & 0xFF;
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

// --- helpers ---
static uint32_t s_last_color = 0xFFFFFF; // used for /led/on (restore last color)

static bool parse_hex_rgb_token(const char *tok, uint32_t *out_rgb) {
    if (!tok || !*tok || !out_rgb) return false;

    // skip optional '#' or '0x'
    if (*tok == '#') tok++;
    if (tok[0]=='0' && (tok[1]=='x' || tok[1]=='X')) tok += 2;

    size_t n = strlen(tok);
    for (size_t i = 0; i < n; i++) {
        if (!isxdigit((unsigned char)tok[i])) return false;
    }

    char *endptr = NULL;
    unsigned long val = strtoul(tok, &endptr, 16);
    if (endptr == tok) return false;

    if (n == 3) {
        // #RGB -> #RRGGBB
        unsigned r = (val >> 8) & 0xF;
        unsigned g = (val >> 4) & 0xF;
        unsigned b = (val >> 0) & 0xF;
        val = (r<<20)|(r<<16)|(g<<12)|(g<<8)|(b<<4)|(b<<0);
    } else if (n != 6) {
        return false;  // keep it simple: only 3 or 6 hex digits
    }

    *out_rgb = (uint32_t)val & 0xFFFFFF;
    return true;
}

// --- LED HTTP handler ---
// Supports:
//   /led/on
//   /led/off
//   /led/color/<hex>
static esp_err_t led_handler(httpd_req_t *req) {
    const char *uri = req->uri;  // e.g. "/led/color/ff00bb"

    // Exact matches
    if (strcmp(uri, "/led/on") == 0) {
        led_set_color(s_last_color);
        httpd_resp_send(req, "LED on", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    if (strcmp(uri, "/led/off") == 0) {
        led_set_color(0x000000);
        httpd_resp_send(req, "LED off", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    // Prefix match: /led/color/<hex>
    const char *prefix = "/led/color/";
    size_t prefix_len = strlen(prefix);
    if (strncmp(uri, prefix, prefix_len) == 0) {
        const char *hex = uri + prefix_len;  // points to "<hex>"
        uint32_t rgb;
        if (!parse_hex_rgb_token(hex, &rgb)) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid hex color");
            return ESP_OK;
        }
        led_set_color(rgb);
        if (rgb != 0) s_last_color = rgb; // remember for /led/on
        char msg[64];
        snprintf(msg, sizeof(msg), "LED set to #%06X", (unsigned)rgb);
        httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    // Anything else under /led/* is a 404 for this handler
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Unknown LED path");
    return ESP_OK;
}


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

    // Initialize onboard led
    led_init();

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
        register_get_route(server, "/mode", mode_handler, NULL);

        // Credential management routes
        register_get_route(server, "/credentials", creds_get_handler, NULL);
        register_post_route(server, "/credentials", creds_post_handler, NULL);
        register_get_route(server, "/credentials/delete", creds_delete_handler, NULL);

        // Dynamically register a LED route
        register_get_route(server, "/led/*", led_handler, NULL);
    }
}
