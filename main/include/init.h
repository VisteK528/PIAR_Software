

#ifndef INITIALIZATION_AND_RESET_TASKS_H
#define INITIALIZATION_AND_RESET_TASKS_H

void peripheral_initialization();
void init_task();
void wifi_setup();

void set_api_token(char* token);
void get_api_token();

#endif //INITIALIZATION_AND_RESET_TASKS_H
