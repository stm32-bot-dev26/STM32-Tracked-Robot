#include "servo.h"
#include "app_config.h"
#include "bsp.h"

static uint16_t cur_us[SERVO_COUNT] = {1500, 1500, 1500};
static uint16_t tgt_us[SERVO_COUNT] = {1500, 1500, 1500};

typedef struct { TIM_HandleTypeDef *h; uint32_t ch; } servo_hw_t;
static const servo_hw_t hw[SERVO_COUNT] = {
    { &htim3, TIM_CHANNEL_1 },   /* 肩   PA6 */
    { &htim3, TIM_CHANNEL_2 },   /* 肘   PA7 */
    { &htim3, TIM_CHANNEL_3 },   /* 夹爪 PB0 */
};

static const struct { uint16_t min_us, max_us, ctr_us; } range[SERVO_COUNT] = {
    { SHOULDER_MIN_US, SHOULDER_MAX_US, SHOULDER_CTR_US },
    { ELBOW_MIN_US,    ELBOW_MAX_US,    ELBOW_CTR_US    },
    { SERVO_MIN_US,    SERVO_MAX_US,    1500            },
};

void Servo_Init(void)
{
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    /* 上电直接落位，之后走斜坡 */
    Servo_SetUs(SERVO_SHOULDER, 1500);
    Servo_SetUs(SERVO_ELBOW,    1500);
    Servo_SetUs(SERVO_GRIP,     SERVO_CLOSE_US);
    for (uint8_t i = 0; i < SERVO_COUNT; i++) {
        cur_us[i] = tgt_us[i];
        __HAL_TIM_SET_COMPARE(hw[i].h, hw[i].ch, cur_us[i]);
    }
}

void Servo_SetUs(servo_id_t id, uint16_t us)
{
    if (id >= SERVO_COUNT) return;
    if (us < SERVO_MIN_US) us = SERVO_MIN_US;
    if (us > SERVO_MAX_US) us = SERVO_MAX_US;
    /* 只更新目标；实际输出由 Servo_Task10ms 按 SERVO_SLEW_US 斜坡推进 */
    tgt_us[id] = us;
}

void Servo_SetNorm(servo_id_t id, int16_t n)
{
    int32_t us, lo, hi;
    if (id >= SERVO_COUNT) return;
    lo = range[id].min_us; hi = range[id].max_us;
    if (n >= 0) us = (int32_t)range[id].ctr_us + ((int32_t)n * (hi - range[id].ctr_us)) / 1000;
    else        us = (int32_t)range[id].ctr_us + ((int32_t)n * (range[id].ctr_us - lo)) / 1000;
    if (us < lo) us = lo;
    if (us > hi) us = hi;
    Servo_SetUs(id, (uint16_t)us);
}

void Servo_Task10ms(void)
{
    for (uint8_t i = 0; i < SERVO_COUNT; i++) {
        if (cur_us[i] != tgt_us[i]) {
            if (tgt_us[i] > cur_us[i])
                cur_us[i] = (uint16_t)((tgt_us[i] - cur_us[i] > SERVO_SLEW_US) ? cur_us[i] + SERVO_SLEW_US : tgt_us[i]);
            else
                cur_us[i] = (uint16_t)((cur_us[i] - tgt_us[i] > SERVO_SLEW_US) ? cur_us[i] - SERVO_SLEW_US : tgt_us[i]);
            __HAL_TIM_SET_COMPARE(hw[i].h, hw[i].ch, cur_us[i]);
        }
    }
}