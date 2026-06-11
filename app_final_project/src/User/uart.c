#include "uart.h"
#include "xparameters.h"
#include "xil_io.h"
#include "xuartlite_l.h"

volatile DDS_Params_t g_dds_params[2];  // CH1, CH2 接收参数存储
volatile uint8_t g_uart_rx_pending = 0; // 待处理帧计数
volatile uint8_t g_uart_rx_flags = 0;   // 待处理通道标记: bit0=CH1, bit1=CH2
volatile uint8_t g_last_ch = 0;         // 最后活动通道

void uart_send_ack(void) {
  static const char ack[] = "OK\r\n";
  for (int i = 0; i < 4; i++) {
    while (Xil_In32(XPAR_AXI_UARTLITE_0_BASEADDR + XUL_STATUS_REG_OFFSET) &
           XUL_SR_TX_FIFO_FULL) {
    }
    Xil_Out32(XPAR_AXI_UARTLITE_0_BASEADDR + XUL_TX_FIFO_OFFSET, ack[i]);
  }
}