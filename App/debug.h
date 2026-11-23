#ifndef DEBUG_H
#define DEBUG_H
#include <stdbool.h>
#include <stdint.h>

void DBG_Init(void);
void DBG_BootBanner(void);
void DBG_Task10ms(void);   /* 内部按模式节流 */
void DBG_Task100ms(void);          /* 周期打印通道值 */
void DBG_Line(const char *s);      /* 发一行文本 */
void DBG_SetEnabled(bool en);
bool DBG_Enabled(void);

#endif