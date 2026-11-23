#ifndef ENCODER_H
#define ENCODER_H
#include <stdint.h>

void    Enc_Init(void);
void    Enc_Task10ms(void);
float   Enc_LeftSpeed(void);
float   Enc_RightSpeed(void);
int32_t Enc_LeftCount(void);
int32_t Enc_RightCount(void);
float   Enc_Distance(void);
void    Enc_Reset(void);

#endif