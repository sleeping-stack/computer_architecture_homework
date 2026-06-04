#include "User/peripheral_init.h"
#include "User/uart.h"
#include "User/dac.h"
#include "User/dds.h"
#include "xil_exception.h"
#include "xil_io.h"
#include "xparameters.h"
#include "xtmrctr_l.h"

// 中断服务函数
void MyISR() __attribute__((interrupt_handler));
void Tim_Handler(void);
void Uart_Handler(void);

int main() {
  spi_init();
  dds_init();

  // 定时器: 100MHz / 250kHz - 2 = 398
  timer_init(XPAR_AXI_TIMER_0_CLOCK_FREQUENCY / F_SAMPLE - 2);
  interrupt_init();

  microblaze_enable_interrupts();

  while (1) {
    if (g_uart_rx_flag == 1) {
      // 参数更新: 重新计算频率字
      uint8_t ctrl0 = g_dds_params[0].ctrl;
      uint8_t mode  = ctrl0 & CTRL_MODE_SYNC;

      if (mode == 0) {
        // 独立模式: 仅更新上次选中的通道
        for (int ch = 0; ch < 2; ch++) {
          if (g_dds_params[ch].ctrl != 0 || ch == 0) {
            dds_set_fword(ch, g_dds_params[ch].frequency);
          }
        }
      } else {
        // 同步模式: 两通道同频
        dds_set_fword(0, g_dds_params[0].frequency);
        dds_set_fword(1, g_dds_params[1].frequency);
      }

      g_uart_rx_flag = 0;  // 清除标志位，允许接收下一帧
    }
  }

  return 0;
}

void MyISR() {
  int inst_status =
      Xil_In32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_ISR_OFFSET); // 读取ISR

  // 定时器中断
  if ((inst_status & (1 << XPAR_FABRIC_AXI_TIMER_0_INTR))) {
    Tim_Handler();
  }
  // 串口中断
  if ((inst_status & (1 << XPAR_FABRIC_AXI_UARTLITE_0_INTR))) {
    Uart_Handler();
  }

  // 写IAR,清除标志位
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IAR_OFFSET, inst_status);
}

void Tim_Handler(void) {
  // DDS 相位累加 + 波形生成 → 更新 volt_ch1/volt_ch2
  dds_update();

  // 同步输出双通道到 TLV5618
  dac_write_both(volt_ch1, volt_ch2);

  // 清除定时器中断标志位
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
            Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) |
                XTC_CSR_INT_OCCURED_MASK);
}

void Uart_Handler(void) {
  static uint8_t state = 0;
  static uint8_t rx_buffer[13];  // 13 字节协议帧 
  static uint8_t data_idx = 0;
  static uint8_t checksum = 0;

  // 检查状态寄存器的 Bit 0
  // (XUL_SR_RX_FIFO_VALID_DATA)，若为1则表示FIFO内有数据
  while (Xil_In32(XPAR_AXI_UARTLITE_0_BASEADDR + XUL_STATUS_REG_OFFSET) &
         XUL_SR_RX_FIFO_VALID_DATA) {

    // 从接收 FIFO (Offset 0) 读取 1 字节数据
    uint8_t rx_byte =
        (uint8_t)Xil_In32(XPAR_AXI_UARTLITE_0_BASEADDR + XUL_RX_FIFO_OFFSET);

    switch (state) {
    case 0: // 匹配第一帧头 0xAA
      if (rx_byte == 0xAA) {
        checksum = 0xAA;
        state = 1;
      }
      break;

    case 1: // 匹配第二帧头 0x55
      if (rx_byte == 0x55) {
        checksum ^= 0x55;
        data_idx = 2; // 数据体存储起始索引
        state = 2;
      } else {
        state = 0; // 帧头不匹配，重置状态
      }
      break;

    case 2: // 接收数据体 (Ctrl, Type, Freq, Amp) 共 10 字节
      rx_buffer[data_idx] = rx_byte;
      if (data_idx < 12) {  // 前 10 字节参与异或校验 (索引 2~11)
        checksum ^= rx_byte;
      }
      data_idx++;

      if (data_idx == 12) { // 数据体接收完毕，下一字节是校验
        state = 3;
      }
      break;

    case 3: // 校验及大端模式组装
      if (rx_byte == checksum) {
        // 解析控制字节
        uint8_t ctrl      = rx_buffer[2];
        uint8_t mode      = ctrl & CTRL_MODE_SYNC;  // bit[7]: 0=独立, 1=同步
        uint8_t ch        = ctrl & CTRL_CH_MASK;     // bit[1:0]: 通道

        // 解析波形参数 
        uint8_t  wave_type = rx_buffer[3];

        // 4字节高位在前拼接 频率 (Hz)
        uint32_t frequency =
            ((uint32_t)rx_buffer[4] << 24) | ((uint32_t)rx_buffer[5] << 16) |
            ((uint32_t)rx_buffer[6] << 8)  | ((uint32_t)rx_buffer[7]);

        // 4字节高位在前拼接 幅度 (mV)
        uint32_t amplitude =
            ((uint32_t)rx_buffer[8] << 24)  | ((uint32_t)rx_buffer[9] << 16) |
            ((uint32_t)rx_buffer[10] << 8)  | ((uint32_t)rx_buffer[11]);

        if (mode == 0) {
          // 独立模式：仅写入选中通道
          if (ch <= CTRL_CH_CH2) {
            g_dds_params[ch].ctrl      = ctrl;
            g_dds_params[ch].wave_type = wave_type;
            g_dds_params[ch].frequency = frequency;
            g_dds_params[ch].amplitude = amplitude;
          }
        } else {
          // 同步模式：同时写入两个通道
          g_dds_params[0].ctrl      = ctrl;
          g_dds_params[0].wave_type = wave_type;
          g_dds_params[0].frequency = frequency;
          g_dds_params[0].amplitude = amplitude;
          g_dds_params[1].ctrl      = ctrl;
          g_dds_params[1].wave_type = wave_type;
          g_dds_params[1].frequency = frequency;
          g_dds_params[1].amplitude = amplitude;
        }

        g_uart_rx_flag = 1; // 成功收到完整且合法的数据后将标志位置 1
      }
      state = 0; // 复位状态机
      break;

    default:
      state = 0;
      break;
    }
  }
}