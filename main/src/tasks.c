#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
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
#include "buzzer_i2c.h"
#include "nvs_flash.h"

#define FILTER_WINDOW_SIZE 11
#define OUTLIER_THRESHOLD 3.5

extern pn532_t pn532_dev;

static float distance_buffer[FILTER_WINDOW_SIZE];
static int buffer_index = 0;
static int buffer_filled = 0;
static float last_valid_distance = 0;

static void extract_data_from_tag(uint8_t* buffer, uint8_t* uid, uint16_t* ml) {
    for(int i = 0; i < 7; i++) {
        char hexStr[3] = {(char)buffer[14+i*3], (char)buffer[15+i*3], '\0'};
        uid[i] = (uint8_t)strtol(hexStr, NULL, 16);
    }

    char hexStr[4] = {(char)buffer[35], (char)buffer[36], (char)buffer[37], '\0'};
    *ml = (uint16_t)strtol(hexStr, NULL, 10);
}

static void factory_reset() {
    // Reset WiFi credentials
    wifi_prov_mgr_reset_provisioning();
    ESP_ERROR_CHECK(nvs_flash_erase());
    esp_restart();
}


static int compare_floats(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

static float median(float* arr, int size) {
    float temp[FILTER_WINDOW_SIZE];
    for (int i = 0; i < size; i++) temp[i] = arr[i];
    qsort(temp, size, sizeof(float), compare_floats);
    return temp[size / 2];
}

static float mad(float* arr, int size, float med) {
    float deviations[FILTER_WINDOW_SIZE];
    for (int i = 0; i < size; i++) deviations[i] = fabsf(arr[i] - med);
    return median(deviations, size);
}

static bool filter_distance(float raw, float* filtered_out) {
    distance_buffer[buffer_index] = raw;
    buffer_index = (buffer_index + 1) % FILTER_WINDOW_SIZE;
    if (buffer_filled < FILTER_WINDOW_SIZE) buffer_filled++;

    if (buffer_filled < FILTER_WINDOW_SIZE) {
        *filtered_out = raw;
        return true;
    }

    float med = median(distance_buffer, FILTER_WINDOW_SIZE);
    float mad_val = mad(distance_buffer, FILTER_WINDOW_SIZE, med);
    float threshold = OUTLIER_THRESHOLD * mad_val;

    if (fabsf(raw - med) > threshold) {
        return false;
    } else {
        *filtered_out = raw;
        return true;
    }
}


void valid_water_level_task() {
    ColorRGB color = {
        .red = 50,
        .green = 50,
        .blue = 0
    };
    animation_state_t anim_sate = {.i = 0, .j = -1};

    ESP_LOGI(TAG, "Valid water task started!");
    while(1) {
        xEventGroupWaitBits(xWifiConnectingEventGroup, SYSTEM_BIT_WIFI_OK,
            false, true, portMAX_DELAY);
        if(validWaterLevel) {
            ssd1306_clear_screen(&dev, false);
            welcome_screen(&dev);

            led_strip_clear(led_strip);

            vTaskSuspend(NULL);
            ssd1306_clear_screen(&dev, false);
            refill_water_tank_info_screen(&dev);
        }

        led_strip_idle_rotating_animation_iteration(&led_strip, color, 1, 1, &anim_sate);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void water_regulator_timer_task(TimerHandle_t xTimer) {
    const float raw_distance = hcsr04_read_distance_cm();

    if(raw_distance < 0) {
        ESP_LOGE(TAG, "Water level sensor error!");
        valve_close();
        valve_status = CLOSED;
        return;
    }

    float distance = 0;
    if (!filter_distance(raw_distance, &distance)) {
        #ifdef WATER_REGULATOR_DEBUG
                ESP_LOGW(TAG, "Outlier detected: %.2f (skipped)", raw_distance);
        #endif
        return;
    }

    last_valid_distance = distance;

    #ifdef WATER_REGULATOR_DEBUG
        ESP_LOGI(TAG, "Distance (filtered): %.4f", distance);
    #endif

    if(distance < WATER_ALARM_LEVEL) {
        valve_close();
        valve_status = CLOSED;
        ESP_LOGW(TAG, "Water level too high!!!");
    }

    if(distance > WATER_Y_ZAD + WATER_HYSTERESIS / 2 && valve_status == CLOSED) {
        validWaterLevel = false;
    }

    if(pump_status != WORKING) {
        if(distance > WATER_Y_ZAD + WATER_HYSTERESIS / 2 && valve_status == CLOSED) {
            gpio_set_level(LD2, 0);
            valve_open();
            valve_status = OPEN;
            validWaterLevel = true;
            if (xValidWaterLevelTask != NULL) {
                vTaskResume(xValidWaterLevelTask);
            }
        }
        else if (distance < WATER_Y_ZAD - WATER_HYSTERESIS / 2 && valve_status == OPEN) {
            gpio_set_level(LD2, 1);
            valve_close();
            valve_status = CLOSED;
            validWaterLevel = true;
        }
    }
}

void tag_pouring_task() {
    uint8_t uid[7];
    uint8_t uid_length;

    while(1) {
        xEventGroupWaitBits(xWifiConnectingEventGroup, SYSTEM_BIT_WIFI_OK,
            false, true, portMAX_DELAY);

        bool success = pn532_readPassiveTargetID(&pn532_dev, 0x00, uid, &uid_length, 1000);

        if (success) {
            Ntag213_t handle;
            if(read_ntag213_data(&pn532_dev, &handle) == 1) {
                uint16_t milliliters = 0;
                extract_data_from_tag(handle.data, uid, &milliliters);
                ESP_LOGI(TAG, "UID: %02X:%02X:%02X:%02X:%02X:%02X:%02X",
                     uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6]);
                ESP_LOGI(TAG, "Milliliters to pour: %d ml", milliliters);
                ESP_LOGI(TAG, "Water status: %d", validWaterLevel);

                if(milliliters != 0) {
                    if(xSemaphoreTake(xMotorMutex, 0) && validWaterLevel) {
                        if(milliliters <= MAX_POUR_MILLILITERS) {
                            pump_status = WORKING;
                            ESP_LOGI(TAG, "Started pouring!");

                            const double time_us_d = ((float)milliliters / PUMP_MILLILITERS_PER_SECOND) * 1000000.0;
                            const int64_t time_us = (int64_t)time_us_d;

                            ssd1306_clear_screen(&dev, false);
                            pouring_tag_info_screen(&dev, milliliters);

                            led_strip_clear(led_strip);
                            pump_set_speed(&motor, 1.0f);
                            const int64_t start_time = esp_timer_get_time();
                            uint32_t led_idx = 0;
                            while(esp_timer_get_time() - start_time < time_us && validWaterLevel) {
                                const double elapsed_time = (double)(esp_timer_get_time() - start_time)/1000000.0;
                                led_idx = round((elapsed_time / (time_us_d / 1000000.0)) * (LED_STRIP_LED_NUM - 1));
                                led_strip_set_pixel(led_strip, led_idx, 50, 50, 50);
                                led_strip_refresh(led_strip);
                                vTaskDelay(pdMS_TO_TICKS(10));
                            }
                            pump_set_speed(&motor, 0.0f);
                            led_strip_clear(led_strip);

                            ESP_LOGI(TAG, "Ended pouring!");

                            int https_status = post_tag_record(uid, milliliters);
                            if (https_status != 201) {
                                ColorRGB ready_color = {50, 50, 0};
                                led_strip_set_mono(&led_strip, ready_color);
                            }
                            else {
                                ColorRGB ready_color = {0, 50, 0};
                                led_strip_set_mono(&led_strip, ready_color);
                            }
                            ssd1306_clear_screen(&dev, false);
                            pouring_finished_screen(&dev);
                            memset(uid, 0, sizeof(uint8_t)*7);

                            buzzer_pouring_finished_signal(&buzzer_dev);
                        }
                        else {
                            pouring_max_capacity_limit_trig_info_screen(&dev);
                            ColorRGB ready_color = {50, 50, 0};
                            led_strip_set_mono(&led_strip, ready_color);
                            buzzer_warning_signal(&buzzer_dev);
                        }
                        vTaskDelay(pdMS_TO_TICKS(CUP_READY_DISPLAY_TIME_MS));
                        welcome_screen(&dev);
                        led_strip_clear(led_strip);
                        pump_status = IDLE;
                        xSemaphoreGive(xMotorMutex);
                    }
                }

            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void manual_pouring_task() {
    ColorRGB color = {50, 50, 50};
    int64_t pouring_start_time = 0;
    int64_t pouring_time = 0;
    uint16_t poured_milliliters = 0;
    while(1) {
        xEventGroupWaitBits(xWifiConnectingEventGroup, SYSTEM_BIT_WIFI_OK,
            false, true, portMAX_DELAY);

        int status = gpio_get_level(BTN_GPIO);
        if (status == 0) {
            if(xSemaphoreTake(xMotorMutex, 0) && validWaterLevel) {
                pump_status = WORKING;
                ESP_LOGI(TAG, "Started pouring!");
                led_strip_clear(led_strip);
                pump_set_speed(&motor, 1.0f);
                pouring_start_time = esp_timer_get_time();
                animation_state_t anim_sate = {.i = 0, .j = -1};

                while(gpio_get_level(BTN_GPIO) == 0 && poured_milliliters <= MAX_POUR_MILLILITERS && validWaterLevel) {
                    led_strip_idle_rotating_animation_iteration(&led_strip, color, 1, 20, &anim_sate);
                    vTaskDelay(pdMS_TO_TICKS(10));
                    pouring_time = esp_timer_get_time() - pouring_start_time;
                    poured_milliliters = (uint16_t)((double)pouring_time / 1000000.0 * PUMP_MILLILITERS_PER_SECOND);
                }


                pump_set_speed(&motor, 0.0f);
                led_strip_clear(led_strip);
                ESP_LOGI(TAG, "Ended pouring!");

                ColorRGB ready_color = {0, 50, 0};
                led_strip_set_mono(&led_strip, ready_color);
                ssd1306_clear_screen(&dev, false);
                pouring_manual_info_screen(&dev, poured_milliliters);
                buzzer_pouring_finished_signal(&buzzer_dev);
                vTaskDelay(pdMS_TO_TICKS(CUP_READY_DISPLAY_TIME_MS));
                welcome_screen(&dev);
                led_strip_clear(led_strip);
                pump_status = IDLE;
                poured_milliliters = 0;
                pouring_start_time = 0;
                pouring_time = 0;

                xSemaphoreGive(xMotorMutex);
            }
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
    animation_state_t anim_sate = {.i = 0, .j = -1};
    while(initializing && !factoryResetStarted) {
        led_strip_idle_rotating_animation_iteration(&led_strip, color, 1, 1, &anim_sate);
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
                buzzer_signal(&buzzer_dev, 100, 2, 10);

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