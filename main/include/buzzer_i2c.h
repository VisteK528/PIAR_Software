

#ifndef BUZZER_I2C_H
#define BUZZER_I2C_H

#define BUZZER_I2C_ADDR     42
#define BUZZER_I2C_FREQ_REG 0x00    // in kHz
#define BUZZER_I2C_VOL_REG  0x01    // from 0-255
#define BUZZER_I2C_TIME_REG 0x02    // incrementation by 100ms

#include "driver/i2c_types.h"

void buzzer_init(i2c_master_bus_handle_t* i2c0_bus, i2c_master_dev_handle_t* buzzer_dev);
void buzzer_signal(i2c_master_dev_handle_t* buzzer_dev, uint8_t volume, uint8_t freq_khz, uint8_t interval);
void buzzer_pouring_finished_signal(i2c_master_dev_handle_t* buzzer_dev);

void buzzer_warning_signal(i2c_master_dev_handle_t* buzzer_dev);
void buzzer_error_signal(i2c_master_dev_handle_t* buzzer_dev);

#endif //BUZZER_I2C_H
