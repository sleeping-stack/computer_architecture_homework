# 项目介绍

华科电信微机原理实验，基于 MicroBlaze 软核处理器的 FPGA 实验。

## 硬件平台

- **开发板**：小梅哥AC850
- **处理器**：zynq7020 (XC7Z020)
- **软核 CPU**：MicroBlaze (配置在 Zynq PL 端)

## 并行接口实验 (app_parallel_IO)

| 功能 | 说明 |
|------|------|
| 开关 → LED | 8 位拨码开关状态实时同步到 8 个 LED |
| 按键 → 数码管 | 5 个按键分别切换显示字符：C、U、L、D、R |

## DAC 实验 (app_DAC)

定时器中断驱动 SPI 输出，控制 TLV5618 DAC 生成锯齿波。

| 功能 | 说明 |
|------|------|
| DAC 输出 | 1ms 定时器中断，SPI 发送 12-bit 数据到 TLV5618 |
| 波形 | 锯齿波，幅度 0–4095，周期可调 |

## DDS 应用 (app_final_project)

双通道 DDS 信号发生器，通过串口协议控制波形参数。

| 功能 | 说明 |
|------|------|
| 波形类型 | 正弦（1024点LUT）、方波、三角、锯齿 |
| 采样率 | 250 kHz（定时器 100MHz / 400） |
| 频率范围 | 0 – ~100 kHz，分辨率 ~0.000058 Hz（32-bit 相位累加器） |
| 幅度范围 | 0 – 4095 mV（V_ref=2.048V，×2 增益） |
| 控制方式 | UART 串口协议，13 字节帧 |
| DAC | TLV5618，双通道同步更新，SPI 16-bit 帧 |

### 串口协议帧格式（13 字节）

```
| 0xAA | 0x55 | ctrl | type | freq[4] | amp[4] | checksum |
```

- `ctrl`: bit[7]=同步模式, bit[1:0]=通道选择
- `type`: 0=正弦, 1=方波, 2=三角, 3=锯齿
- `freq`/`amp`: 大端序 uint32
- `checksum`: 累进 XOR（帧头2 + 数据体10 字节）

## 软件环境

- **AMD Vitis** (2024.2+)
- **MicroBlaze Standalone BSP**

## 多版本实现架构 (app_parallel_IO)

项目采用多版本实现架构，通过 `UserConfig.cmake` 中的 `IMPL_VERSION` 变量选择编译版本：

| 文件 | 说明 |
|------|------|
| `example_1.c` | 教学视频配套测试文件，通过串口交互测试各外设 |
| `impl_v1.c` | **轮询版本** — 使用 `Xil_In8`/`Xil_Out8` 直接寄存器访问，主循环非阻塞轮询按键状态 |
| `impl_v2.c` | **普通中断版本** — 使用 Vitis 驱动 API (`XGpio`, `XIntc`)，`XIntc_Connect` 注册普通 ISR |
| `impl_v3.c` | **快速中断版本** — 使用 `XIntc_ConnectFastHandler`|

### 版本选择

在 [UserConfig.cmake](app_parallel_IO/src/UserConfig.cmake) 中设置 `IMPL_VERSION`：

```cmake
# 选择实现版本:
#   1 — 轮询版本 (impl_v1.c)
#   2 — 普通中断版本 (impl_v2.c)
#   3 — 快速中断版本 (impl_v3.c)
set(IMPL_VERSION 3)

set(USER_COMPILE_SOURCES "impl_v${IMPL_VERSION}.c")
```

## 项目结构

```
├── README.md
├── .gitignore
├── app_parallel_IO/src/          # 并行接口实验
│   ├── example_1.c               # 教学视频配套测试文件
│   ├── impl_v1.c                 # 轮询版本
│   ├── impl_v2.c                 # 普通中断版本
│   ├── impl_v3.c                 # 快速中断版本
│   └── UserConfig.cmake          # cmake配置（版本选择、编译选项）
├── app_DAC/src/                  # DAC 实验
│   ├── main.c                    # SPI DAC 锯齿波输出
│   └── UserConfig.cmake
└── app_final_project/src/        # DDS 应用
    ├── main.c                    # 入口 + ISR（定时器DAC输出 + UART协议解析）
    ├── UserConfig.cmake
    └── User/                     # 外设驱动模块
        ├── peripheral_init.c/h   # SPI/Timer/Interrupt 初始化
        ├── uart.c/h              # UART 协议 & DDS 参数
        ├── dac.c/h               # TLV5618 DAC 驱动
        └── dds.c/h               # DDS 核心（相位累加 + 波形生成）
```
