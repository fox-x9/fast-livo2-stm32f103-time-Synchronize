#include "led.h"
#include "delay.h"
//#include "key.h"	// KEY功能已屏蔽（文件保留以备后续启用）
#include "sys.h"
#include "usart.h"
#include "timer.h"
#include "gprmc.h"
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

/* LED 闪烁间隔（ms） */
#define LED_BLINK_INTERVAL   500

extern vu16 var_Exp;
extern volatile u32 uwTick;

/* 启动 SysTick 产生 1ms 中断 */
static void SysTick_Init(void)
{
    // SysTick 时钟源已在 delay_init() 中设为 HCLK/8 = 9MHz
    // LOAD = 9000-1 = 1ms
    SysTick->LOAD = (u32)(SystemCoreClock / 8000) - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

int main(void)
{
    u32 last_led_tick = 0;  // PC13 工作指示灯闪烁计时

	delay_init();	    	//延时函数初始化	  
    SysTick_Init();         //开启 SysTick 1ms 中断
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); 	//设置NVIC中断分组2:2位抢占优先级，2位响应优先级
	uart_init(9600);         // 串口初始化为9600 (Livox GPS同步标准波特率)
 	LED_Init();			    //LED端口初始化
	TIM2_PWM_Init(TIM2_PWM_ARR, TIM2_PWM_PSC); // 10 Hz 相机触发 PA1       
 	TIM3_PWM_Init(TIM3_PWM_ARR, TIM3_PWM_PSC); // 1Hz PPS + 从模式同步 PB5
    LED_PC13 = 1;   // 初始熄灭（LED 低电平有效）

	while(1)
	{
		__WFI();  // 进入休眠，中断唤醒

		// GPRMC 串口输出（非阻塞，由TIM3中断标志触发）
		GPRMC_Output();

		// 非阻塞：每隔 LED_BLINK_INTERVAL ms 翻转一次 PC13
		if (uwTick - last_led_tick >= LED_BLINK_INTERVAL)
        {
            last_led_tick = uwTick;
            LED_PC13 = !LED_PC13;
        }
	}
}
