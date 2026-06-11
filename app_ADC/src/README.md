# ADC Sampling Experiment (app_ADC)

AC850 ADC 采样实验 — 通过 SPI 读取外部 ADC，拨码开关调节采样间隔，串口输出 mV 电压值。

## 硬件

| 外设 | 基地址 | 用途 |
|------|--------|------|
| AXI Quad SPI 0 | `XPAR_AXI_QUAD_SPI_0_BASEADDR` | SPI 主机，读取 ADC |
| AXI Timer 0 | `XPAR_AXI_TIMER_0_BASEADDR` | 可配置周期的采样定时器 |
| AXI GPIO 0 | `XPAR_AXI_GPIO_0_BASEADDR` | Ch1: 16-bit 拨码开关，调节采样间隔 |
| AXI Interrupt Controller | `XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR` | 中断控制器，3 路快速中断 |

- **SPI 配置**：主模式，CPOL=1, CPHA=0
- **ADC**：12-bit 分辨率，参考电压 3.3V

## 架构

```
[拨码开关变化] → switch_handler → 更新定时器间隔
[定时器到期]   → timer_handler  → 启动 SPI 读取 ADC
[SPI 完成]     → adc_handler    → 读取 ADC 数据，设标志位
[主循环]       → 检测标志位     → 串口打印 mV 值
```

### 3 个快速中断处理程序

| ISR | 中断源 | 功能 |
|-----|--------|------|
| `switch_handler` | GPIO 0（In4） | 读取拨码开关，计算定时器重载值，更新采样间隔 |
| `timer_handler` | Timer 0（In6） | 启动一次 SPI 传输（发送 `0x0000` 时钟读取 ADC） |
| `adc_handler` | SPI 0（In1） | 读取 SPI RX 数据，转换为 mV，设 `flag_adc_ready` |

全部使用 `__attribute__((fast_interrupt))` 声明，低延迟。

### 采样间隔

- 基础间隔：0.1s（`BASE_TIME_0_1_S = 10,000,000 - 2` 个 100MHz 周期）
- 拨码开关值 `n`（0–15）→ 采样间隔 `(n+1) × 0.1s`
- 范围：0.1s – 1.6s

### 电压转换

```
voltage_mV = (raw × 3300 / 4095) × 2
```

乘以 2 是因为前端分压电路（等效测量范围 0–6.6V）。

## 构建

在 Vitis IDE 中打开项目并构建，单文件 `main.c`。

## 文件

| 文件 | 说明 |
|------|------|
| `main.c` | 入口 + 3 个快速中断 ISR + 主循环串口输出 |
| `UserConfig.cmake` | CMake 配置（编译选项） |
