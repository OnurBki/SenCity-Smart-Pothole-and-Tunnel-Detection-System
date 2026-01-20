#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include <stdio.h>

static const char *TAG = "SERVER";

// Handler to download the log file
static esp_err_t download_get_handler(httpd_req_t *req)
{
    FILE* f = fopen("/spiffs/log.csv", "r");
    if (f == NULL) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Set headers to force browser to download file
    httpd_resp_set_type(req, "text/csv");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"citysense_logs.csv\"");

    char chunk[1024];
    size_t chunksize;
    
    // Read file in chunks and send to browser
    while ((chunksize = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }
    
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // End response
    return ESP_OK;
}

// Handler to clear logs
static esp_err_t clear_get_handler(httpd_req_t *req)
{
    // Re-open in 'w' mode to wipe it, then close
    FILE* f = fopen("/spiffs/log.csv", "w");
    if (f) {
        fprintf(f, "Time(ms),Shock,Light\n"); // Write header
        fclose(f);
        httpd_resp_send(req, "Logs Cleared!", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_500(req);
    }
    return ESP_OK;
}

// Handler for Root / (Instruction Page)
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char* resp_str = 
        "<h1>CitySense Node</h1>"
        "<p><a href='/log'><button style='font-size:20px'>Download Logs</button></a></p>"
        "<p><a href='/clear'><button style='font-size:20px;color:red'>Clear Logs</button></a></p>";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// URI Configs
static const httpd_uri_t uri_download = { .uri = "/log", .method = HTTP_GET, .handler = download_get_handler };
static const httpd_uri_t uri_clear = { .uri = "/clear", .method = HTTP_GET, .handler = clear_get_handler };
static const httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192; // Give server enough RAM (8KB)
    
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_root);
        httpd_register_uri_handler(server, &uri_download);
        httpd_register_uri_handler(server, &uri_clear);
        ESP_LOGI(TAG, "Web Server Started on Port 80");
    }
}