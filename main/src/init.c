#include <string.h>

#include "buzzer_i2c.h"
#include "setup.h"
#include "hc_sr04.h"
#include "display_functions.h"
#include "pn532.h"
#include "tasks.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
#include "nvs_flash.h"


extern TaskHandle_t xInitTaskHandle;
extern TaskHandle_t xFactoryResetTaskHandle;
extern pn532_t pn532_dev;

static volatile int led_enabled = 0;
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

/* Handler for the optional provisioning endpoint registered by the application.
 * The data format can be chosen by applications. Here, we are using plain ascii text.
 * Applications can choose to use other formats like protobuf, JSON, XML, etc.
 * Note that memory for the response buffer must be allocated using heap as this buffer
 * gets freed by the protocomm layer once it has been sent by the transport layer.
 */
static esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                          uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    if (inbuf) {
        ESP_LOGI(TAG, "Received data: %.*s", inlen, (char *)inbuf);
    }
    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL) {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

    return ESP_OK;
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
    hcsr04_init();

    // I2C setup
    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);

    init_display(&dev);
    i2c_master_bus_handle_t i2c0_bus = dev._i2c_bus_handle;

    buzzer_init(&i2c0_bus, &buzzer_dev);
    xMotorMutex = xSemaphoreCreateMutex();


    pn532_spi_init(&pn532_dev, SPI2_CLK, SPI2_MISO, SPI2_MOSI, RFID1_SS);
    pn532_begin(&pn532_dev);

    uint32_t versiondata = pn532_getFirmwareVersion(&pn532_dev);
    if (!versiondata)
    {
        while (1)
        {
            versiondata = pn532_getFirmwareVersion(&pn532_dev);
            if(versiondata) {
                break;
            }
            ESP_LOGI(TAG, "Didn't find PN53x board");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    // Got ok data, print it out!
    ESP_LOGI(TAG, "Found chip PN5 %x", (versiondata >> 24) & 0xFF);
    ESP_LOGI(TAG, "Firmware ver. %d.%d", (versiondata >> 16) & 0xFF, (versiondata >> 8) & 0xFF);

    // configure board to read RFID tags
    pn532_SAMConfig(&pn532_dev);

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

    // Manual btn setup
    gpio_reset_pin(BTN_GPIO);
    gpio_set_direction(BTN_GPIO, GPIO_INPUT_IO);

    // WiFi init
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    welcome_screen(&dev);
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
        wifi_prov_mgr_endpoint_create("API-token-endpoint");

        // Security 1 = Proof of Possession
        const char *pop = "abcd1234";

        char service_name[17];
        uint8_t eth_mac[6];
        esp_read_mac(eth_mac, ESP_MAC_WIFI_STA);
        snprintf(service_name, sizeof(service_name), "WATER_DISP-%02X%02X", eth_mac[4], eth_mac[5]);

        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, pop, service_name, NULL));
        wifi_prov_mgr_endpoint_register("API-token-endpoint", custom_prov_data_handler, NULL);
        wifi_prov_mgr_wait();
        wifi_prov_mgr_deinit();


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



