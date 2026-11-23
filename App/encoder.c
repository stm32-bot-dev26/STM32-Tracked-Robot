#include "encoder.h"
#include "app_config.h"
#include "bsp.h"

static int32_t last_l, last_r;
static int32_t total_l, total_r;
static float   speed_l, speed_r;

void Enc_Init(void)
{
    last_l = g_enc_l;
    last_r = g_enc_r;
    total_l = 0;
    total_r = 0;
    speed_l = 0.0f;
    speed_r = 0.0f;
}

void Enc_Task10ms(void)
{
    int32_t nl = g_enc_l;
    int32_t nr = g_enc_r;
    int32_t dl = nl - last_l;
    int32_t dr = nr - last_r;

    last_l = nl;
    last_r = nr;
    total_l += dl;
    total_r += dr;

    /* 一阶低通（吸收队友方案）：10 ms 窗口下 1 个计数约 3.6 mm/s，
       不过滤会让 PI 一直抖 */
    {
        float inst_l = ((float)dl) * MM_PER_COUNT / 0.01f;
        float inst_r = ((float)dr) * MM_PER_COUNT / 0.01f;
        const float a = ENC_SPEED_EMA_ALPHA;
        speed_l = (1.0f - a) * speed_l + a * inst_l;
        speed_r = (1.0f - a) * speed_r + a * inst_r;
    }
}

float Enc_LeftSpeed(void)  { return speed_l; }
float Enc_RightSpeed(void) { return speed_r; }
int32_t Enc_LeftCount(void)  { return total_l; }
int32_t Enc_RightCount(void) { return total_r; }

void Enc_Reset(void)
{
    __disable_irq();
    last_l = g_enc_l; last_r = g_enc_r;
    total_l = 0; total_r = 0;
    speed_l = 0.0f; speed_r = 0.0f;
    __enable_irq();
}

float Enc_Distance(void)
{
    return ((float)(total_l + total_r)) * 0.5f * MM_PER_COUNT;
}