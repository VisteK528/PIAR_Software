#include "esp_log.h"
#include "ws2812_led_strip.h"
#include "led_strip_rmt.h"

static const char *TAG = "WS2812 LED STRIP";

void led_strip_init(led_strip_handle_t* handle) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = GPIO_NUM_23,
        .max_leds = 8,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .flags.with_dma = false
    };


    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, handle));
    ESP_ERROR_CHECK(led_strip_clear(*handle));
    led_strip_refresh(*handle);
    ESP_LOGI(TAG, "Led strip initialized successfully!");
}

void led_strip_set_mono(led_strip_handle_t* handle, ColorRGB color) {
    for (int i = 0; i < LED_STRIP_LED_NUM; i++) {
        ESP_ERROR_CHECK(led_strip_set_pixel(*handle, i, color.red, color.green, color.blue));
    }
    led_strip_refresh(*handle);
}

void led_strip_idle_rotating_animation_blocking(led_strip_handle_t* handle, ColorRGB color,
    uint8_t clockwise, uint8_t update_period_ms) {

    int i = 0;
    int j = LED_STRIP_LED_NUM / 2;
    while (1) {
        ESP_ERROR_CHECK(led_strip_clear(*handle));
        ESP_ERROR_CHECK(led_strip_set_pixel(*handle, i, color.red, color.green, color.blue));
        ESP_ERROR_CHECK(led_strip_set_pixel(*handle, j, color.red, color.green, color.blue));
        led_strip_refresh(*handle);
        vTaskDelay(update_period_ms / portTICK_PERIOD_MS);

        if(clockwise == 1) {
            ++i;
            ++j;
            if(i > LED_STRIP_LED_NUM - 1)  i = 0;
            if(j > LED_STRIP_LED_NUM - 1)  j = 0;
        }
        else {
            --i;
            --j;
            if(i < 0)  i = LED_STRIP_LED_NUM - 1;
            if(j < 0)  j = LED_STRIP_LED_NUM - 1;
        }
    }
}

void led_strip_idle_rotating_animation_iteration(led_strip_handle_t* handle, ColorRGB color, uint8_t clockwise, uint32_t time_scaling, animation_state_t *state) {
    if (state->j == -1) {
        state->j = time_scaling * LED_STRIP_LED_NUM / 2;
    }

    ESP_ERROR_CHECK(led_strip_clear(*handle));
    ESP_ERROR_CHECK(led_strip_set_pixel(*handle, state->i / time_scaling, color.red, color.green, color.blue));
    ESP_ERROR_CHECK(led_strip_set_pixel(*handle, state->j / time_scaling, color.red, color.green, color.blue));
    led_strip_refresh(*handle);

    if(clockwise == 1) {
        ++state->i;
        ++state->j;
        if(state->i > LED_STRIP_LED_NUM * time_scaling - 1)  state->i = 0;
        if(state->j > LED_STRIP_LED_NUM * time_scaling - 1)  state->j = 0;
    }
    else {
        --state->i;
        --state->j;
        if(state->i < 0)  state->i = LED_STRIP_LED_NUM * time_scaling - 1;
        if(state->j < 0)  state->j = LED_STRIP_LED_NUM * time_scaling - 1;
    }
}

void led_strip_idle_breathing_animation_blocking(led_strip_handle_t* handle, ColorRGB color, uint8_t update_period_ms) {
    int counter = 0;
    int change = 1;
    const float red_part = (float)color.red / 255.f;
    const float green_part = (float)color.green / 255.f;
    const float blue_part = (float)color.blue / 255.f;

    while (1) {
        for (int i = 0; i < LED_STRIP_LED_NUM; i++) {
            ESP_ERROR_CHECK(led_strip_set_pixel(*handle, i, (int)(red_part*counter),
                (int)(green_part*counter), (int)(blue_part*counter)));
        }

        if(counter == 255 && change == 1) {
            change = -1;
        }
        else if (counter == 0 && change == -1) {
            change = 1;
        }
        counter += change;

        led_strip_refresh(*handle);
        vTaskDelay(update_period_ms / portTICK_PERIOD_MS);
    }
}

void led_strip_idle_breathing_animation_iteration(led_strip_handle_t* handle, ColorRGB color) {
    const float red_part = (float)color.red / 255.f;
    const float green_part = (float)color.green / 255.f;
    const float blue_part = (float)color.blue / 255.f;

    static int counter = 0;
    static int change = 1;


    for (int i = 0; i < LED_STRIP_LED_NUM; i++) {
        ESP_ERROR_CHECK(led_strip_set_pixel(*handle, i, (int)(red_part*counter),
            (int)(green_part*counter), (int)(blue_part*counter)));
    }

    if(counter == 255 && change == 1) {
        change = -1;
    }
    else if (counter == 0 && change == -1) {
        change = 1;
    }
    counter += change;

    led_strip_refresh(*handle);
}