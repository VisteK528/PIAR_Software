#include "hc_sr04.h"
#include "esp_rom_sys.h"

static const char *TAG = "HC-SR04";

void hcsr04_init() {
    gpio_reset_pin(TRIG_GPIO);
    gpio_set_direction(TRIG_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(TRIG_GPIO, 1);

    gpio_reset_pin(ECHO_GPIO);
    gpio_set_direction(ECHO_GPIO, GPIO_MODE_INPUT);
}

float hcsr04_read_distance_cm() {
    gpio_set_level(TRIG_GPIO, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_GPIO, 0);

    int64_t start_time = esp_timer_get_time();
    while (gpio_get_level(ECHO_GPIO) == 0) {
        if (esp_timer_get_time() - start_time > 1000) {
            ESP_LOGW(TAG, "Timeout waiting for ECHO HIGH");
            return -1;
        }
    }

    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_GPIO) == 1) {
        if (esp_timer_get_time() - echo_start > 25000) {
            ESP_LOGW(TAG, "Timeout waiting for ECHO LOW");
            return -1;
        }
    }
    int64_t echo_end = esp_timer_get_time();
    int64_t duration_us = echo_end - echo_start;

    float distance_cm = duration_us * 0.034 / 2.;
    return distance_cm;
}
