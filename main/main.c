#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "led_strip.h"
#include "led_strip_rmt.h"
#include "pump_control.h"
#include "valve_control.h"
#include "include/ws2812_led_strip.h"



static const char *TAG = "DispenserSoft";

TaskHandle_t xValveHandle = NULL;
TaskHandle_t xInitTaskHandle = NULL;

bdc_motor_handle_t motor;
led_strip_handle_t led_strip;

void valve_task() {

    while(1) {
        //valve_open();
        //pump_set_speed(&motor, 1.0f);
        vTaskDelay(pdMS_TO_TICKS(5000));
        //valve_close();
        //pump_set_speed(&motor, 0.0f);
        vTaskDelay(pdMS_TO_TICKS(5000));

    }
}

void init_task() {

    ColorRGB color = {
        .red = 50,
        .green = 50,
        .blue = 50
    };

    ESP_LOGI(TAG, "Init task start!");
    while(1) {
        led_strip_idle_rotating_animation_iteration(&led_strip, color, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    led_strip_init(&led_strip);
    ColorRGB color = {
        .red = 50,
        .green = 50,
        .blue = 0
    };

    valve_init();
    pump_init(&motor);
    pump_set_direction_clockwise();

    ESP_LOGI(TAG, "Initialized all peripherals! Waiting for user setup...");


    xTaskCreate(init_task, "initTask", 2048, NULL, 2, &xInitTaskHandle );
    xTaskCreate(valve_task, "ValveTask", 2048, NULL, 1, &xValveHandle );

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    led_strip_idle_breathing_animation_blocking(&led_strip, color, 10);

    // while (1) {
    //     if (led_on_off) {
    //         /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    //         for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
    //             ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, 5, 5, 5));
    //         }
    //         /* Refresh the strip to send data */
    //         ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    //         ESP_LOGI(TAG, "LED ON!");
    //     } else {
    //         /* Set all LED off to clear all pixels */
    //         ESP_ERROR_CHECK(led_strip_clear(led_strip));
    //         ESP_LOGI(TAG, "LED OFF!");
    //     }
    //
    //     led_on_off = !led_on_off;
    //     vTaskDelay(pdMS_TO_TICKS(500));
    // }
}
