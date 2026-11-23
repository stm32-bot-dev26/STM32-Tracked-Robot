#ifndef BATTERY_H
#define BATTERY_H
#include <stdbool.h>

void  Battery_Init(void);
void  Battery_Task100ms(void);
float Battery_GetVolts(void);
bool  Battery_IsLow(void);

#endif
