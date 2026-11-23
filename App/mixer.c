#include "mixer.h"
#include "app_config.h"
#include <math.h>

static float cur_l, cur_r;

void Mixer_Reset(void) { cur_l = 0.0f; cur_r = 0.0f; }

static float ramp_to(float cur, float tgt)
{
    /* 每个调用周期 10 ms */
    float step_up   = MIX_RAMP_UP   * 0.01f;
    float step_dn   = MIX_RAMP_DOWN * 0.01f;
    float d = tgt - cur;
    if (d >  step_up) d =  step_up;
    if (d < -step_dn) d = -step_dn;
    return cur + d;
}

void Mixer_Update(int16_t thr, int16_t steer, float *l_mm_s, float *r_mm_s, bool *stop)
{
    float tgt_l, tgt_r;

    if (thr > -THR_DEADBAND && thr < THR_DEADBAND)     thr = 0;
    if (steer > -STEER_DEADBAND && steer < STEER_DEADBAND) steer = 0;

    if (thr == 0 && steer == 0) {
        cur_l = 0.0f;
        cur_r = 0.0f;
        *l_mm_s = 0.0f;
        *r_mm_s = 0.0f;
        *stop = true;
        return;
    }

    /* 先算归一化比例（-1..1），再做饱和缩放：吸收队友方案的比例归一化，
       避免"满油门+转向"时单轮先饱和导致转向半径变大 */
    float nl = ((float)thr + (float)steer) / 1000.0f;
    float nr = ((float)thr - (float)steer) / 1000.0f;
    float m  = fabsf(nl) > fabsf(nr) ? fabsf(nl) : fabsf(nr);
    if (m > 1.0f) { nl /= m; nr /= m; }
    tgt_l = nl * SPEED_MAX_MM_S;
    tgt_r = nr * SPEED_MAX_MM_S;

    cur_l = ramp_to(cur_l, tgt_l);
    cur_r = ramp_to(cur_r, tgt_r);

    *l_mm_s = cur_l;
    *r_mm_s = cur_r;
    *stop = false;
}