

#ifndef TASKS_H
#define TASKS_H

#include "freertos/timers.h"

void init_task();
void water_regulator_timer_task(TimerHandle_t xTimer);
void tag_pouring_task();
void manual_pouring_task();
void factory_reset_task();
void pour_manager_task();

#endif //TASKS_H
