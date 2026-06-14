# MicroBlaze Soft-Core Processor Experiments

华科电信微机原理实验，基于 MicroBlaze 软核处理器的 FPGA 实验项目。

## 硬件平台

- **开发板**：小梅哥 AC850
- **主控芯片**：Xilinx Zynq-7020 (XC7Z020)
- **软核 CPU**：MicroBlaze v11.0（配置在 Zynq PL 端）
- **存储器**：LMB BRAM（~32KB，地址 `0x50`–`0x7FB0`）
- **系统时钟**：100 MHz

## 实验项目

| 目录 | 实验 | 关键技术 | 独立文档 |
|------|------|---------|---------|
| [`app_parallel_IO/`](app_parallel_IO/src/README.md) | 并行接口实验 | GPIO 轮询/普通中断/快速中断（3 版本） | [README](app_parallel_IO/src/README.md) |
| [`app_DAC/`](app_DAC/src/README.md) | DAC 锯齿波实验 | 定时器中断 + SPI 驱动 TLV5618 | [README](app_DAC/src/README.md) |
| [`app_final_project/`](app_final_project/src/README.md) | DDS 双通道信号发生器 | DDS + SPI DAC + UART 协议 + 键盘显示 | [README](app_final_project/src/README.md) |
| [`app_ADC/`](app_ADC/src/README.md) | ADC 采样实验 | SPI ADC + 定时器触发 + 串口输出 | [README](app_ADC/src/README.md) |
| [`操作考试复习/`](操作考试复习/README.md) | 操作考试复习资料 | 并行IO实验复习（任务0–3 + 题目A–E），每题含轮询/普通中断/快速中断三版本 | [README](操作考试复习/README.md) |

## 软件环境

- **AMD Vitis** 2024.2+
- **编译器**：`mb-gcc` 13.3.0，MicroBlaze Standalone BSP
- **构建系统**：CMake + Ninja（Vitis 自动管理）
- **Python 上位机**：[`scripts/dds_host.py`](scripts/dds_host.py)（需 `pyserial`）

### Python 上位机用法

```bash
# 命令行模式
python scripts/dds_host.py --port COM3 --ch1 sine 1000 2000        # CH1: 正弦 1kHz 2V
python scripts/dds_host.py --port COM3 --ch2 square 5000 1500       # CH2: 方波 5kHz 1.5V
python scripts/dds_host.py --port COM3 --sync triangle 3000 2048    # 同步: 三角 3kHz 2.048V

# 交互模式
python scripts/dds_host.py --port COM3

# 列出可用串口
python scripts/dds_host.py --list
```

参数范围：波形 `sine|square|triangle|sawtooth`，频率 0–50000 Hz，幅度 0–4095 mV。

## 项目结构

```
├── README.md                          # 项目总览（本文件）
├── .gitignore                         # 白名单模式，仅追踪源码
├── platform_new/                      # BSP（Vitis 生成，不手动修改）
├── scripts/
│   └── dds_host.py                    # Python 上位机脚本
├── app_parallel_IO/src/               # 并行接口实验
│   ├── README.md                      # 实验文档
│   ├── impl_v1.c / impl_v2.c / impl_v3.c  # 轮询/普通中断/快速中断 三版本
│   ├── example_1.c                    # 教学视频配套测试
│   └── UserConfig.cmake               # CMake 配置（IMPL_VERSION 选择版本）
├── app_DAC/src/                       # DAC 实验
│   ├── README.md                      # 实验文档
│   ├── main.c                         # 定时器 ISR + SPI DAC 锯齿波输出
│   └── UserConfig.cmake
├── app_final_project/src/             # DDS 信号发生器
│   ├── README.md                      # 实验文档
│   ├── main.c                         # 入口 + ISR（定时器 DAC 输出 + UART 协议处理）
│   ├── User/
│   │   ├── dds.c/h                    # DDS 核心（相位累加 + 波形生成）
│   │   ├── dac.c/h                    # TLV5618 DAC SPI 驱动
│   │   ├── uart.c/h                   # UART 协议 & DDS 参数
│   │   ├── kb_display.c/h             # 键盘扫描 + 数码管 + 开关/按键中断
│   │   └── peripheral_init.c/h        # SPI/Timer/Interrupt 初始化
│   └── UserConfig.cmake
├── app_ADC/src/                       # ADC 实验
│   ├── README.md                      # 实验文档
│   ├── main.c                         # 快速中断 ISR + SPI ADC 采样
│   └── UserConfig.cmake
└── 操作考试复习/                       # 操作考试复习资料
    ├── README.md                      # 复习总览（硬件映射、段码表、题目速览）
    ├── 题目.md                        # 全部实验题目描述
    ├── EDA-V4扩展板原理图.pdf          # 扩展板硬件原理图
    ├── 共阳极数码管.png                # 数码管参考图
    ├── 任务0/–任务3/                   # 实验任务0–3（基础并行IO实验）
    └── 题目A/–题目E/                   # 题目A–E（综合应用题）
```

## 构建

通过 Vitis IDE 打开项目并构建。CMake 由 Vitis 管理，不要手动运行 `cmake`。

各 app 的编译配置（源文件、优化级别）在各自的 `UserConfig.cmake` 中设置。

## 代码风格

- 不依赖 Vitis 驱动 API（`XGpio`/`XUartLite` 等），使用底层寄存器宏 `Xil_In32()`/`Xil_Out32()`
- ISR 使用 `__attribute__((interrupt_handler))` 或 `__attribute__((fast_interrupt))` 声明
- 字符串输出仅使用 `xil_printf()`（轻量级 printf）
- include 路径：`src/` 下用 `#include "User/xxx.h"`，`User/` 下用 `#include "xxx.h"`

## .gitignore 策略

使用白名单模式，仅追踪源码文件（`.c`、`.h`、`UserConfig.cmake`、`*.md`），忽略 Vitis 生成的构建产物和中间文件。
