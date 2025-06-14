

#ifndef WS2812_LED_STRIP_H
#define WS2812_LED_STRIP_H

#include "led_strip.h"

#define LED_STRIP_RMT_RES_HZ    10000000    // 10MHz resolution
#define LED_STRIP_GPIO          23          // GPIO connected to WS2812C
#define LED_STRIP_LED_NUM       8           // Number of LEDs

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} ColorRGB;

typedef struct {
    int i;
    int j;
} animation_state_t;

void led_strip_init(led_strip_handle_t* handle);

void led_strip_set_mono(led_strip_handle_t* handle, ColorRGB color);
//void led_strip_off(led_strip_handle_t* handle);

void led_strip_idle_rotating_animation_blocking(led_strip_handle_t* handle, ColorRGB color, uint8_t clockwise, uint8_t update_period_ms);
void led_strip_idle_breathing_animation_blocking(led_strip_handle_t* handle, ColorRGB color, uint8_t update_period_ms);
void led_strip_idle_rotating_animation_iteration(led_strip_handle_t* handle, ColorRGB color, uint8_t clockwise, uint32_t time_scaling, animation_state_t *state);
void led_strip_idle_breathing_animation_iteration(led_strip_handle_t* handle, ColorRGB color);

#endif //WS2812_LED_STRIP_H
