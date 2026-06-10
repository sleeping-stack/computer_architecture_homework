#include "uart.h"

volatile DDS_Params_t g_dds_params[2];  // CH1, CH2 接收参数存储
volatile uint8_t g_uart_rx_flag = 0;    // 接收完成标志位
volatile uint8_t g_last_ch = 0;         // 最后一次 UART 选中的通道