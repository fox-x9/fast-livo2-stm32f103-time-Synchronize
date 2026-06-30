#include "timer.h"
#include "led.h"
#include "usart.h"
#include "gprmc.h"
#include "sys.h"
#include <stdio.h>
#include <string.h>

// 定时器3中断次数计数器
vu16 var_Exp=0;
void TIM3_IRQHandler(void)   //TIM3中断
{
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) //更新事件中断
	{
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);

		var_Exp++;                      // 累加，供回传时间差测量

	// GPRMC 时间更新 (不在ISR中printf, 只准备数据)
		GPRMC_Update();
	}
}

//TIM2 PWM部分初始化
//PWM输出初始化
//arr：自动重装值
//psc：时钟预分频数
void TIM2_PWM_Init(u16 arr,u16 psc)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	TIM_DeInit(TIM2);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1; //TIM_CH1  PA1 pin out
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  //复用推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化GPIO

	//初始化TIM2
	TIM_TimeBaseStructure.TIM_Period = arr;
	TIM_TimeBaseStructure.TIM_Prescaler = psc;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	//初始化TIM2 Channel2 PWM模式
	TIM_OCInitStructure.TIM_Pulse = 50; //初始脉冲宽度
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2; //PWM模式2
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; //使能输出
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High; //输出极性高
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
	TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable); //使能TIM2 CCR2预装载
	TIM_OC2Init(TIM2, &TIM_OCInitStructure); //初始化TIM2 OC2

	TIM_ARRPreloadConfig(TIM2, ENABLE);
	TIM_Cmd(TIM2, ENABLE);  //使能TIM2

	TIM_SetCompare2(TIM2, TIM2->ARR / 2);

	// TIM2从模式将在TIM3_PWM_Init中配置（TIM3→TIM2硬件同步）
}

//TIM3 PWM部分初始化
//PWM输出初始化
//arr：自动重装值
//psc：时钟预分频数
void TIM3_PWM_Init(u16 arr,u16 psc)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);//使能定时器3时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB  | RCC_APB2Periph_AFIO, ENABLE);  //使能GPIO和AFIO复用功能模块时钟

	GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3, ENABLE); //Timer3部分引脚映射  TIM3_CH2->PB5

	//设置该引脚为复用输出功能,输出TIM3 CH2的PWM脉冲波形GPIOB.5
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5; //TIM_CH2
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  //复用推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);//初始化GPIO

	//初始化TIM3
	TIM_TimeBaseStructure.TIM_Period = arr; //自动重装载寄存器周期的值
	TIM_TimeBaseStructure.TIM_Prescaler = psc; //定时器分频值
	TIM_TimeBaseStructure.TIM_ClockDivision = 0; //时钟分割:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM向上计数模式
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); //初始化TIM3的时间基数单位

	//初始化TIM3 Channel2 PWM模式 (PPS窄脉冲: 10ms高电平)
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1; //PWM模式1 (CNT<CCR=高)
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; //比较输出使能
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High; //输出极性高
	TIM_OC2Init(TIM3, &TIM_OCInitStructure);  //初始化TIM3 OC2

	TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);  //使能TIM3 CCR2上的预装载寄存器

	TIM_Cmd(TIM3, ENABLE);  //使能TIM3

	TIM_SetCompare2(TIM3, 100);    // PPS窄脉冲 ~10ms @10KHz

	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE); //使能TIM3更新中断

	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;  //TIM3中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  //抢占优先级0级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;  //响应优先级3级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);  //初始化NVIC寄存器

	/* 硬件同步: TIM3(主)→TIM2(从)
	 * TIM3溢出时自动复位TIM2计数器, 保证相机触发与PPS相位严格对齐
	 * 消除软件写CNT产生的毛刺脉冲
	 */
	TIM_SelectMasterSlaveMode(TIM3, TIM_MasterSlaveMode_Enable);
	TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update);

	TIM_Cmd(TIM2, DISABLE);                             //暂停TIM2
	TIM_SelectSlaveMode(TIM2, TIM_SlaveMode_Reset);     //从模式: 触发复位
	TIM_SelectInputTrigger(TIM2, TIM_TS_ITR2);          //触发源: TIM3
	TIM_Cmd(TIM2, ENABLE);                              //重启, 自动对齐相位
}

