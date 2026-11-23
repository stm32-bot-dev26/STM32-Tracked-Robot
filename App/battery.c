#include "battery.h"
#include "app_config.h"
#include "pins.h"

/* 电压判定的有效窗口：2S~4S 锂电都可能，但必须是"看起来像电池"的值。
   分压未接时 PA5 悬空读到的是噪声，不能判成低压（否则电机永远停）。 */
#define V_VALID_MIN   7.0f     /* 低于 7 V 视为"分压没接/异常"，不判低压 */
#define V_VALID_MAX   18.0f
#define V_RECOVER     10.5f    /* 回滞：升到此值以上才解除低压 */

static float  volts;
static bool   low;
static bool   valid_seen;  /* 曾经读到过合法电压 → 分压确实接了 */
static uint8_t low_cnt;    /* 低压去抖计数：3 次（300 ms）才认定 */

void Battery_Init(void)
{
    volts = 12.0f;
    low   = false;
    valid_seen = false;
    HAL_ADCEx_Calibration_Start(&hadc1);   /* F1 上电校准，提高精度 */
}

void Battery_Task100ms(void)
{
    uint32_t raw = 0;
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 5) == HAL_OK) {
        raw = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    volts = ((float)raw * VREF / ADC_FULL) * BATT_DIV;

    if (volts < V_VALID_MIN || volts > V_VALID_MAX) {
        /* 从没读到过合法值：分压没接（台架/未接线），不判低压，避免误停。
           一旦读到过合法值，后续再读到异常 → 视为分压断线/严重欠压 → 立即停机。 */
        if (valid_seen) { low = true; }
        return;
    }
    valid_seen = true;
    /* 去抖：连续 3 次（约 300 ms）低于阈值才判低压，避免加速瞬间压降误停 */
    if (volts < BATT_LOW_V) {
        if (low_cnt < 3) low_cnt++;
        if (low_cnt >= 3) low = true;
    } else if (volts > V_RECOVER) {
        low_cnt = 0;
        low = false;
    }
}

float Battery_GetVolts(void) { return volts; }
bool  Battery_IsLow(void)    { return low; }