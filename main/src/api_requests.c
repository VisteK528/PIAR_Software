#include "api_requests.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "setup.h"

#include "esp_tls.h"
#include "esp_crt_bundle.h"


#define POST_DISTRIBUTOR_RECORD "https://piar.blaise.app/dystrybutor/record"
#define API_TOKEN               "Bearer $2b$10$B6SzfbXGQm7WUM9XuXHC/eN129nIFG9lRSYFdGu/GkW83Wr37Gx3G"

static void format_bytes(const uint8_t *data, size_t len, char *out_str, size_t out_str_size) {
    if (len != 7 || out_str_size < 3 * len) {
        snprintf(out_str, out_str_size, "Invalid input");
        return;
    }

    snprintf(out_str, out_str_size,
             "%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             data[0], data[1], data[2],
             data[3], data[4], data[5], data[6]);
}

int post_tag_record(uint8_t* tag, int milliliters) {

    char tag_id[20];
    format_bytes(tag, 7, tag_id, 32);
    char json_payload[128];
    snprintf(json_payload, sizeof(json_payload),
             "{\"tag_id\":\"%s\",\"value\":%d}", tag_id, milliliters);

    esp_http_client_config_t config = {
        .url = POST_DISTRIBUTOR_RECORD,
        .crt_bundle_attach = esp_crt_bundle_attach
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", API_TOKEN);

    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

    esp_err_t err = esp_http_client_perform(client);
    int https_status = esp_http_client_get_status_code(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %d",
                 https_status,
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return https_status;
}
