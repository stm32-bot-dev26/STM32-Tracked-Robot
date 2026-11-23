#ifndef GRIPPER_H
#define GRIPPER_H
#include <stdbool.h>

void Gripper_Init(void);
void Gripper_Task10ms(void);
void Gripper_Open(void);
void Gripper_Close(void);
bool Gripper_IsClosed(void);

#endif
