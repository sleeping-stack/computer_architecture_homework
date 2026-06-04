#ifndef UART_H
#define UART_H

#include "xil_types.h"       // 引入 Xil_In32 和 Xil_Out32

// DDS 控制字节位定义
#define CTRL_MODE_SYNC      0x80  // bit[7]: 0=双通道独立, 1=双通道同步
#define CTRL_CH_MASK        0x03  // bit[1:0] 通道掩码
#define CTRL_CH_CH1         0x00  // 选中 CH1
#define CTRL_CH_CH2         0x01  // 选中 CH2
#define CTRL_CH_BOTH        0x02  // 双通道同时(同步模式)

// DDS参数结构体
typedef struct {
    uint8_t  ctrl;        // 控制字节: bit[7]=模式, bit[1:0]=通道
    uint8_t  wave_type;   // 0:正弦, 1:方波, 2:三角, 3:锯齿, 4:任意
    uint32_t frequency;   // 频率 (Hz)
    uint32_t amplitude;   // 幅度 (mV)
} DDS_Params_t;

volatile extern DDS_Params_t g_dds_params[2];  // CH1, CH2 接收参数存储
volatile extern uint8_t g_uart_rx_flag; // 接收完成标志位

#endif