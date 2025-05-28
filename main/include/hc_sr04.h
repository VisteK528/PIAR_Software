

#ifndef HC_SR04_H
#define HC_SR04_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"


#define TRIG_GPIO GPIO_NUM_10
#define ECHO_GPIO GPIO_NUM_11

void hcsr04_init();
float hcsr04_read_distance_cm();


#endif //HC_SR04_H
