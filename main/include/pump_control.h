

#ifndef PUMP_CONTROL_H
#define PUMP_CONTROL_H

#include "bdc_motor.h"

#define BDC_MCPWM_TIMER_RESOLUTION_HZ   10000000 // 10MHz, 1 tick = 0.1us
#define BDC_MCPWM_FREQ_HZ               25000    // 25KHz PWM
#define BDC_MCPWM_DUTY_TICK_MAX         (BDC_MCPWM_TIMER_RESOLUTION_HZ / BDC_MCPWM_FREQ_HZ)
#define BDC_MCPWM_GPIO_A                2

#define PUMP_IN1_GPIO                   4
#define PUMP_IN2_GPIO                   5


void pump_init(bdc_motor_handle_t* motor);

void pump_set_direction_clockwise();
void pump_set_direction_anticlockwise();

void pump_set_speed(bdc_motor_handle_t* motor, float speed);
void pump_stop();

#endif //PUMP_CONTROL_H
