# ADC Sampling Experiment (app_ADC)

AC850 ADC 采样实验 — 通过 SPI 读取 ADC128S102，拨码开关调节采样间隔，串口输出 mV 电压值。

## 硬件

| 外设 | 基地址 | 用途 |
|------|--------|------|
| AXI Quad SPI 0 | `XPAR_AXI_QUAD_SPI_0_BASEADDR` | SPI 主机，驱动 ADC128S102（CPOL=1, CPHA=1, Mode 3） |
| AXI Timer 0 | `XPAR_AXI_TIMER_0_BASEADDR` | 100MHz 定时器，自动重装载，产生周期性采样中断 |
| AXI GPIO 0 | `XPAR_AXI_GPIO_0_BASEADDR` | Ch1: 8-bit 输入（拨码开关），Ch2: 8-bit 输出（LED） |
| AXI Interrupt Controller | `XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR` | 中断控制器，快速中断模式，3 路中断输入 |

- **SPI 配置**：主模式，CPOL=1, CPHA=1（SPI Mode 3，匹配 ADC128S102 时序），TX/RX FIFO 复位
- **ADC**：ADC128S102，12-bit 分辨率，8 通道（默认 CH0），参考电压 VA=3.3V
- **定时器**：100MHz 时钟，减计数，自动重装载。Load = (10,000,000 − 2) × (sw + 1) − 2

## 架构

```
[拨码开关变化] → switch_handler → 停止定时器 → 写新 TLR → 重读 TCSR → LOAD → 重使能
[定时器到期]   → timer_handler  → 拉低 CS → 使能 SPI TX 中断 → 写 DTR → 启动传输
[SPI TX 完成]  → adc_handler    → 拉高 CS → 禁传输 → 读 DRR → 设 flag_adc_ready
[主循环]       → 检测标志位     → 串口打印电压值 / 开关信息
```

### 3 个快速中断处理程序

| ISR | INTC 输入 | IVAR 偏移 | 功能 |
|-----|----------|----------|------|
| `switch_handler` | In4（GPIO 0） | +0x10 | 读拨码开关(GPIO_DATA & 0xFF)，计算定时器 TLR，更新采样间隔。停止→写 TLR→重读 TCSR→LOAD→重使能 |
| `timer_handler` | In6（Timer 0） | +0x18 | 拉低 SPI_SSR[0]→使能 TX 空中断→写 0x0000 到 DTR→清除 TRANS_INHIBIT 启动传输 |
| `adc_handler` | In1（SPI 0） | +0x04 | 禁用 SPI 中断→拉高 CS→置 TRANS_INHIBIT→读 DRR(16-bit)→清 TX 空中断→设 flag |

全部使用 `__attribute__((fast_interrupt))` 声明，INTC 快速中断模式自动应答 IAR。

### 采样间隔

- 基础单位：0.1s（100MHz 时钟，10,000,000 个 tick）
- 拨码开关值 `n`（0–255，8-bit GPIO）→ 采样间隔 `(n+1) × 0.1s`
- 范围：0.1s – 25.6s
- 定时器更新序列：停止 → 写新 TLR → 重读 TCSR（干净基准）→ LOAD → 最终配置使能

### 电压转换

```
voltage_mV = raw × 3300 / 4095
```

- ADC 参考电压 VA = 3.3V，12-bit 分辨率 → 1 LSB ≈ 0.806 mV
- 公式与教材第 19 章一致，无需额外补偿系数
- 串口输出格式：`[ADC] 0x0FFF = 3.300V`

## 代码风格

- 全部使用底层寄存器宏：`Xil_In32()` / `Xil_Out32()`，不使用 Vitis 驱动 API
- GPIO 读使用 `Xil_In32()` + `& 0xFF` 掩码（8-bit 通道）
- SPI DRR 读使用 `Xil_In32()` + `& 0xFFFF`（16-bit 帧）
- 外设寄存器通过 `<peripheral>_l.h` 中的偏移和掩码宏访问

## 构建

Vitis IDE 中打开项目并构建。编译选项：`-O0 -g3 -Wall -Wextra`。

## 文件

| 文件 | 说明 |
|------|------|
| `main.c` | 入口 + 3 个快速中断 ISR + 主循环串口输出（181 行） |
| `UserConfig.cmake` | CMake 配置（源文件、编译选项） |
| `lscript.ld` | 链接脚本（LMB BRAM, 堆 0x800, 栈 0x400） |
