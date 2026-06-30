#include "gprmc.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

char gprmcStr[7]="$GPRMC,";
int chckNum=0;
char chckNumChar[3];   // 2字符 + '\0' 终止符

int ss=0;
int mm=0;
int hh=0;

/*******************************************************************************
 * 函数名: checkNum
 * 功能  : 计算 GPRMC 字符串的异或校验和
 * 输入  : gprmcContext - GPRMC 数据字符串
 * 返回值: 校验和值（十六进制），失败返回 -1
 *******************************************************************************/
int checkNum(const char *gprmcContext)
{
    int i;
    unsigned char result;

    if (gprmcContext == NULL) 
    {
        return -1;
    }

    result = gprmcContext[1];

    for (i = 2; gprmcContext[i] != '*' && gprmcContext[i] != '\0'; i++)
    {
        result ^= gprmcContext[i];
    }

    if (gprmcContext[i] != '*') 
    {
        return -1;
    }

    return result;
}

/*******************************************************************************
 * 函数名: GPRMC_Update
 * 功能  : UTC 时间递增（hh:mm:ss），拼接 GPRMC 字符串并输出
 * 说明  : 由定时器中断周期性调用
 *******************************************************************************/
void GPRMC_Update(void)
{
    static char value_1[100];
    static char value_2[100];

    // UTC 时间递增
    if(ss < 59){
        ss++;
    }else{
        ss = 0;
        if(mm < 59){
            mm++;
        }else{
            mm = 0;
            if(hh < 23){
                hh++;
            }else{
                hh = 0;
            }
        }
    }
    
    // 拼接 GPRMC 字符串
    sprintf(value_2, "%s%02d%02d%02d%s", gprmcStr, hh, mm, ss, ".00,A,2237.496474,N,11356.089515,E,0.0,225.5,230520,2.3,W,A*");
    strcpy(value_1, value_2);
    chckNum = checkNum(value_1);
    sprintf(chckNumChar, "%02X", chckNum);
    printf("%s", value_2);
    printf("%s\n", chckNumChar);
}
