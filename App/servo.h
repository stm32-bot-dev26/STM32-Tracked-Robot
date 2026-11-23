#ifndef SERVO_H
#define SERVO_H
#include <stdint.h>

typedef enum { SERVO_SHOULDER = 0, SERVO_ELBOW, SERVO_GRIP, SERVO_COUNT } servo_id_t;

void Servo_Init(void);
void Servo_SetUs(servo_id_t id, uint16_t us);
void Servo_SetNorm(servo_id_t id, int16_t n);
void Servo_Task10ms(void);

#endif
