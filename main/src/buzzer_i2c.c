#include "buzzer_i2c.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void buzzer_init(i2c_master_bus_handle_t* i2c0_bus, i2c_master_dev_handle_t* buzzer_dev) {
    i2c_device_config_t buzzer_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BUZZER_I2C_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*i2c0_bus, &buzzer_cfg, buzzer_dev));
}

void buzzer_signal(i2c_master_dev_handle_t* buzzer_dev, uint8_t volume, uint8_t freq_khz, uint8_t interval) {
    uint8_t volume_packet[2] = {BUZZER_I2C_VOL_REG, volume};
    uint8_t freq_packet[2] = {BUZZER_I2C_FREQ_REG, freq_khz};
    uint8_t interval_packet[2] = {BUZZER_I2C_TIME_REG, interval};

    ESP_ERROR_CHECK(i2c_master_transmit(*buzzer_dev, volume_packet, 2, -1));
    ESP_ERROR_CHECK(i2c_master_transmit(*buzzer_dev, freq_packet, 2, -1));
    ESP_ERROR_CHECK(i2c_master_transmit(*buzzer_dev, interval_packet, 2, -1));
}

void buzzer_pouring_finished_signal(i2c_master_dev_handle_t* buzzer_dev) {
    buzzer_signal(buzzer_dev, 100, 3, 2);
    vTaskDelay(pdMS_TO_TICKS(500));
    buzzer_signal(buzzer_dev, 100, 3, 2);
    vTaskDelay(pdMS_TO_TICKS(500));

}

void buzzer_error_signal(i2c_master_dev_handle_t* buzzer_dev) {
    buzzer_signal(buzzer_dev, 100, 2, 10);
}

void buzzer_warning_signal(i2c_master_dev_handle_t* buzzer_dev) {
    buzzer_signal(buzzer_dev, 100, 2, 5);
}