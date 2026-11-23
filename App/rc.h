#ifndef RC_H
#define RC_H
#include <stdbool.h>
#include <stdint.h>

void     RC_Init(void);
void     RC_Task1ms(void);
uint16_t RC_Raw(uint8_t ch);
int16_t  RC_Norm(uint8_t ch);
bool     RC_Switch(uint8_t ch);
bool     RC_Alive(void);
uint32_t RC_FrameCount(void);
uint32_t RC_ErrorCount(void);

#endif