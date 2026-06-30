#ifndef __GPRMC_H
#define __GPRMC_H
#include "sys.h"

// GPRMC 校验和计算
int checkNum(const char *gprmcContext);

// GPRMC 时间更新与数据输出（供定时器中断调用）
void GPRMC_Update(void);

// 外部可访问的 GPRMC 字符串变量
extern char gprmcStr[7];
extern int chckNum;
extern char chckNumChar[2];
extern int ss;
extern int mm;
extern int hh;

#endif
