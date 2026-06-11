#ifndef DAC_H
#define DAC_H

#include "xil_types.h"

// ============================================================
// TLV5618 控制位定义 (D15–D12)
// 16 位 SPI 帧: [R1 SPD PWR R0] [D11 ... D0]
// ============================================================

// 单控制位
#define DAC_R1              0x8000  // D15: 寄存器选择高位
#define DAC_SPD             0x4000  // D14: 速度控制 (1=快速, 0=慢速)
#define DAC_PWR             0x2000  // D13: 电源控制 (1=掉电, 0=正常工作)
#define DAC_R0              0x1000  // D12: 寄存器选择低位

// R1R0 通道选择组合
#define DAC_CTRL_DACB       0x0000  // R1=0,R0=0: 写 DAC B + 缓冲区
#define DAC_CTRL_BUF        0x1000  // R1=0,R0=1: 只写缓冲区(预装载)
#define DAC_CTRL_DACA       0x8000  // R1=1,R0=0: 写 DAC A + 更新 B (从缓冲区)

// 组合控制字 (快速模式 + 正常工作) — 单通道独立输出
#define DAC_DACA_FAST       (DAC_CTRL_DACA | DAC_SPD)  // 0xC000
#define DAC_DACB_FAST       (DAC_CTRL_DACB | DAC_SPD)  // 0x4000
#define DAC_BUF_FAST        (DAC_CTRL_BUF  | DAC_SPD)  // 0x5000

// 组合控制字 (慢速模式 + 正常工作) — 双通道同步输出 (数据手册要求 SPD=0)
#define DAC_BUF_SLOW        (DAC_CTRL_BUF)              // 0x1000: Step 1 预装载 B→BUFFER
#define DAC_DACA_SYNC_SLOW  (DAC_CTRL_DACA)             // 0x8000: Step 2 写 A + BUFFER→B 同步

// 数据掩码
#define DAC_DATA_MASK       0x0FFF  // 12 位 DAC 数据

// ============================================================
// 函数声明
// ============================================================

// 写 DAC A (快速模式)
void dac_write_A(uint16_t data);

// 写 DAC B (快速模式)
void dac_write_B(uint16_t data);

// 同步更新双通道: 快速模式 (SPD=1), 先预装 B→BUFFER 再写 A 触发同步
void dac_write_both(uint16_t data_a, uint16_t data_b);

#endif
