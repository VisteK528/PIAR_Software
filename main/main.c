#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "display_functions.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

#include "driver/gpio.h"
#include "i2c_bus.h"

#include "esp_log.h"
#include "led_strip.h"
#include "led_strip_rmt.h"
#include "pump_control.h"
#include "valve_control.h"
#include "esp_event.h"

#include "setup.h"
#include "tasks.h"
#include "pn532.h"
#include "init.h"

// API Token
char API_TOKEN[68];

// Water Regulator
ValveStatus_t valve_status = CLOSED;

// Water dispenser
PumpStatus_t pump_status = IDLE;

// Timer and Task handles
TimerHandle_t xWaterLevelRegulator = NULL;
TaskHandle_t xValidWaterLevelTask = NULL;

TaskHandle_t xTagPourHandle = NULL;
TaskHandle_t xManualPourHandle = NULL;

TaskHandle_t xInitTaskHandle = NULL;
TaskHandle_t xFactoryResetTaskHandle = NULL;

// Other handles
EventGroupHandle_t xWifiConnectingEventGroup = NULL;
SemaphoreHandle_t xMotorMutex;

// Device handles
bdc_motor_handle_t motor;
led_strip_handle_t led_strip;
pn532_t pn532_dev;
bool initializing = true;
bool startup = true;
bool factoryResetStarted = false;
bool validWaterLevel = true;

// I2C setup
i2c_master_bus_config_t i2c_mst_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = 1,
    .scl_io_num = GPIO_NUM_7,
    .sda_io_num = GPIO_NUM_6,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = false,
};

i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = GPIO_NUM_6,
    .sda_pullup_en = GPIO_PULLUP_DISABLE,
    .scl_io_num = GPIO_NUM_7,
    .scl_pullup_en = GPIO_PULLUP_DISABLE,
    .master.clk_speed = 400000,
};

SSD1306_t dev;
i2c_master_bus_handle_t i2c0_bus;
i2c_bus_device_handle_t i2c0_device1;
i2c_bus_device_handle_t i2c0_device2;
i2c_master_dev_handle_t buzzer_dev;


void app_main(void)
{
    peripheral_initialization();
    wifi_setup();
    get_api_token();

    xTaskCreate(factory_reset_task, "factoryResetTask", 4096, NULL, 6, &xFactoryResetTaskHandle);
    ESP_LOGI(TAG, "WATER DISP ready!");

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
