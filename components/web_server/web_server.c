#include "web_server.h"
#include "esp_log.h"
#include <sys/stat.h>

static const char *TAG = "WEB_SERVER";

// Keep the public handler definition here
esp_err_t public_handler(httpd_req_t *req) {
    char filepath[128];
    struct stat file_stat;

    snprintf(filepath, sizeof(filepath), "/public/%s", req->uri + 1);

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE("PUBLIC_HANDLER", "File not found: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    FILE *file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGE("PUBLIC_HANDLER", "Failed to open file: %s", filepath);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *buffer = malloc(1024);
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
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Enable wildcard URI matching
    config.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Web server started successfully.");

        // Register public file handler
        httpd_uri_t uri_public = {
            .uri = "/*",
            .method = HTTP_GET,
            .handler = public_handler,
            .user_ctx = NULL,
        };
        httpd_register_uri_handler(server, &uri_public);

        return server;
    }

    ESP_LOGE(TAG, "Failed to start web server.");
    return NULL;
}

void stop_webserver(httpd_handle_t server) {
    if (server) {
        httpd_stop(server);
        ESP_LOGI(TAG, "Web server stopped.");
    }
}

esp_err_t register_route(httpd_handle_t server, const httpd_uri_t *uri_handler) {
    if (server && uri_handler) {
        return httpd_register_uri_handler(server, uri_handler);
    }
    ESP_LOGE(TAG, "Failed to register route: Server or handler is NULL.");
    return ESP_FAIL;
}
