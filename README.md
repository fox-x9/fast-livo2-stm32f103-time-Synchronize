# STM32 GPS 时间同步 + 相机触发系统

> 基于 STM32F103 (Cortex-M3) 的 **Fast-LIVO2 时间同步** 前端硬件方案。
> 通过硬件定时器实现 PPS 秒脉冲与相机触发信号的**零延迟硬件同步**，
> 并**由 STM32 模拟 GPS 报文**输出 $GPRMC 数据。

---

## 1. 项目简介

本工程为 **Fast-LIVO2 激光雷达-惯性-视觉里程计** 提供前端时间同步硬件实现：

- 产生 **1Hz PPS 秒脉冲**（模拟真实 GPS 授时信号）
- 产生 **10Hz 相机触发脉冲**（与 PPS 硬件相位对齐）
- **由 STM32 模拟 GPS 报文**，通过串口输出标准 `$GPRMC` NMEA 语句
- 为下游 Fast-LIVO2 提供时间对齐所需的 PPS 触发与 GPS 时间戳

### 核心优势

| 特性 | 说明 |
|------|------|
| 硬件级同步 | TIM3（主）→ TIM2（从）硬件触发复位，**零延迟**消除毛刺 |
| 免外部 GPS | GPS 报文由 STM32 内部软件模拟，无需外接 GPS 模块 |
| 高精度时序 | PPS 上升沿后 10ms 输出 GPRMC，满足 0~900ms 规格 |
| 极低功耗 | 主循环 `__WFI()` 休眠，中断驱动唤醒 |

---

## 2. 硬件接口定义

| 引脚 | 外设 | 功能 | 说明 |
|------|------|------|------|
| `PA1` | TIM2_CH2 | 相机触发输出 | **10Hz PWM**，50% 占空比 |
| `PB5` | TIM3_CH2 | PPS 秒脉冲输出 | **1Hz 窄脉冲**，50ms 高电平 |
| `PA9` | USART1_TX | GPS 模拟报文 TX | 9600bps, 8N1 |
| `PA10` | USART1_RX | GPS 串口 RX | 备用接收 |
| `PC13` | GPIO | 板载 LED | 500ms 闪烁，工作指示 |

> **注意**：TIM3 使用 `GPIO_PartialRemap_TIM3` 部分重映射，`PB5` 为 TIM3_CH2 输出。

---

## 3. 系统架构

### 3.1 模块组成

```
STM32F103 (Cortex-M3)
│
├── TIM3  (1Hz PPS, PB5)  ──主模式──┐
│   └─ 1s 更新中断                     │
│       ├─ var_Exp++                 │  硬件同步
│       └─ GPRMC_Update()            │  (溢出触发复位)
│                                   ↓
├── TIM2  (10Hz 相机触发, PA1) ──从模式──┘
│
├── USART1 (9600bps) ──→ $GPRMC 模拟GPS报文
│
├── PC13 LED  ──→ 500ms 非阻塞闪烁 (工作指示灯)
│
└── SysTick ──→ 1ms 系统 tick (uwTick)
```

### 3.2 数据流向

1. **TIM3** 每 1s 溢出一次（PPS 同步）
2. TIM3 中断中调用 `GPRMC_Update()`，递增 UTC 时间并拼接 GPRMC 字符串
3. TIM3 溢出同时触发 **TIM2 硬件复位**，使相机触发与 PPS 严格对齐
4. 主循环 `__WFI()` 唤醒后调用 `GPRMC_Output()`，延迟 10ms 发送报文

### 3.3 硬件同步机制

TIM3 配置为主模式，溢出事件经 `ITR2` 触发 TIM2 从模式复位：

```c
TIM_SelectMasterSlaveMode(TIM3, TIM_MasterSlaveMode_Enable);
TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update);
TIM_SelectSlaveMode(TIM2, TIM_SlaveMode_Reset);
TIM_SelectInputTrigger(TIM2, TIM_TS_ITR2);
```

- 相机触发与 PPS **相位严格对齐**，0 延迟
- 避免软件写 CNT 产生的毛刺脉冲
- 保证下游时间同步精度

---

## 4. GPS 报文模拟（重点）

> ⚠️ **本系统不使用真实 GPS 模块，$GPRMC 报文完全由 STM32 内部软件模拟。**

### 4.1 工作原理

- 每次 TIM3 秒脉冲中断触发 `GPRMC_Update()`
- UTC 时间（时/分/秒）软件自增
- 经纬度、日期等字段为**固定模拟值**
- 自动计算 NMEA 异或校验和并附加

### 4.2 报文格式

```
$GPRMC,hhmmss.00,A,2237.496474,N,11356.089515,E,0.0,225.5,230520,2.3,W,A*<校验和>
```

### 4.3 模拟字段说明

| 字段 | 值 | 说明 |
|------|-----|------|
| `hhmmss` | 实时递增 | UTC 时间（由软件自增） |
| 纬度 | `2237.496474,N` | 固定模拟值 |
| 经度 | `11356.089515,E` | 固定模拟值 |
| 地速 | `0.0` | 固定模拟值 |
| 航向 | `225.5` | 固定模拟值 |
| 日期 | `230520` | 固定模拟值 |
| 磁偏角 | `2.3,W` | 固定模拟值 |
| 状态 | `A` | 有效定位 |

### 4.4 发送时序

```
PPS 上升沿 (TIM3)
    │
    ├─ 0ms   : GPRMC_Update() 准备数据
    ├─ 10ms  : 主循环开始串口发送（延迟保证报文不先于 PPS）
    └─ ~12ms : 发送完成
```

- 发送延迟 `GPRMC_TX_DELAY_MS = 10ms`
- 满足规格要求 **0~900ms**（推荐 0~430ms）

---

## 5. 软件参数配置

参数集中在各模块头文件/源文件中，可根据实际需求修改：

### 5.1 PWM 频率（`main.c`）

| 参数 | 值 | 说明 |
|------|-----|------|
| `TIM2_PWM_ARR` | 999 | 相机触发周期 (10Hz) |
| `TIM2_PWM_PSC` | 7199 | 72MHz/7200=10KHz |
| `TIM3_PWM_ARR` | 9999 | PPS 周期 (1Hz) |
| `TIM3_PWM_PSC` | 7199 | 72MHz/7200=10KHz |

### 5.2 GPRMC 参数（`gprmc.c`）

| 参数 | 值 | 说明 |
|------|-----|------|
| `GPRMC_TX_DELAY_MS` | 10 | PPS 后发送延迟 |
| `GPRMC_LATITUDE` | 2237.496474,N | 模拟纬度 |
| `GPRMC_LONGITUDE` | 11356.089515,E | 模拟经度 |
| `GPRMC_DATE` | 230520 | 模拟日期 |

### 5.3 LED 闪烁（`main.c`）

| 参数 | 值 | 说明 |
|------|-----|------|
| `LED_BLINK_INTERVAL` | 500 | PC13 翻转间隔 (ms) |

---

## 6. 编译与烧录

### 6.1 环境

- Keil MDK-ARM V5 (ARMCC)
- 芯片：STM32F103 (Cortex-M3)
- 工程文件：`USER/PWM.uvprojx`

### 6.2 编译

在 Keil MDK-ARM 中打开 `USER/PWM.uvprojx`，点击 **Build (F7)** 编译。

### 6.3 烧录

- 使用 ST-Link / J-Link，通过 `USER/PWM.uvprojx` 中的下载配置烧录
- 芯片选择：`STM32F103C8` / `STM32F103ZE` 等（见 `DebugConfig/`）

---

## 7. 快速使用

1. 上电，`PC13` LED 以 **500ms** 间隔闪烁 → 系统正常运行
2. `PA1` 输出 **10Hz** 相机触发脉冲
3. `PB5` 输出 **1Hz** PPS 秒脉冲
4. `PA9` (USART1, 9600bps) 周期性输出 **模拟 GPS `$GPRMC` 报文**

### 串口报文示例

```
$GPRMC,083015.00,A,2237.496474,N,11356.089515,E,0.0,225.5,230520,2.3,W,A*3A
$GPRMC,083016.00,A,2237.496474,N,11356.089515,E,0.0,225.5,230520,2.3,W,A*3B
$GPRMC,083017.00,A,2237.496474,N,11356.089515,E,0.0,225.5,230520,2.3,W,A*3C
```

---

## 8. 目录结构

```
├── CORE/                  # 内核文件 (core_cm3, startup)
├── HARDWARE/
│   ├── GPRMC/             # GPS 报文模拟模块
│   ├── KEY/               # 按键（已屏蔽，保留）
│   ├── LED/               # LED 驱动
│   └── TIMER/             # TIM2/TIM3 PWM + 硬件同步
├── STM32F10x_FWLib/       # STM32 标准外设库
├── SYSTEM/
│   ├── delay/             # 延时函数
│   ├── sys/               # 系统配置
│   └── usart/             # 串口 + printf 重定向
└── USER/                  # 主程序 (main.c, 工程文件)
```

---

## 9. 注意事项

- **GPS 报文为模拟数据**：经纬度/日期等字段为固定值，仅用于时间同步，不代表真实定位
- 若需接入**真实 GPS**，只需将 `$GPRMC` 报文源替换为真实模块，并调整接收解析逻辑
- `KEY` 功能已注释保留，后续可按需启用
- 波特率 9600 为 Livox GPS 同步标准波特率，勿随意修改
