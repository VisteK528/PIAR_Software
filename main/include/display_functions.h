

#ifndef DISPLAY_FUNCTIONS_H
#define DISPLAY_FUNCTIONS_H

#include "ssd1306.h"

void init_display(SSD1306_t* dev);

void setup_screen(SSD1306_t* dev);
void welcome_screen(SSD1306_t* dev);


void pouring_finished_screen(SSD1306_t* dev);
void factory_reset_screen(SSD1306_t* dev, uint8_t time_to_restart);

void refill_water_tank_info_screen(SSD1306_t* dev);
void pouring_manual_info_screen(SSD1306_t* dev, uint16_t milliliters);
void pouring_max_capacity_limit_trig_info_screen(SSD1306_t* dev);
void pouring_tag_info_screen(SSD1306_t* dev, uint16_t milliliters);
void wifi_connection_attempt_screen(SSD1306_t* dev, uint8_t attempt_number, uint8_t max_attempts);
void failed_to_connect_to_wifi_screen(SSD1306_t* dev);

#endif //DISPLAY_FUNCTIONS_H
