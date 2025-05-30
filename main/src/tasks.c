#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "memory.h"
#include "wifi_provisioning/manager.h"
#include "esp_http_client.h"

#include "tasks.h"
#include "valve_control.h"
#include "pump_control.h"
#include "driver/gpio.h"
#include "hc_sr04.h"

#include "setup.h"

#include "ws2812_led_strip.h"
#include "display_functions.h"
#include "pn532.h"
#include "api_requests.h"


extern pn532_t pn532_dev;

static void factory_reset() {
    // Reset WiFi credentials
    wifi_prov_mgr_reset_provisioning();

    esp_restart();
}


void water_regulator_timer_task(TimerHandle_t xTimer) {
    const float distance = hcsr04_read_distance_cm();

    if(distance < 0) {
        ESP_LOGE(TAG, "Water level sensor error!");
        valve_close();
        valve_status = CLOSED;
    }
    else {
    #ifdef WATER_REGULATOR_DEBUG
            ESP_LOGI(TAG, "Distance: %.4f", distance);
    #endif


        if(distance < WATER_ALARM_LEVEL) {
            valve_close();
            valve_status = CLOSED;
            ESP_LOGW(TAG, "Water level too high!!!");
        }

        if(pump_status != WORKING) {
            if(distance > WATER_Y_ZAD + WATER_HYSTERESIS / 2 && valve_status == CLOSED) {
                gpio_set_level(LD2, 0);
                valve_open();
                valve_status = OPEN;
            }
            else if (distance < WATER_Y_ZAD - WATER_HYSTERESIS / 2 && valve_status == OPEN) {
                gpio_set_level(LD2, 1);
                valve_close();
                valve_status = CLOSED;
            }
        }
    }
}

void tag_pouring_task() {
    uint8_t uid[7];
    uint8_t uid_length;

    char* rfid_tag = "29:227:38:133:3:16:128";

    while(1) {
        // TODO Change to simple isTagPresent(), without checking UID
        bool success = pn532_readPassiveTargetID(&pn532_dev, 0x00, uid, &uid_length, 1000);

        if (success) {
            ESP_LOGI(TAG, "UID length: %d", uid_length);
            ESP_LOGI(TAG, "UID: %02X:%02X:%02X:%02X:%02X:%02X:%02X",
                     uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6]);

            // TODO Get info about UID and milliliters

            // TODO Check if reading 4 pages at the time is possible. If so then create new function read4Pages
            // and another function with reads the whole tag memory
            uint8_t buffer[4];
            for (uint8_t page = 4; page < 39; page++) {
                if (pn532_ntag2xx_ReadPage(&pn532_dev, page, buffer) == 1) {
                    ESP_LOGI(TAG, "Page %d: %02X %02X %02X %02X | '%c' '%c' '%c' '%c'",
                     page,
                     buffer[0], buffer[1], buffer[2], buffer[3],
                     (buffer[0] >= 32 && buffer[0] <= 126) ? buffer[0] : '.',
                     (buffer[1] >= 32 && buffer[1] <= 126) ? buffer[1] : '.',
                     (buffer[2] >= 32 && buffer[2] <= 126) ? buffer[2] : '.',
                     (buffer[3] >= 32 && buffer[3] <= 126) ? buffer[3] : '.');
                } else {
                    ESP_LOGE(TAG, "Failed to read page %d", page);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }


        // TODO uncomment when TAG related work is ended and then integrate
        // if(memcmp(uid, RFID_TAG, 7) == 0) {
        //     if(xSemaphoreTake(xMotorMutex, 100)) {
        //         pump_status = WORKING;
        //         ESP_LOGI(TAG, "Started pouring!");
        //         // TODO add milliliters passing and signalization (audio)
        //
        //         const double time_us_d = (250.f / 24.16f) * 1000000.0;
        //         const int64_t time_us = (int64_t)time_us_d;
        //
        //         ESP_LOGI(TAG, "Time int us: %lld", time_us);
        //         led_strip_clear(led_strip);
        //         pump_set_speed(&motor, 1.0f);
        //         const int64_t start_time = esp_timer_get_time();
        //         uint32_t led_idx = 0;
        //         while(esp_timer_get_time() - start_time < time_us) {
        //             const double elapsed_time = (double)(esp_timer_get_time() - start_time)/1000000.0;
        //             led_idx = round((elapsed_time / (time_us_d / 1000000.0)) * (LED_STRIP_LED_NUM - 1));
        //             led_strip_set_pixel(led_strip, led_idx, 50, 50, 50);
        //             led_strip_refresh(led_strip);
        //
        //             ESP_LOGI(TAG, "Elapsed time: %f", elapsed_time);
        //             vTaskDelay(pdMS_TO_TICKS(10));
        //         }
        //         pump_set_speed(&motor, 0.0f);
        //         led_strip_clear(led_strip);
        //
        //         ESP_LOGI(TAG, "Ended pouring!");
        //         xSemaphoreGive(xMotorMutex);
        //     }
        //
        //     ColorRGB ready_color = {0, 100, 0};
        //     led_strip_set_mono(&led_strip, ready_color);
        //     ssd1306_clear_screen(&dev, false);
        //     pouring_finished_screen(&dev);
        //
        //     post_tag_record(uid, 250);
        //     memset(uid, 0, sizeof(uint8_t)*7);
        //
        //     vTaskDelay(pdMS_TO_TICKS(CUP_READY_DISPLAY_TIME_MS));
        //     welcome_screen(&dev);
        //     led_strip_clear(led_strip);
        //     pump_status = IDLE;
        // }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void manual_pouring_task() {
    ColorRGB color = {50, 50, 50};
    while(1) {
        int status = gpio_get_level(BTN_GPIO);
        if (status == 1) {
            // TODO add signalization (display, audio)
            if(xSemaphoreTake(xMotorMutex, 100)) {
                pump_status = WORKING;
                ESP_LOGI(TAG, "Started pouring!");
                led_strip_clear(led_strip);
                pump_set_speed(&motor, 1.0f);
                while(gpio_get_level(BTN_GPIO)) {
                    led_strip_idle_rotating_animation_iteration(&led_strip, color, 1, 20);
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                pump_set_speed(&motor, 0.0f);
                led_strip_clear(led_strip);
                ESP_LOGI(TAG, "Ended pouring!");
                xSemaphoreGive(xMotorMutex);
            }

            ColorRGB ready_color = {0, 100, 0};
            led_strip_set_mono(&led_strip, ready_color);
            ssd1306_clear_screen(&dev, false);
            pouring_finished_screen(&dev);
            vTaskDelay(pdMS_TO_TICKS(CUP_READY_DISPLAY_TIME_MS));
            welcome_screen(&dev);
            led_strip_clear(led_strip);
            pump_status = IDLE;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void init_task() {

    ColorRGB color = {
        .red = 0,
        .green = 0,
        .blue = 50
    };

    ESP_LOGI(TAG, "Init task start!");
    while(initializing && !factoryResetStarted) {
        led_strip_idle_rotating_animation_iteration(&led_strip, color, 1, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    led_strip_clear(led_strip);
    vTaskDelete(NULL);
}

void factory_reset_task(){

    int64_t last_interrupt_time = 0;
    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Factory reset started!");

        ssd1306_clear_screen(&dev, false);
        led_strip_clear(led_strip);
        factoryResetStarted = true;
        int i = 0;
        const int64_t start_time = esp_timer_get_time();
        last_interrupt_time = esp_timer_get_time();
        while(factoryResetStarted) {
            int64_t now = esp_timer_get_time();

            int64_t time_to_reset = FACTORY_RESET_TIME_US - (esp_timer_get_time() - start_time);
            factory_reset_screen(&dev, (uint8_t)(time_to_reset / 1000000));

            if(now - last_interrupt_time > FACTORY_RESET_TIME_US / LED_STRIP_LED_NUM) {
                last_interrupt_time = now;
                ++i;
            }

            if(i == 8) {
                ESP_LOGI(TAG, "Factory reset!");
                ColorRGB color ={
                    .red = 50,
                    .green = 0,
                    .blue = 0
                };

                led_strip_set_mono(&led_strip, color);

                factory_reset();
                factoryResetStarted = false;
                break;
            }
            if(gpio_get_level(9) == 0) {
                led_strip_set_pixel(led_strip, i, 50, 50, 50);
            }
            else {
                factoryResetStarted = false;
                led_strip_clear(led_strip);
                welcome_screen(&dev);
                ESP_LOGI(TAG, "Factory reset break!");
                break;
            }
            led_strip_refresh(led_strip);
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}