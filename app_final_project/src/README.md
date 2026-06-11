# DDS Dual-Channel Signal Generator (app_final_project)

双通道 DDS 信号发生器，运行在 MicroBlaze 软核上。支持本地控制（键盘/数码管/拨码开关/按键）和远程控制（UART 串口协议），通过 TLV5618 双通道 DAC 同步输出。

## 硬件

| 外设 | 基地址 | 用途 |
|------|--------|------|
| AXI Quad SPI 1 | `XPAR_AXI_QUAD_SPI_1_BASEADDR` | SPI 主机，驱动 TLV5618 DAC |
| AXI Timer 0 | `XPAR_AXI_TIMER_0_BASEADDR` | 100 kHz 采样时钟，自动重装载 |
| AXI UART Lite 0 | `XPAR_AXI_UARTLITE_0_BASEADDR` | 串口，接收上位机参数 |
| AXI Interrupt Controller | `XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR` | 中断控制器，5 路中断 |
| AXI GPIO 0 | `XPAR_AXI_GPIO_0_BASEADDR` | Ch1: 8-bit 拨码开关输入, Ch2: 8-bit LED 输出 |
| AXI GPIO 1 | `XPAR_AXI_GPIO_1_BASEADDR` | Ch1: 8-bit 数码管段码, Ch2: 8-bit 位选 |
| AXI GPIO 2 | `XPAR_AXI_GPIO_2_BASEADDR` | Ch1: 模式按键输入（bit0） |
| AXI GPIO 3 | `XPAR_AXI_GPIO_3_BASEADDR` | Ch1: 4-bit 键盘行输入, Ch2: 4-bit 列输出 |

- **DAC**：TI TLV5618 双通道 12-bit DAC，SPI 快速模式（SPD=1），双通道同步更新

## DDS 参数

| 参数 | 值 |
|------|-----|
| 采样率 (F_s) | 100 kHz |
| 相位累加器 | 32-bit |
| 频率分辨率 | F_s / 2^32 ≈ 0.000023 Hz |
| 频率范围 | 0 – 50 kHz（Nyquist） |
| 幅度范围 | 0 – 4095 mV（V_ref=2.048V, ×2 增益） |
| 正弦表 | 1024 点 × 12-bit（~2KB `.rodata`） |
| 默认波形 | 1 kHz 正弦, 1000 mV, 双通道同步 |

## 波形类型

| 索引 | 类型 | 生成方式 |
|------|------|---------|
| 0 | 正弦 | 1024 点 LUT 查表 + 幅度缩放 |
| 1 | 方波 | 相位 MSB 判断正负半周 |
| 2 | 三角 | 相位高位折叠斜坡 |
| 3 | 锯齿 | 相位高位线性斜坡 |
| 4 | 任意 | 预留 |

## SPI DAC 协议（TLV5618 16-bit 帧）

```
| R1 | SPD | PWR | R0 | D11 | ... | D0 |
| 15 | 14  | 13  | 12 | 11  | ... | 0  |
```

快速模式（SPD=1）双通道同步更新：

1. **Step 1**：写 CH2 数据到 BUFFER（`0x5000`），DAC 输出不变
2. 等待 ~1.28µs（5 轮 NOP，保证 CS 高电平）
3. **Step 2**：写 CH1 数据到 DAC A（`0xC000`），CS 上升沿同时更新 A 和 B

## UART 远程协议

### 帧格式（13 字节）

```
| 0xAA | 0x55 | ctrl | type | freq[4] | amp[4] | checksum |
```

| 字段 | 字节 | 说明 |
|------|------|------|
| Header | 0–1 | `0xAA 0x55` 帧同步 |
| ctrl | 2 | bit[7]=模式（0=独立, 1=同步），bit[1:0]=通道（00=CH1, 01=CH2） |
| type | 3 | 0=正弦, 1=方波, 2=三角, 3=锯齿 |
| freq | 4–7 | 频率 Hz，大端 uint32 |
| amp | 8–11 | 幅度 mV，大端 uint32 |
| checksum | 12 | 字节 0–11 的累进 XOR |

### 上位机

```bash
python scripts/dds_host.py --port COM3 --ch1 sine 1000 2000      # CH1: 正弦 1kHz 2V
python scripts/dds_host.py --port COM3 --sync triangle 3000 2048  # 同步: 三角 3kHz 2.048V
```

## 本地控制

| 控件 | 功能 |
|------|------|
| 4×4 矩阵键盘 | 编辑频率/幅度（数字键 0–9、确认、退格、复位、切换、CH1/CH2） |
| 8 位数码管 | 左 4 位显示频率，右 4 位显示幅度 |
| 8-bit 拨码开关 | 选择波形类型（bits[1:0]=CH1, bits[3:2]=CH2；同步模式下仅 bits[1:0]） |
| 模式按键 | 切换独立模式/同步模式 |
| 8-bit LED | 通道指示（bits[1:0]=CH1, bits[3:2]=CH2）+ 同步指示（bit4） |

## 中断架构

5 路中断源，统一入口 `MyISR()` 通过读取 ISR 寄存器分发：

| 中断 | 触发源 | 处理函数 | 功能 |
|------|--------|---------|------|
| Timer 0 | 100 kHz | `Tim_Handler()` | DDS 更新 + DAC 输出 + 数码管扫描 |
| UART | 串口接收 | `Uart_Handler()` | 状态机解析 13 字节帧 |
| GPIO 3 | 键盘按下 | `key_scan_handler()` | 4×4 矩阵键盘扫描 + 去抖 |
| GPIO 0 | 开关拨动 | `sw_isr()` | 读取波形选择 |
| GPIO 2 | 按键按下 | `mode_btn_isr()` | 切换同步/独立模式 |

### ISR 时序预算（100 kHz = 10µs 周期）

| 阶段 | 耗时 |
|------|------|
| SPI 两帧传输 | ~2.6µs |
| 帧间延时 | ~0.25µs |
| DDS 计算 | ~0.3µs |
| 中断开销 | ~0.2µs |
| **合计** | **< 10µs** |

> ⚠️ 必须使用 `-O1` 编译。`-O0` 下软件乘除 `__mulsi3/__divsi3` 会使 DDS 计算膨胀至 ~30µs，超出周期。

## 构建

在 Vitis IDE 中打开项目并构建，优化级别 `-O1`。通过 `UserConfig.cmake` 配置源文件列表：

```cmake
set(USER_COMPILE_OPTIMIZATION_LEVEL "-O1")
```

## 文件

| 文件 | 说明 |
|------|------|
| `main.c` | 入口 + ISR（中断分发、定时器 DAC 输出、UART 协议处理） |
| `User/dds.c` / `dds.h` | DDS 核心（相位累加 + 波形生成） |
| `User/dac.c` / `dac.h` | TLV5618 DAC SPI 驱动 |
| `User/uart.c` / `uart.h` | UART 协议解析 & 参数管理 |
| `User/kb_display.c` / `kb_display.h` | 键盘扫描 + 数码管显示 + 开关/按键中断 |
| `User/peripheral_init.c` / `peripheral_init.h` | SPI/Timer/Interrupt 初始化 |
| `UserConfig.cmake` | CMake 配置（源文件列表、优化级别） |
