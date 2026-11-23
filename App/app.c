#include "app.h"
#include "app_config.h"
#include "battery.h"
#include "debug.h"
#include "encoder.h"
#include "gripper.h"
#include "mixer.h"
#include "motor.h"
#include "pins.h"
#include "rc.h"
#include "servo.h"

static uint32_t t1ms, t10ms, t100ms;
static bool armed, fault;
static bool braking;    /* 只在进入停止的沿上给一次制动，之后交给 150 ms 窗口 */

/* 任意 Fault 入口：关履带电机（保留舵机保持力）、LED 常亮，然后死循环 */
void App_FaultKill(void)
{
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 | GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
}

void App_Init(void)
{
    BSP_Init();
    RC_Init();
    Enc_Init();
    Motor_Init();
    Servo_Init();
    Gripper_Init();
    Battery_Init();
    DBG_Init();
    DBG_BootBanner();   /* 开机横幅：版本与关键参数 */
    Mixer_Reset();

    armed = false;
    fault = false;
    t1ms = t10ms = t100ms = HAL_GetTick();
}

bool App_IsArmed(void)  { return armed; }
bool App_HasFault(void) { return fault; }

static void app_safety(void)
{
    if (!RC_Alive()) {
        Motor_Coast();
        armed = false;
        return;
    }
    if (Battery_IsLow()) {
        Motor_Coast();
        fault = true;
        armed = false;   /* 低压解除后需重新回中放行，不自动恢复 */
        return;
    }
}

static void app_loop_1ms(void)
{
    BSP_IwdgFeed();      /* 喂狗：1 s 超时 */
    Motor_Task1ms();
    RC_Task1ms();
    app_safety();
}

/* 底盘：CH3 前后 + CH4 左右（差速混控，可原地转） */
static void app_chassis_10ms(void)
{
    float l_mm_s = 0.0f, r_mm_s = 0.0f;
    bool  stop = true;
    int16_t thr   = RC_Norm(RC_CH_THROTTLE);
    int16_t steer = RC_Norm(RC_CH_STEER);

    if (!armed) {
        if (thr > -THR_DEADBAND && thr < THR_DEADBAND &&
            steer > -STEER_DEADBAND && steer < STEER_DEADBAND) {
            armed = true;
            Mixer_Reset();
        }
        return;
    }

    Mixer_Update(thr, steer, &l_mm_s, &r_mm_s, &stop);

#if (ESTOP_NEED_BOTH == 0)
    /* 任一摇杆回中即停：会禁用原地转，仅调试用 */
    if (thr == 0 || steer == 0) stop = true;
#endif

    /* 堵转后：摇杆回中即解锁，否则保持停机（避免每 10 ms 抽搐式重启） */
    if (Motor_Stalled()) {
        if (stop) {
            Motor_ClearStall();
            braking = false;
        } else if (!braking) {
            Motor_StopWithBrake(BRAKE_MS);
            braking = true;
        }
        return;
    }

    if (stop) {
        /* 边沿触发：只在刚停下时制动一次，150 ms 后由 motor.c 自动转真滑行 */
        if (!braking) {
            Motor_StopWithBrake(BRAKE_MS);
            braking = true;
        }
    } else {
        braking = false;
        Motor_SetTarget(l_mm_s, r_mm_s);
        Motor_SpeedLoop10ms();
    }
}

/* 机械臂：CH9 滚轮 → 肩，CH10 滚轮 → 肘 */
static void app_arm_10ms(void)
{
    Servo_SetNorm(SERVO_SHOULDER, RC_Norm(RC_CH_SHOULDER));
    Servo_SetNorm(SERVO_ELBOW,    RC_Norm(RC_CH_ELBOW));
    Servo_Task10ms();
}

static void app_loop_10ms(void)
{
    Enc_Task10ms();
    DBG_Task10ms();

    if (!RC_Alive() || Battery_IsLow()) {
        return;
    }

    app_chassis_10ms();
    app_arm_10ms();
    Gripper_Task10ms();
}

static void app_loop_100ms(void)
{
    static uint8_t led_div;
    Battery_Task100ms();
    DBG_Task100ms();

    /* LED 状态码（吸收"缺现场反馈"的缺陷）：
       正常     -> 500 ms 慢闪
       遥控失控 -> 100 ms 快闪
       低压     -> 常亮 */
    /* 四态 LED（现场不用连电脑就能分清）：
       常亮          = 低压
       100 ms 快闪   = 遥控断
       250 ms 中闪   = 已连遥控但未放行（摇杆没回中）
       500 ms 慢闪   = 正常可跑 */
    led_div++;
    if (Battery_IsLow()) {
        HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
    } else if (!RC_Alive()) {
        if ((led_div % 1U) == 0U) HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
    } else if (!armed) {
        if ((led_div % 2U) == 0U) HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
    } else {
        if ((led_div % 5U) == 0U) HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
    }
}

void App_Task(void)
{
    uint32_t now = HAL_GetTick();

    /* 1 ms 任务：可以补拍（只做斜坡/喂狗，无积分） */
    uint8_t guard = 0;
    while (now - t1ms >= 1U && guard < 20U) { t1ms += 1U; app_loop_1ms(); guard++; }

    /* 10 ms 控制环：**不补拍**——积分 dt 写死 10 ms，连跑多拍会把 50 ms 的积分
       塞进 0 ms 的真实时间，导致超调/饱和。落后超过 100 ms 直接重同步。 */
    if (now - t10ms >= 10U) {
        t10ms += 10U;
        if (now - t10ms > 100U) t10ms = now;
        app_loop_10ms();
    }

    /* 100 ms 与 1 ms 同风格：加分频，但同样只跑一拍 */
    if (now - t100ms >= 100U) {
        t100ms += 100U;
        if (now - t100ms > 300U) t100ms = now;
        app_loop_100ms();
    }
}