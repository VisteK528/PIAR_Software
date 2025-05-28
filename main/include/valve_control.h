

#ifndef VALVE_CONTROL_H
#define VALVE_CONTROL_H

#define VALVE_GPIO_PIN  3

typedef enum {
    CLOSED = 0,
    OPEN = 1,
} ValveStatus_t;

void valve_init();
void valve_open();
void valve_close();

#endif //VALVE_CONTROL_H
