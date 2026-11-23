#ifndef BSP_H
#define BSP_H
#include "stm32f1xx_hal.h"

extern TIM_HandleTypeDef  htim2;   /* 电机 4 路 PWM PA0-PA3 */
extern TIM_HandleTypeDef  htim3;   /* 舵机 PA6/PA7/PB0 */
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart1;   /* PA9 调试备用 */  /* iBUS PB11 */
extern ADC_HandleTypeDef  hadc1;   /* 电池 PA5 */

extern volatile int32_t g_enc_l;
extern volatile int32_t g_enc_r;

void BSP_Init(void);
void BSP_IwdgFeed(void);
void BSP_GpioInit(void);
void BSP_TimersInit(void);
void BSP_Usart3Init(void);
void BSP_AdcInit(void);

#endif