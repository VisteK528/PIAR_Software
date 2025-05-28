

#ifndef DISPLAY_FUNCTIONS_H
#define DISPLAY_FUNCTIONS_H

#include "ssd1306.h"

void init_display(SSD1306_t* dev);

void setup_screen(SSD1306_t* dev);
void welcome_screen(SSD1306_t* dev);


void pouring_finished_screen(SSD1306_t* dev);
void factory_reset_screen(SSD1306_t* dev, uint8_t time_to_restart);

#endif //DISPLAY_FUNCTIONS_H
