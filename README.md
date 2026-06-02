# 项目介绍

华科电信微机原理实验，基于 MicroBlaze 软核处理器的 FPGA 实验。

## 硬件平台

- **开发板**：小梅哥AC850
- **处理器**：zynq7020

## 并行接口实验

| 功能 | 说明 |
|------|------|
| 开关 → LED | 8 位拨码开关状态实时同步到 8 个 LED |
| 按键 → 数码管 | 5 个按键分别切换显示字符：C、U、L、D、R |

## 软件环境

- **AMD Vitis** (2024.2+)
- **MicroBlaze Standalone BSP**

## 项目结构

```
├── app_parallel_IO/src/     # 应用源码
│   ├── main.c               # 主程序
│   ├── CMakeLists.txt       # 构建配置
│   └── lscript.ld           # 链接脚本
└── .gitignore
```
