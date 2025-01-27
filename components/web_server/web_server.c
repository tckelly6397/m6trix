#include "web_server.h"
#include "esp_log.h"
#include <sys/stat.h>

static const char *TAG = "WEB_SERVER";

const char *get_mime_type(const char *filepath) {
    if (strstr(filepath, ".css")) {
        return "text/css";
    } else if (strstr(filepath, ".html")) {
        return "text/html";
    } else if (strstr(filepath, ".js")) {
        return "application/javascript";
    } else if (strstr(filepath, ".png")) {
        return "image/png";
    } else if (strstr(filepath, ".jpg") || strstr(filepath, ".jpeg")) {
        return "image/jpeg";
    } else if (strstr(filepath, ".svg")) {
        return "image/svg+xml";
    } else {
        return "text/plain"; // Default to plain text
    }
}

esp_err_t serve_file(httpd_req_t *req, const char *filepath) {
    struct stat file_stat;

    // Check if the file exists
    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE("FILE_HANDLER", "File not found: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Open the file
    FILE *file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGE("FILE_HANDLER", "Failed to open file: %s", filepath);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Allocate a buffer for reading the file
    char *buffer = malloc(1024);
    if (!buffer) {
        fclose(file);
        ESP_LOGE("FILE_HANDLER", "Memory allocation failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Set the Content-Type dynamically
    httpd_resp_set_type(req, get_mime_type(filepath));

    // Read and send the file in chunks
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, 1024, file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
            fclose(file);
            free(buffer);
            ESP_LOGE("FILE_HANDLER", "File sending failed");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    }

    // Clean up
    free(buffer);
    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);  // End the response

    ESP_LOGI("FILE_HANDLER", "Served file: %s", filepath);
    return ESP_OK;
}

esp_err_t public_handler(httpd_req_t *req) {
    char filepath[128];

    // Construct the file path based on the request URI
    snprintf(filepath, sizeof(filepath), "/public/%s", req->uri + 1);

    // Use the generic serve_file function
    return serve_file(req, filepath);
}

esp_err_t index_handler(httpd_req_t *req) {
    // Serve the specific file index.html
    return serve_file(req, "/public/index.html");
}

// Define the generic route
httpd_uri_t generic_route = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = public_handler,
    .user_ctx = NULL,
};

esp_err_t unregister_generic_uri(httpd_handle_t server) {
    // Check if the route is already registered
    esp_err_t err = httpd_unregister_uri_handler(server, generic_route.uri, generic_route.method);
    if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "Route %s not registered, will register it now", generic_route.uri);
        return true;
    } else if (err == ESP_OK) {
        ESP_LOGI(TAG, "Route %s was already registered, unregistering it first", generic_route.uri);
        return true;
    } else {
        ESP_LOGE(TAG, "Error while unregistering route %s: %s", generic_route.uri, esp_err_to_name(err));
        return false;
    }
}

// Function to unregister and re-register the /* route
esp_err_t manage_generic_route(httpd_handle_t server) {
    if (!server) {
        ESP_LOGE(TAG, "Server is NULL");
        return ESP_FAIL;
    }

    if (!unregister_generic_uri(server)) {
        return ESP_FAIL;
    }

    // Register the route again
    esp_err_t err = httpd_register_uri_handler(server, &generic_route);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to re-register route %s: %s", generic_route.uri, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Successfully registered route %s", generic_route.uri);
    return ESP_OK;
}

httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Enable wildcard URI matching
    config.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Web server started successfully.");
        
        // Define the generic route
        httpd_uri_t index_route = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL,
        };

        httpd_register_uri_handler(server, &index_route);

        httpd_register_uri_handler(server, &generic_route);

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

// Function to register a new GET route
esp_err_t register_get_route(httpd_handle_t server, const char *uri, esp_err_t (*handler)(httpd_req_t *), void *context) {
    if (!server || !uri || !handler) {
        ESP_LOGE(TAG, "Invalid arguments to registerGetRoute");
        return ESP_FAIL;
    }

    httpd_uri_t get_route = {
        .uri = uri,
        .method = HTTP_GET,
        .handler = handler,
        .user_ctx = context,
    };

    // Unregister generic
    if (!unregister_generic_uri(server)) {
        return ESP_FAIL;
    }

    esp_err_t err = httpd_register_uri_handler(server, &get_route);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GET route %s: %s", uri, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Registered GET route: %s", uri);

    // Manage the generic route after registering the new route
    return manage_generic_route(server);
}

// Function to register a new POST route
esp_err_t register_post_route(httpd_handle_t server, const char *uri, esp_err_t (*handler)(httpd_req_t *), void *context) {
    if (!server || !uri || !handler) {
        ESP_LOGE(TAG, "Invalid arguments to registerPostRoute");
        return ESP_FAIL;
    }

    httpd_uri_t post_route = {
        .uri = uri,
        .method = HTTP_POST,
        .handler = handler,
        .user_ctx = context,
    };

    // Unregister generic
    if (!unregister_generic_uri(server)) {
        return ESP_FAIL;
    }

    esp_err_t err = httpd_register_uri_handler(server, &post_route);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register POST route %s: %s", uri, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Registered POST route: %s", uri);

    // Manage the generic route after registering the new route
    return manage_generic_route(server);
}
