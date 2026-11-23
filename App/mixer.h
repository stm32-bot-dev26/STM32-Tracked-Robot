#ifndef MIXER_H
#define MIXER_H
#include <stdbool.h>
#include <stdint.h>

void Mixer_Reset(void);
/* thr/steer: -1000..1000；输出左右目标速度 mm/s；stop=true 表示双摇杆回中 */
void Mixer_Update(int16_t thr, int16_t steer, float *l_mm_s, float *r_mm_s, bool *stop);

#endif
