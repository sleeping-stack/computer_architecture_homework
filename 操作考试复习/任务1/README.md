# 实验任务1 — 开关输入 + 按键触发运算 + LED 输出

## 硬件连接

| 外设 | 通道 | 方向 | 功能 |
|------|------|------|------|
| GPIO_0 Ch1 | 8-bit | 输入 | 拨码开关 (SW) |
| GPIO_0 Ch2 | 8-bit | 输出 | LED 灯 (1=亮) |
| GPIO_2 Ch1 | 5-bit (低5位) | 输入 | 独立按键 (按下=0, 上拉=1) |

### 按键位映射

| 按键 | GPIO_2 Ch1 位 | 功能 |
|------|--------------|------|
| BTNU | bit0 | 无符号加法 (data1 + data2) |
| BTNL | bit1 | (本任务未使用) |
| BTNC | bit2 | 读开关 → 存为 data1 → 显示到 LED |
| BTNR | bit3 | 读开关 → 存为 data2 → 显示到 LED |
| BTND | bit4 | 无符号乘法 (data1 × data2) |

## 功能描述

1. **BTNC**: 读取拨码开关状态作为第一个 8-bit 二进制数据 (data1)，立即显示到 LED
2. **BTNR**: 读取拨码开关状态作为第二个 8-bit 二进制数据 (data2)，立即显示到 LED
3. **BTNU**: 将保存的两组数据做无符号加法，结果显示到 LED（低 8 位）
4. **BTND**: 将保存的两组数据做无符号乘法，结果显示到 LED（低 8 位）

未按下按键时 LED 保持上次运算结果，拨动开关不改变显示（仅在 BTNC/BTNR 按下时读入）。

## 三种实现版本

| 版本 | 文件 | 控制方式 | 关键技术 |
|------|------|----------|----------|
| V1 | `impl_v1_polling.c` | 主循环轮询按键 | 忙等延时去抖 (`delay_ms`)，按下→等待释放→执行 |
| V2 | `impl_v2_interrupt.c` | GPIO_2 普通中断 | `interrupt_handler`，ISR 清除 GPIO ISR + INTC IAR |
| V3 | `impl_v3_fast_interrupt.c` | GPIO_2 快速中断 | `fast_interrupt`，配置 IMR + IVAR，ISR 仅清除 GPIO ISR |

### V1 — 轮询

- 主循环读取按键状态，检测到按下后执行对应操作
- 等待按键释放实现去抖（忙等循环）
- 无中断硬件参与

### V2 — 普通中断

- 按键按下触发 GPIO_2 中断，ISR 中读取按键和开关并执行运算
- ISR 需手动清除两级中断标志：GPIO ISR + INTC IAR
- 主循环仅输出全局 LED 值
- ISR 声明: `__attribute__((interrupt_handler))`

### V3 — 快速中断

- 同 V2 逻辑，但使用快速中断机制
- 初始化时配置 IMR（中断模式寄存器）和 IVAR（向量地址寄存器）
- INTC 硬件自动应答，ISR 内仅需清除 GPIO ISR
- ISR 声明: `__attribute__((fast_interrupt))`

## 寄存器访问

全部使用 32-bit `Xil_In32()` / `Xil_Out32()` 直接操作寄存器，不使用 Vitis 驱动 API。
