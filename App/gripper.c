#include "gripper.h"
#include "app_config.h"
#include "rc.h"
#include "servo.h"

typedef enum { GRIP_STATE_OPEN, GRIP_STATE_HOLD, GRIP_STATE_CLOSE } grip_state_t;

static grip_state_t state;

void Gripper_Init(void)
{
    /* 上电默认张开：避免上电瞬间夹到手/夹住待装件 */
    state = GRIP_STATE_OPEN;
    Servo_SetUs(SERVO_GRIP, SERVO_OPEN_US);
}

void Gripper_Open(void)  { state = GRIP_STATE_OPEN;  Servo_SetUs(SERVO_GRIP, SERVO_OPEN_US);  }
void Gripper_Close(void) { state = GRIP_STATE_CLOSE; Servo_SetUs(SERVO_GRIP, SERVO_CLOSE_US); }

bool Gripper_IsClosed(void) { return state == GRIP_STATE_CLOSE; }

/* CH5 三档：拨杆最上 = 张开，中间 = 保持，最下 = 夹紧 */
void Gripper_Task10ms(void)
{
    uint16_t raw = RC_Raw(RC_CH_GRIP);

    if (raw > GRIP_RAW_OPEN) {
        if (state != GRIP_STATE_OPEN) Gripper_Open();
    } else if (raw < GRIP_RAW_CLOSE) {
        if (state != GRIP_STATE_CLOSE) Gripper_Close();
    } else {
        /* 中位：保持当前脉宽，不动作（拨杆回中不会误夹） */
        state = GRIP_STATE_HOLD;
    }
}