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
#include "include/ws2812_led_strip.h"

// GPIO assignment
#define LED_STRIP_BLINK_GPIO  23
// Numbers of the LED in the strip
#define LED_STRIP_LED_NUMBERS 10
// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)
led_strip_handle_t led_strip;

static const char *TAG = "example";

led_strip_handle_t configure_led(void)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
#endif
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

void app_main(void)
{
    led_strip_handle_t led_strip;
    led_strip_init(&led_strip);

    bool led_on_off = false;

    ESP_LOGI(TAG, "Start blinking LED strip");
    ColorRGB color = {
        .red = 50,
        .green = 50,
        .blue = 0
    };

    while(1) {
        led_strip_idle_breathing_animation_iteration(&led_strip, color);
        vTaskDelay(pdMS_TO_TICKS(10));
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
