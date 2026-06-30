#include "led.h"
#include "delay.h"
#include "key.h"
#include "sys.h"
#include "usart.h"
#include "timer.h"
//灰色 SWIO  7 左4
//白色 SWCLK 9 左5
//黑色 GND 右 2

/* PWM 频率参数
 * TIM2: 72MHz/(7199+1)=10KHz, ARR=999 → 10Hz  (PA1)
 * TIM3: 72MHz/(7199+1)=10KHz, ARR=9999 → 1Hz  (PB5)
 */
#define TIM2_PWM_ARR   999
#define TIM2_PWM_PSC   7199
#define TIM3_PWM_ARR   9999
#define TIM3_PWM_PSC   7199

extern vu16 var_Exp;
int main(void)
{

	delay_init();	    	 //延时函数初始化	  
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); 	 //设置NVIC中断分组2:2位抢占优先级，2位响应优先级
	//uart_init(115200);	 //串口初始化为115200
	uart_init(9600);
 	LED_Init();			     //LED端口初始化
	TIM2_PWM_Init(TIM2_PWM_ARR, TIM2_PWM_PSC); // 10 Hz    pin_A1       
 	TIM3_PWM_Init(TIM3_PWM_ARR, TIM3_PWM_PSC); // 1 Hz  pin_B5

	while(1)
	{
		__WFI();  // 进入休眠，中断唤醒
	}
}
