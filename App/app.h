#ifndef APP_H
#define APP_H
#include <stdbool.h>

void App_Init(void);
void App_FaultKill(void);
void App_Task(void);
bool App_IsArmed(void);
bool App_HasFault(void);

#endif