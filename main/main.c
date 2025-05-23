#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "led_strip.h"
#include "led_strip_rmt.h"
#include "pump_control.h"
#include "valve_control.h"
#include "include/ws2812_led_strip.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_timer.h"

#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"

#define GPIO_INPUT_IO     9
#define GPIO_INPUT_PIN_SEL  (1ULL << GPIO_INPUT_IO)

#define LD2 GPIO_NUM_1
static volatile int led_enabled = 0;

static const char *TAG = "DispenserSoft";

TaskHandle_t xValveHandle = NULL;
TaskHandle_t xInitTaskHandle = NULL;
TaskHandle_t xFactoryResetTaskHandle = NULL;


bdc_motor_handle_t motor;
led_strip_handle_t led_strip;
bool initializing = true;
bool factoryResetStarted = false;

static volatile int64_t last_interrupt_time = 0;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{

    int64_t now = esp_timer_get_time();
    if (now - last_interrupt_time > 250000) {  // 250ms debounce
        if (gpio_get_level(GPIO_INPUT_IO) == 0) {
            last_interrupt_time = now;

            if (led_enabled == 0) {
                led_enabled = 1;
                gpio_set_level(LD2, 0);
            }
            else {
                led_enabled = 0;;
                gpio_set_level(LD2, 1);
            }

            BaseType_t xHigherPriorityTaskWoken = pdFALSE;

            vTaskNotifyGiveFromISR(xFactoryResetTaskHandle, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

void init_task() {

    ColorRGB color = {
        .red = 50,
        .green = 50,
        .blue = 50
    };

    ESP_LOGI(TAG, "Init task start!");
    while(initializing && !factoryResetStarted) {
        led_strip_idle_rotating_animation_iteration(&led_strip, color, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    led_strip_clear(led_strip);
    vTaskDelete(NULL);
}

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG, "Received Wi-Fi credentials"
                         "\n\tSSID     : %s\n\tPassword : %s",
                         (const char *) wifi_sta_cfg->ssid,
                         (const char *) wifi_sta_cfg->password);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
                         "\n\tPlease reset to factory and retry provisioning",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                         "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                break;
            case WIFI_PROV_END:
                ESP_LOGI(TAG, "Provisioning deinitialized!  ");
                initializing = false;
                wifi_prov_mgr_deinit();
                break;
            default:
                break;
        }
    }
}


void peripheral_initialization() {
    led_strip_init(&led_strip);
    ColorRGB color = {
        .red = 50,
        .green = 50,
        .blue = 0
    };

    valve_init();
    pump_init(&motor);
    pump_set_direction_clockwise();

    // TODO temporary LED setup, remove later
    gpio_reset_pin(LD2);
    gpio_set_direction(LD2, GPIO_MODE_OUTPUT);


    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = GPIO_INPUT_PIN_SEL,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(0);

    gpio_isr_handler_add(GPIO_INPUT_IO, gpio_isr_handler, (void*) GPIO_INPUT_IO);

    // WiFi init
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

}

void wifi_setup() {
    // Check if device is already provisioned
    bool provisioned = false;
    wifi_prov_mgr_is_provisioned(&provisioned);

    if (!provisioned) {
        if(xInitTaskHandle == NULL) {
            xTaskCreate(init_task, "initTask", 2048, NULL, 2, &xInitTaskHandle);
        }

        ESP_LOGI("PROV", "Starting BLE provisioning");
        wifi_prov_scheme_ble_set_service_uuid((const uint8_t *)"0000ffff-0000-1000-8000-00805f9b34fb");

        wifi_prov_mgr_config_t config = {
            .scheme = wifi_prov_scheme_ble,
            .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
        };

        ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

        // Security 1 = Proof of Possession
        const char *pop = "abcd1234";

        char service_name[12];
        uint8_t eth_mac[6];
        esp_read_mac(eth_mac, ESP_MAC_WIFI_STA);
        snprintf(service_name, sizeof(service_name), "WATER_DISP-%02X%02X", eth_mac[4], eth_mac[5]);

        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, pop, service_name, NULL));

    } else {
        ESP_LOGI("PROV", "Already provisioned, starting WiFi");
        wifi_config_t wifi_cfg;
        size_t len = sizeof(wifi_cfg);
        esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg);
        ESP_LOGI("WIFI", "Connecting to SSID: %s", wifi_cfg.sta.ssid);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
}

void valve_task() {

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(10));

    }
}


void factory_reset() {
    // Reset WiFi credentials
    wifi_prov_mgr_reset_provisioning();

    esp_restart();
}


void factory_reset_task(){

    int64_t last_interrupt_time = 0;
    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Factory reset started!");

        led_strip_clear(led_strip);
        factoryResetStarted = true;
        int i = 0;
        last_interrupt_time = esp_timer_get_time();
        while(factoryResetStarted) {
            int64_t now = esp_timer_get_time();
            if(now - last_interrupt_time > 625000) {
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
                ESP_LOGI(TAG, "Factory reset break!");
                break;
            }
            led_strip_refresh(led_strip);
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    peripheral_initialization();

    // Create tasks
    xTaskCreate(valve_task, "ValveTask", 2048, NULL, 1, &xValveHandle );
    xTaskCreate(factory_reset_task, "factoryResetTask", 4096, NULL, 5, &xFactoryResetTaskHandle);

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    wifi_setup();

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

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
