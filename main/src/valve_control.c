#include "esp_log.h"
#include "valve_control.h"
#include "driver/gpio.h"

static const char *TAG = "VALVE";

void valve_init() {
    ESP_ERROR_CHECK(gpio_reset_pin(VALVE_GPIO_PIN));
    ESP_ERROR_CHECK(gpio_set_direction(VALVE_GPIO_PIN, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(VALVE_GPIO_PIN, 0));
    ESP_LOGI(TAG, "Valve initialized successfully!");
}

void valve_open() {
    ESP_ERROR_CHECK(gpio_set_level(VALVE_GPIO_PIN, 1));
    ESP_LOGI(TAG, "Valve opened successfully!");
}

void valve_close() {
    ESP_ERROR_CHECK(gpio_set_level(VALVE_GPIO_PIN, 0));
    ESP_LOGI(TAG, "Valve closed successfully!");
}
