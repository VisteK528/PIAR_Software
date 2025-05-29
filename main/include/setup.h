

#ifndef SETUP_H
#define SETUP_H

#include "pump_control.h"
#include "ssd1306.h"
#include "valve_control.h"

// General settings
#define FACTORY_RESET_TIME_US       5000000
#define CUP_READY_DISPLAY_TIME_MS   5000

// Water Regulator
//#define WATER_REGULATOR_DEBUG

#define GPIO_INPUT_IO       9
#define GPIO_INPUT_PIN_SEL  (1ULL << GPIO_INPUT_IO)

#define SPI2_MISO           21
#define SPI2_MOSI           22
#define SPI2_CLK            20
#define RFID1_SS            18
#define RFID2_SS            19

#define BTN_GPIO            GPIO_NUM_0
#define LD2                 GPIO_NUM_1

extern ValveStatus_t valve_status;
extern const float WATER_Y_ZAD;
extern const float WATER_HYSTERESIS;
extern const float WATER_ALARM_LEVEL;
extern PumpStatus_t pump_status;
extern const char *TAG;

extern bdc_motor_handle_t motor;
extern led_strip_handle_t led_strip;

extern SemaphoreHandle_t xMotorMutex;
extern SSD1306_t dev;

extern const uint8_t RFID_TAG[7];

extern bool initializing;
extern bool factoryResetStarted;

#endif //SETUP_H
