#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ===== 引脚定义（飞线版，与 robot_003.ioc 一致） ===== */
/* 电机：两块 BTS7960，每块 1 路 H 桥 → 每电机 2 路 PWM（RPWM/LPWM） */
#define M_L_RPWM_CH  TIM_CHANNEL_1   /* PA0 */
#define M_L_LPWM_CH  TIM_CHANNEL_2   /* PA1 */
#define M_R_RPWM_CH  TIM_CHANNEL_3   /* PA2 */
#define M_R_LPWM_CH  TIM_CHANNEL_4   /* PA3 */
#define EN_L_PORT    GPIOB
#define EN_L_PIN     GPIO_PIN_1
#define EN_R_PORT    GPIOB
#define EN_R_PIN     GPIO_PIN_9
/* 舵机：肩 PA6(TIM3_CH1) / 肘 PA7(TIM3_CH2) / 夹爪 PB0(TIM3_CH3) */
/* 编码器：左 PA10/PA11，右 PB4/PB5（都是 EXTI） */
#define LED_PORT           GPIOC   /* 板载 LED */
#define LED_PIN            GPIO_PIN_13

/* ===== 硬件 ===== */
#define PWM_ARR              3599U     /* TIM2 ARR：20 kHz @72MHz */
#define SERVO_ARR            19999U    /* TIM3 ARR：50 Hz @1MHz  */

/* 编码器 / 轮径 */
/* 编码器常数（可标定，只改 GEAR_RATIO）：
 *   计数/输出圈 = 线数 × 4(AB四倍频) × 减速比
 *   实测：断电机电源，手转输出轴整 1 圈，看串口 CNT 差值
 *     1560 → 1:30（改 GEAR_RATIO=30）
 *     4680 → 1:90（当前，13 线）
 *   安全方向：读数偏大（CPR 偏小）只会让车变慢；读数偏小（CPR 偏大）
 *   会让 PI 顶到满占空比 → 松杆也全速，所以宁小勿大 */
#define ENC_LINES_PER_REV    13
#define ENC_QUAD             4
#define GEAR_RATIO           90
#define ENC_COUNTS_PER_REV   ((float)(ENC_LINES_PER_REV * ENC_QUAD * GEAR_RATIO))
#define WHEEL_DIA_MM         45.0f
#define WHEEL_CIRC_MM        (3.14159265f * WHEEL_DIA_MM)
#define MM_PER_COUNT         (WHEEL_CIRC_MM / ENC_COUNTS_PER_REV)

/* ===== 速度 ===== */
#define SPEED_MAX_MM_S       260.0f    /* 满杆目标速度 */
#define THR_DEADBAND         60        /* CH3 死区：1440–1560 判回中 */
#define STEER_DEADBAND       100       /* CH4 死区：1400–1600 判回中 */

/* 停车判定：
 *   1 = 双摇杆都回中才停（推荐：保留"只打转向=原地转"）
 *   0 = 任一摇杆回中即停（会让原地转失效，仅演示/调试用） */
#define ESTOP_NEED_BOTH      1
#define MIX_RAMP_UP          1200.0f   /* mm/s^2 */
#define MIX_RAMP_DOWN        2400.0f

/* ===== 速度环 ===== */
#define PID_KFF              2.6f      /* ‰/(mm/s)：保守值，标定 KFF=1000/实测最高速 */
#define PID_KP               2.0f
#define PID_KI               6.0f
#define PID_INT_LIMIT        500.0f

/* ===== 制动 ===== */
#define BRAKE_MS             150
#define MOTOR_DEADTIME_MS    2      /* 换向死区（吸收队友方案） */

/* ===== 步进 ===== */

/* ===== 舵机 ===== */
#define SERVO_MIN_US         800
#define SERVO_MAX_US         2200
#define SERVO_OPEN_US        1700      /* 夹爪张开 */
#define SERVO_CLOSE_US       1150      /* 夹爪闭合 */
#define SERVO_SLEW_US        20        /* 每 10ms 最大变化 */

/* ===== 安全 ===== */
#define RC_TIMEOUT_MS        100
#define RC_CH_MIN            750      /* iBUS 合法通道范围（吸收队友方案） */
#define RC_CH_MAX            2250
#define BATT_LOW_V           9.9f
#define BATT_DIV             6.0f
#define VREF                 3.3f
#define ADC_FULL             4095.0f
/* 计数式堵转判据（吸收队友方案）：比"速度阈值"更可靠，低速蠕动不会误判 */
#define STALL_SAMPLE_MS      100
#define STALL_SAMPLES        5
#define STALL_GRACE_MS       500      /* 起步宽限 */
#define STALL_CMD_MIN        150      /* 占空比 ≥15% 才判堵转 */
/* 速度一阶低通（吸收队友方案） */
#define ENC_SPEED_EMA_ALPHA  0.35f

/* ===== 通道反向（摇杆方向不对就改成 1） ===== */
#define RC_REV_THROTTLE  0   /* CH3：向前=正转 */
#define RC_REV_STEER     0   /* CH4：向右=右转 */
#define RC_REV_SHOULDER  0   /* CH9 滚轮 */
#define RC_REV_ELBOW     0   /* CH10 滚轮 */

/* ===== 舵机行程（按机械限位改） ===== */
#define SHOULDER_MIN_US  900
#define SHOULDER_MAX_US  2100
#define SHOULDER_CTR_US  1500
#define ELBOW_MIN_US     900
#define ELBOW_MAX_US     2100
#define ELBOW_CTR_US     1500

/* ===== 调试串口打印 ===== */
#define DBG_ENABLE       1
/* 0 = 人读文本 200 ms；1 = CSV 20 ms（VOFA+ FireWater，10 通道+速度+电压） */
#define DBG_MODE_CSV         0
#define DBG_PERIOD_MS    200

/* ===== 通道（iBUS 0-based） ===== */
#define RC_CH_THROTTLE       2   /* CH3  前进后退 */
#define RC_CH_STEER          3   /* CH4  转向 */
#define RC_CH_GRIP           4   /* CH5  夹爪三档：上=张开 中=保持 下=夹紧 */
#define GRIP_RAW_OPEN        1600  /* >1600 判为张开 */
#define GRIP_RAW_CLOSE       1400  /* <1400 判为夹紧 */
#define RC_CH_SHOULDER       8   /* CH9  肩 */
#define RC_CH_ELBOW          9   /* CH10 肘 */
#define RC_CH_COUNT          10

#endif