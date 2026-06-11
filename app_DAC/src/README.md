# DAC Experiment (app_DAC)

AC850 DAC 实验 — 1ms 定时器中断驱动 TLV5618 DAC 通过 SPI 输出锯齿波。

## 硬件

| 外设 | 基地址 | 用途 |
|------|--------|------|
| AXI Quad SPI 1 | `XPAR_AXI_QUAD_SPI_1_BASEADDR` | SPI 主机，驱动 TLV5618 DAC |
| AXI Timer 0 | `XPAR_AXI_TIMER_0_BASEADDR` | 1ms 周期定时器，自动重装载 |
| AXI Interrupt Controller | `XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR` | 中断控制器 |

- **DAC**：TI TLV5618，双通道 12-bit DAC，SPI 接口，片选 SS0（SSR=`0xfffe`）
- **SPI 配置**：主模式，CPOL=1, CPHA=0，16 位字长

## 架构

```
Timer ISR (1 kHz)
  │
  ├── volt += 4096.0 / DAC_T     // 递增电压值
  ├── if (volt >= 4096) volt = 0 // 锯齿波回绕
  └── SPI TX: (int)volt & 0x0FFF // 发送到 DAC
```

- **定时器周期**：1ms（`RESET_VALUE = 100MHz / 1000 - 2 = 99998`）
- **锯齿波步数**：`DAC_T = 500`
- **每步增量**：`4096.0 / 500 = 8.192`
- **输出频率**：`1 / (500 × 1ms) = 2 Hz`
- **输出幅度**：0 – 4095（12-bit，`V_ref` 决定实际电压）

## 构建

在 Vitis IDE 中打开项目并构建。使用 `-O0` 编译（该实验无 ISR 时序压力）。

## 配置参数

在 `main.c` 中可调整：

```c
#define DAC_T 500  // 锯齿波周期（定时器 tick 数），默认 500ms
```

改变 `DAC_T` 可调节锯齿波频率：`f_out = 1000 / DAC_T` Hz。

## 文件

| 文件 | 说明 |
|------|------|
| `main.c` | 入口 + 定时器 ISR + SPI DAC 输出 |
| `UserConfig.cmake` | CMake 配置（编译选项） |
