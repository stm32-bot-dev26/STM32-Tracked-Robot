#include "debug.h"
#include "app_config.h"
#include "rc.h"
#include "encoder.h"
#include "battery.h"
#include "motor.h"
#include "bsp.h"
#include <string.h>

#define DBG_BUF_SIZE 192   /* 文本行实际约 139 字节，128 会写穿到 txbuf1 */
static char txbuf[DBG_BUF_SIZE];
static char txbuf1[DBG_BUF_SIZE];
static bool enabled;
static volatile uint8_t busy3, busy1;

/* 非阻塞发送（吸收队友方案）：上一行没发完就丢弃本行，绝不阻塞主循环。
   PB10(USART3) 与 PA9(USART1) 同时输出，调试线插哪个脚都能收到。 */
static void uart_tx(const char *s)
{
    uint16_t n = (uint16_t)strlen(s);
    if (n == 0) return;

    if (!busy3) {
        memmove(txbuf, s, n + 1);
        busy3 = 1;
        if (HAL_UART_Transmit_IT(&huart3, (uint8_t *)txbuf, n) != HAL_OK) busy3 = 0;
    }
    if (!busy1) {
        memmove(txbuf1, s, n + 1);
        busy1 = 1;
        if (HAL_UART_Transmit_IT(&huart1, (uint8_t *)txbuf1, n) != HAL_OK) busy1 = 0;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == NULL || huart->Instance == NULL) return;
    if (huart->Instance == USART3) busy3 = 0;
    else if (huart->Instance == USART1) busy1 = 0;
}

static char *append_int(char *p, int32_t v, uint8_t width)
{
    char tmp[12];
    uint8_t n = 0;
    bool neg = false;
    if (v < 0) { neg = true; v = -v; }
    if (v == 0) tmp[n++] = '0';
    while (v > 0 && n < sizeof(tmp)) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n < width) tmp[n++] = ' ';
    if (neg) tmp[n++] = '-';
    while (n > 0) *p++ = tmp[--n];
    return p;
}

/* 开机横幅（吸收队友方案）：确认烧的是哪版、参数对不对，
   不用连电脑猜。没有 printf，用现成的 append_int 拼。 */
void DBG_BootBanner(void)
{
    char *p = txbuf;
    memcpy(p, "\r\nBOOT ", 8); p += 8;
    memcpy(p, __DATE__, sizeof(__DATE__) - 1); p += sizeof(__DATE__) - 1;
    *p++ = ' ';
    memcpy(p, __TIME__, sizeof(__TIME__) - 1); p += sizeof(__TIME__) - 1;
    memcpy(p, " | CPR=", 7); p += 7;
    p = append_int(p, (int32_t)ENC_COUNTS_PER_REV, 0);
    memcpy(p, " GEAR=", 6); p += 6;
    p = append_int(p, GEAR_RATIO, 0);
    memcpy(p, " KFFx10=", 8); p += 8;
    p = append_int(p, (int32_t)(PID_KFF * 10.0f), 0);
    memcpy(p, " VMAX=", 6); p += 6;
    p = append_int(p, (int32_t)SPEED_MAX_MM_S, 0);
    memcpy(p, " BRK=", 5); p += 5;
    p = append_int(p, BRAKE_MS, 0);
    memcpy(p, "\r\n", 2); p += 2;
    *p = 0;
    uart_tx(txbuf);
}

void DBG_Init(void)
{
    enabled = (DBG_ENABLE != 0);
}

void DBG_SetEnabled(bool en) { enabled = en; }
bool DBG_Enabled(void) { return enabled; }

void DBG_Line(const char *s) { if (enabled) uart_tx(s); }

/* CSV 模式（吸收队友方案，兼容 VOFA+ FireWater）：
   每 20 ms 一行，10 个通道 + 左右速度 + 电压，方便看波形/调 PI、标定 PPR */
void DBG_Task10ms(void)
{
#if (DBG_MODE_CSV != 0)
    static uint16_t acc;
    if (!enabled) return;
    acc += 10;
    if (acc < 20U) return;
    acc = 0;
    {
        char *p = txbuf;
        p = append_int(p, (int32_t)Motor_GetTargetMmS(0), 0);  *p++ = ',';
        p = append_int(p, (int32_t)Motor_GetTargetMmS(1), 0);  *p++ = ',';
        p = append_int(p, (int32_t)Enc_LeftSpeed(), 0);        *p++ = ',';
        p = append_int(p, (int32_t)Enc_RightSpeed(), 0);       *p++ = ',';
        p = append_int(p, (int32_t)Motor_GetDuty(0), 0);       *p++ = ',';
        p = append_int(p, (int32_t)Motor_GetDuty(1), 0);       *p++ = ',';
        {
            int32_t mv = (int32_t)(Battery_GetVolts() * 10.0f);
            p = append_int(p, mv / 10, 0);
            *p++ = '.';
            *p++ = (char)('0' + (mv % 10));
        }
        memcpy(p, "\r\n", 2); p += 2;
        *p = 0;
    }
    uart_tx(txbuf);
#endif
}

void DBG_Task100ms(void)
{
    static uint16_t acc;
    char *p = txbuf;

    if (!enabled) return;
    if (DBG_MODE_CSV != 0) return;   /* CSV 模式由 DBG_Task10ms 输出 */
    acc += 100;
    if (acc < DBG_PERIOD_MS) return;
    acc = 0;

    /* 打印全部 10 个通道，避免摇杆模式差异导致看错通道 */
    memcpy(p, "CH:", 3); p += 3;
    for (uint8_t i = 0; i < 10; i++) {
        p = append_int(p, RC_Raw(i), 4);
        *p++ = ' ';
    }
    memcpy(p, "| N:", 4); p += 4;
    p = append_int(p, RC_Norm(RC_CH_THROTTLE), 5);
    *p++ = ' ';
    p = append_int(p, RC_Norm(RC_CH_STEER), 5);
    /* 实测速度与电压：验证闭环与编码器 */
    memcpy(p, "| SPD:", 6); p += 6;
    p = append_int(p, (int32_t)Enc_LeftSpeed(), 5);
    p = append_int(p, (int32_t)Enc_RightSpeed(), 5);
    memcpy(p, " | D:", 5); p += 5;
    p = append_int(p, Motor_GetDuty(0), 5);
    p = append_int(p, Motor_GetDuty(1), 5);
    memcpy(p, " | V:", 5); p += 5;
    {
        int32_t mv = (int32_t)(Battery_GetVolts() * 10.0f);
        p = append_int(p, mv / 10, 4);
        *p++ = '.';
        *p++ = (char)('0' + (mv % 10));
    }
    /* 编码器累计计数：手转一圈用来自测 PPR */
    memcpy(p, "| CNT:", 6); p += 6;
    p = append_int(p, Enc_LeftCount(), 6);
    p = append_int(p, Enc_RightCount(), 6);
    memcpy(p, " | E:", 5); p += 5;
    p = append_int(p, (int32_t)RC_ErrorCount(), 6);
    memcpy(p, "\r\n", 2); p += 2;
    *p = 0;
    uart_tx(txbuf);
}