#ifndef MOTOR_H
#define MOTOR_H
#include <stdbool.h>
#include <stdint.h>

void Motor_Init(void);
void Motor_SetDuty(int16_t l, int16_t r);   /* -1000..1000 */
void Motor_Coast(void);
void Motor_SetBrake(void);
void Motor_StopWithBrake(uint16_t ms);
void Motor_Task1ms(void);
void Motor_DirService(void);
void Motor_SetTarget(float l_mm_s, float r_mm_s);
void Motor_SpeedLoop10ms(void);
bool Motor_Stalled(void);
void Motor_ClearStall(void);
int16_t Motor_GetDuty(uint8_t idx);
float   Motor_GetTargetMmS(uint8_t idx);

#endif