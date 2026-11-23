#include "motor.h"
#include "app_config.h"
#include "encoder.h"
#include "bsp.h"
#include <math.h>

static int16_t brake_timer;
static float   tgt_l, tgt_r;
static float   int_l, int_r;
static bool    stalled;

static int16_t last_duty_l, last_duty_r;
static float   tgt_last_l, tgt_last_r;

/* 换向死区状态 */
typedef struct { int8_t active, pending; uint8_t waiting; uint16_t tmr; } dir_state_t;
static dir_state_t dL = {0,0,0,0}, dR = {0,0,0,0};

/* 计数式堵转（每轮独立） */
static uint16_t stall_tick_ms, grace_ms;
static uint16_t stall_hits_l, stall_hits_r;
static int32_t  cnt_l_prev, cnt_r_prev;

static uint32_t duty_to_ccr(int16_t duty)
{
    int32_t v = (int32_t)duty * (int32_t)PWM_ARR / 1000;
    if (v < 0) v = -v;
    if (v > (int32_t)PWM_ARR) v = (int32_t)PWM_ARR;
    return (uint32_t)v;
}

static int8_t sgn(int16_t v) { return (int8_t)((v > 0) - (v < 0)); }

static uint8_t dir_allow(dir_state_t *d, int8_t want, GPIO_TypeDef *port, uint16_t pin)
{
    if (want == 0) return 1;
    if (want == d->active) return 1;
    if (!d->waiting) {
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
        d->waiting = 1;
        d->tmr = MOTOR_DEADTIME_MS;
        d->pending = want;
    }
    return 0;
}

void Motor_DirService(void)
{
    if (dL.waiting && --dL.tmr == 0) {
        dL.active = dL.pending; dL.waiting = 0;
        HAL_GPIO_WritePin(EN_L_PORT, EN_L_PIN, GPIO_PIN_SET);
    }
    if (dR.waiting && --dR.tmr == 0) {
        dR.active = dR.pending; dR.waiting = 0;
        HAL_GPIO_WritePin(EN_R_PORT, EN_R_PIN, GPIO_PIN_SET);
    }
}

/* 真·滑行：关模块使能（半桥全关），电机自由 */
void Motor_Coast(void)
{
    /* 吸收 V2 意见：滑行要连"计时/等待/目标/积分"一起清掉，
       否则 1 ms 任务还会继续走旧制动计时或重新拉高 EN */
    brake_timer = 0;
    dL.waiting = 0; dR.waiting = 0;
    dL.pending = 0; dR.pending = 0;
    tgt_l = 0.0f; tgt_r = 0.0f;
    int_l = 0.0f; int_r = 0.0f;
    stall_hits_l = 0; stall_hits_r = 0; grace_ms = 0; stall_tick_ms = 0;

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, 0);
    HAL_GPIO_WritePin(EN_L_PORT, EN_L_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(EN_R_PORT, EN_R_PIN, GPIO_PIN_RESET);
    last_duty_l = last_duty_r = 0;
}

/* 真·制动：使能 + 两路满占空（两高边导通，绕组短接到 V+） */
void Motor_SetBrake(void)
{
    HAL_GPIO_WritePin(EN_L_PORT, EN_L_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(EN_R_PORT, EN_R_PIN, GPIO_PIN_SET);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, PWM_ARR);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, PWM_ARR);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, PWM_ARR);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, PWM_ARR);
}

void Motor_StopWithBrake(uint16_t ms)
{
    Motor_SetBrake();
    brake_timer = (int16_t)ms;
    tgt_l = 0.0f; tgt_r = 0.0f;
    int_l = 0.0f; int_r = 0.0f;
    last_duty_l = last_duty_r = 0;
}

void Motor_Task1ms(void)
{
    Motor_DirService();
    if (brake_timer > 0) {
        brake_timer--;
        if (brake_timer == 0) Motor_Coast();   /* 制动窗结束 → 真滑行 */
        else                  Motor_SetBrake();
    }
}

void Motor_Init(void)
{
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
    dL.active = dR.active = 0;
    dL.waiting = dR.waiting = 0;
    Motor_Coast();
}

void Motor_SetDuty(int16_t l, int16_t r)
{
    /* 制动窗口内重新给输出：立刻取消剩余的制动计时，否则 1 ms 任务会继续写 CCR=ARR */
    if (l != 0 || r != 0) brake_timer = 0;

    uint8_t okL = dir_allow(&dL, sgn(l), EN_L_PORT, EN_L_PIN);
    uint8_t okR = dir_allow(&dR, sgn(r), EN_R_PORT, EN_R_PIN);

    last_duty_l = l; last_duty_r = r;

    /* 滑行时 EN 被拉低，同向重新起步必须先把 EN 拉高，否则不出力 */
    if (okL && l != 0) HAL_GPIO_WritePin(EN_L_PORT, EN_L_PIN, GPIO_PIN_SET);
    if (okR && r != 0) HAL_GPIO_WritePin(EN_R_PORT, EN_R_PIN, GPIO_PIN_SET);

    if (okL) {
        if (l >= 0) {
            __HAL_TIM_SET_COMPARE(&htim2, M_L_RPWM_CH, duty_to_ccr(l));
            __HAL_TIM_SET_COMPARE(&htim2, M_L_LPWM_CH, 0);
        } else {
            __HAL_TIM_SET_COMPARE(&htim2, M_L_RPWM_CH, 0);
            __HAL_TIM_SET_COMPARE(&htim2, M_L_LPWM_CH, duty_to_ccr(l));
        }
    } else {
        __HAL_TIM_SET_COMPARE(&htim2, M_L_RPWM_CH, 0);
        __HAL_TIM_SET_COMPARE(&htim2, M_L_LPWM_CH, 0);
    }

    if (okR) {
        if (r >= 0) {
            __HAL_TIM_SET_COMPARE(&htim2, M_R_RPWM_CH, duty_to_ccr(r));
            __HAL_TIM_SET_COMPARE(&htim2, M_R_LPWM_CH, 0);
        } else {
            __HAL_TIM_SET_COMPARE(&htim2, M_R_RPWM_CH, 0);
            __HAL_TIM_SET_COMPARE(&htim2, M_R_LPWM_CH, duty_to_ccr(r));
        }
    } else {
        __HAL_TIM_SET_COMPARE(&htim2, M_R_RPWM_CH, 0);
        __HAL_TIM_SET_COMPARE(&htim2, M_R_LPWM_CH, 0);
    }
}

void Motor_SetTarget(float l_mm_s, float r_mm_s)
{
    tgt_l = l_mm_s; tgt_r = r_mm_s;
    tgt_last_l = l_mm_s; tgt_last_r = r_mm_s;
}

int16_t Motor_GetDuty(uint8_t idx)      { return idx ? last_duty_r : last_duty_l; }
float   Motor_GetTargetMmS(uint8_t idx) { return idx ? tgt_last_r : tgt_last_l; }

/* 条件积分抗饱和：输出顶限幅且误差同向时，不再累积积分 */
static int16_t pi_step(float tgt, float act, float *integ)
{
    float err = tgt - act;
    float u;
    float integ_prev = *integ;

    *integ += err * 0.01f;
    if (*integ >  PID_INT_LIMIT) *integ =  PID_INT_LIMIT;
    if (*integ < -PID_INT_LIMIT) *integ = -PID_INT_LIMIT;

    u = PID_KFF * tgt + PID_KP * err + PID_KI * (*integ);

    /* 输出必须与目标同向（吸收队友方案）：超调时不反向加电当刹车 */
    if (tgt > 0.0f) {
        if (u < 0.0f) u = 0.0f;
        if (u > 1000.0f) { u = 1000.0f; if (err > 0.0f) *integ = integ_prev; }
    } else {
        if (u > 0.0f) u = 0.0f;
        if (u < -1000.0f) { u = -1000.0f; if (err < 0.0f) *integ = integ_prev; }
    }
    return (int16_t)u;
}

void Motor_SpeedLoop10ms(void)
{
    float sl = Enc_LeftSpeed();
    float sr = Enc_RightSpeed();
    int16_t dl, dr;
    int32_t cl, cr;

    if (fabsf(tgt_l) < 0.001f && fabsf(tgt_r) < 0.001f) {
        int_l = 0.0f; int_r = 0.0f;
        /* 自洽：目标归零 → 真滑行；但制动窗口内交给 Motor_Task1ms 处理，别打断 */
        if (brake_timer == 0) {
            Motor_Coast();
        } else {
            Motor_SetDuty(0, 0);
        }
        stall_tick_ms = 0; grace_ms = 0; stall_hits_l = stall_hits_r = 0;
        cnt_l_prev = Enc_LeftCount(); cnt_r_prev = Enc_RightCount();
        return;
    }

    /* 单侧目标为 0（差速/原地转）：该侧必须输出 0，否则会反向加电当刹车 */
    if (fabsf(tgt_l) < 0.001f) { int_l = 0.0f; dl = 0; } else { dl = pi_step(tgt_l, sl, &int_l); }
    if (fabsf(tgt_r) < 0.001f) { int_r = 0.0f; dr = 0; } else { dr = pi_step(tgt_r, sr, &int_r); }
    Motor_SetDuty(dl, dr);

    /* 每轮独立判堵转（任一侧卡住都能发现） */
    if (grace_ms < STALL_GRACE_MS) {
        grace_ms += 10;
    } else {
        stall_tick_ms += 10;
        if (stall_tick_ms >= STALL_SAMPLE_MS) {
            stall_tick_ms = 0;
            cl = Enc_LeftCount();
            cr = Enc_RightCount();
            {
                int16_t adl = last_duty_l < 0 ? (int16_t)-last_duty_l : last_duty_l;
                int16_t adr = last_duty_r < 0 ? (int16_t)-last_duty_r : last_duty_r;
                /* 只有该侧真在给油才累计，避免未驱动侧误判 */
                if (adl >= STALL_CMD_MIN) {
                    if (cl == cnt_l_prev) { if (stall_hits_l < 0xFFFF) stall_hits_l++; } else stall_hits_l = 0;
                } else stall_hits_l = 0;
                if (adr >= STALL_CMD_MIN) {
                    if (cr == cnt_r_prev) { if (stall_hits_r < 0xFFFF) stall_hits_r++; } else stall_hits_r = 0;
                } else stall_hits_r = 0;
            }
            cnt_l_prev = cl;
            cnt_r_prev = cr;
        }
    }

    if (stall_hits_l >= STALL_SAMPLES || stall_hits_r >= STALL_SAMPLES) {
        stalled = true;
        stall_hits_l = stall_hits_r = 0;
        grace_ms = 0;
        Motor_StopWithBrake(BRAKE_MS);
    }
}

bool Motor_Stalled(void) { return stalled; }

void Motor_ClearStall(void)
{
    stalled = false;
    stall_hits_l = stall_hits_r = 0;
    grace_ms = 0; stall_tick_ms = 0;
}