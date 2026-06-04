#ifndef DDS_H
#define DDS_H

#include "xil_types.h"

// ============================================================
// DDS 参数
// ============================================================
#define F_SAMPLE        250000UL   // 采样率 250 kHz
#define TABLE_SIZE      1024       // 波形表点数
#define PHASE_SHIFT     22         // 32-10=22, 高10位索引1024点表

// ============================================================
// 波形类型 (与 UART 协议一致)
// ============================================================
#define DDS_WAVE_SINE       0
#define DDS_WAVE_SQUARE     1
#define DDS_WAVE_TRIANGLE   2
#define DDS_WAVE_SAWTOOTH   3
#define DDS_WAVE_ARBITRARY  4

// ============================================================
// DAC 输出变量 (dds_update 写入, Tim_Handler 读取)
// ============================================================
extern volatile uint16_t volt_ch1;
extern volatile uint16_t volt_ch2;

// ============================================================
// 函数声明
// ============================================================
void dds_init(void);
void dds_update(void);
void dds_set_fword(uint8_t ch, uint32_t freq_hz);

#endif
