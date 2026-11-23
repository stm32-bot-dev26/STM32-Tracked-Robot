#include "rc.h"
#include "app_config.h"
#include "pins.h"
#include "stm32f1xx_hal.h"
#include <string.h>

#define IBUS_LEN 32

static uint8_t  rx_byte;
static uint8_t  frame[IBUS_LEN];
static uint8_t  idx;
static volatile uint16_t channels[RC_CH_COUNT];   /* ISR 写 / 主循环读 */
static volatile uint32_t last_frame_tick;
static volatile uint32_t frame_count;
static volatile uint32_t err_count;    /* 坏帧累计（校验/范围/丢头） */

void RC_Init(void)
{
    idx = 0;
    memset(frame, 0, sizeof(frame));
    last_frame_tick = HAL_GetTick();
    HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
}

static uint16_t tmp_ch[RC_CH_COUNT];

static void RC_Parse(void)
{
    uint16_t sum = 0xFFFFU;
    if (frame[0] != 0x20U || frame[1] != 0x40U) { err_count++; return; }

    for (uint8_t i = 0; i < 30; i++) sum = (uint16_t)(sum - frame[i]);
    if (sum != (uint16_t)(frame[30] | ((uint16_t)frame[31] << 8))) { err_count++; return; }

    /* 通道范围校验（吸收队友方案）：越界整帧丢弃，避免干扰值直接驱动 */
    for (uint8_t i = 0; i < RC_CH_COUNT; i++) {
        tmp_ch[i] = (uint16_t)(((uint16_t)frame[3 + i * 2] << 8) | frame[2 + i * 2]);
        tmp_ch[i] &= 0x0FFFU;
        if (tmp_ch[i] < RC_CH_MIN || tmp_ch[i] > RC_CH_MAX) { err_count++; return; }
    }
    for (uint8_t i = 0; i < RC_CH_COUNT; i++) channels[i] = tmp_ch[i];

    last_frame_tick = HAL_GetTick();
    frame_count++;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        /* 同步：等待 0x20 0x40 帧头 */
        if (idx == 0 && rx_byte != 0x20U) { /* skip */ }
        else if (idx == 1 && rx_byte != 0x40U) { idx = 0; }
        else {
            frame[idx++] = rx_byte;
            if (idx >= IBUS_LEN) {
                RC_Parse();
                /* 帧重同步：在缓冲内找下一个 0x20 0x40，避免错位后长期失效 */
                uint8_t start = IBUS_LEN;
                for (uint8_t i = 1; i < IBUS_LEN - 1; i++) {
                    if (frame[i] == 0x20U && frame[i + 1] == 0x40U) { start = i; break; }
                }
                if (start < IBUS_LEN) {
                    uint8_t n = (uint8_t)(IBUS_LEN - start);
                    memmove(frame, &frame[start], n);
                    idx = n;
                } else {
                    idx = 0;
                }
            }
        }
        HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
    }
}

/* 关键：溢出(ORE)/噪声错误后 HAL 会停掉接收，必须清标志并重新挂上，
   否则 iBUS 会永久失联（表现为 LED 一直快闪，且只能复位恢复） */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_PEFLAG(huart);
        idx = 0;
        /* 用 HAL 官方 API 复位接收状态，避免依赖内部字段 RxState */
        HAL_UART_AbortReceive(huart);
        HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
    }
}

void RC_Task1ms(void) { }

bool RC_Alive(void)
{
    return (HAL_GetTick() - last_frame_tick) < RC_TIMEOUT_MS;
}

uint32_t RC_FrameCount(void) { return frame_count; }
uint32_t RC_ErrorCount(void) { return err_count; }

uint16_t RC_Raw(uint8_t ch)
{
    if (ch >= RC_CH_COUNT) return 1500U;
    return channels[ch] ? channels[ch] : 1500U;
}

static const uint8_t rc_rev[RC_CH_COUNT] = {
    0, 0, RC_REV_THROTTLE, RC_REV_STEER, 0, 0, 0, 0, RC_REV_SHOULDER, RC_REV_ELBOW
};

int16_t RC_Norm(uint8_t ch)
{
    int32_t v = ((int32_t)RC_Raw(ch) - 1500) * 2;
    if (v > 1000)  v = 1000;
    if (v < -1000) v = -1000;
    if (ch < RC_CH_COUNT && rc_rev[ch]) v = -v;
    return (int16_t)v;
}

bool RC_Switch(uint8_t ch)
{
    return RC_Raw(ch) > 1600U;
}