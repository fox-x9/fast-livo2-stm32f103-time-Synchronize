/*******************************************************************************
 * STM32 GPS TimeSync + Camera Trigger System
 * 平台: STM32F103 (Cortex-M3) / Keil MDK-ARM V5
 *
 * 模块架构:
 *   TIM3 (1Hz PPS, PB5) ──硬件同步──→ TIM2 (10Hz 相机触发, PA1)
 *   USART1 (9600bps)     ──→  $GPRMC 模拟GPS报文
 *   PC13 LED             ──→  500ms 非阻塞闪烁 (工作指示灯)
 *   SysTick              ──→  1ms 系统 tick (uwTick)
 *
 * 硬件接口定义:
 *   PA1  (TIM2_CH2)  →  10Hz PWM 相机触发输出 (50% 占空比)
 *   PB5  (TIM3_CH2)  →  1Hz PPS 秒脉冲输出 (10ms 窄脉冲)
 *   PA9  (USART1_TX) →  GPS 模拟报文 TX (9600bps, 8N1)
 *   PA10 (USART1_RX) →  GPS 串口 RX (备用)
 *   PC13             →  板载 LED  输出 (低电平有效)
 *
 * 数据流向:
 *   TIM3 1Hz ISR → var_Exp++ / GPRMC_Update() / 溢出→TIM2硬件复位
 *   主循环 WFI   → GPRMC_Output() 串口发送 / PC13 LED 闪烁检测
 *
 * 硬件同步:
 *   TIM3 选主模式, 溢出触发 TIM2 从模式复位, 0延迟消除毛刺
 *
 * 禁用: KEY (key.c 保留但 main.c 已注释)
 *******************************************************************************/
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
