#ifndef __GPRMC_H
#define __GPRMC_H
#include "sys.h"

// GPRMC 校验和计算
int checkNum(const char *gprmcContext);

// GPRMC 时间更新（ISR中调用，仅准备数据）
void GPRMC_Update(void);

// GPRMC 串口输出（主循环调用，非阻塞发送）
void GPRMC_Output(void);

// 外部可访问的 GPRMC 字符串变量
extern char gprmcStr[];
extern int chckNum;
extern char chckNumChar[3];
extern int ss;
extern int mm;
extern int hh;

// 主循环轮询标志：1=有待输出的GPRMC数据
extern volatile u8 gprmc_output_flag;

#endif
