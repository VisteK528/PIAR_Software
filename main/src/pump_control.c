#include "esp_log.h"
#include "driver/gpio.h"
#include "pump_control.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ws2812_led_strip.h"


static const char *TAG = "PUMP";

void pump_init(bdc_motor_handle_t* motor) {
    gpio_reset_pin(PUMP_IN1_GPIO);
    gpio_reset_pin(PUMP_IN2_GPIO);
    gpio_set_direction(PUMP_IN1_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(PUMP_IN2_GPIO, GPIO_MODE_OUTPUT);

    bdc_motor_config_t motor_config = {
        .pwm_freq_hz = BDC_MCPWM_FREQ_HZ,
        .pwma_gpio_num = BDC_MCPWM_GPIO_A,
    };

    bdc_motor_mcpwm_config_t mcpwm_config = {
        .group_id = 0,
        .resolution_hz = BDC_MCPWM_TIMER_RESOLUTION_HZ,
    };

    ESP_ERROR_CHECK(bdc_motor_new_mcpwm_device(&motor_config, &mcpwm_config, motor));
    bdc_motor_enable(*motor);
    bdc_motor_forward(*motor);
    ESP_LOGI(TAG, "Pump initialized successfully!");
}

void pump_set_direction_clockwise() {
    gpio_set_level(PUMP_IN1_GPIO, 1);
    gpio_set_level(PUMP_IN2_GPIO, 0);
    ESP_LOGI(TAG, "Pump direction set to clockwise");
}

void pump_set_direction_anticlockwise() {
    gpio_set_level(PUMP_IN1_GPIO, 0);
    gpio_set_level(PUMP_IN2_GPIO, 1);
    ESP_LOGI(TAG, "Pump direction set to anticlockwise");
}

void pump_set_speed(bdc_motor_handle_t* motor, float speed) {
    if(speed <= 1.0f && speed >= 0.0f) {
        bdc_motor_set_speed(*motor, (uint32_t)(speed*BDC_MCPWM_DUTY_TICK_MAX));
        ESP_LOGI(TAG, "Pump speed set up");
    }
}

void pump_stop() {
    gpio_set_level(PUMP_IN1_GPIO, 0);
    gpio_set_level(PUMP_IN2_GPIO, 0);
    ESP_LOGI(TAG, "Pump stop!");
}
