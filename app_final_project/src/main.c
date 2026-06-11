#include "User/dac.h"
#include "User/dds.h"
#include "User/kb_display.h"
#include "User/peripheral_init.h"
#include "User/uart.h"
#include "mb_interface.h"
#include "xil_io.h"
#include "xparameters.h"
#include "xtmrctr_l.h"

// 中断服务函数
void MyISR() __attribute__((interrupt_handler));
void Tim_Handler(void);
void Uart_Handler(void);

int main() {
  dds_init(); // 先初始化 DDS 参数 (数据先于传输就绪)
  kb_display_init(); // 初始化键盘/显示外设 (需在 dds_init 之后, 读取 g_dds_params 默认值)
  spi_init(); // 再初始化 SPI
  uart_init();
  // 定时器: 100MHz / 100kHz - 2
  timer_init(XPAR_AXI_TIMER_0_CLOCK_FREQUENCY / F_SAMPLE - 2);
  interrupt_init();

  microblaze_enable_interrupts();

  while (1) {
    kb_display_poll(); // 处理键盘输入事件

    while (g_uart_rx_pending > 0) {
      DDS_Params_t p0, p1;
      uint8_t flags;

      // 临界区: 关中断拷贝 g_dds_params 和 flags,
      // 清零标记并递减计数 (每帧对应一次 ACK)
      microblaze_disable_interrupts();
      p0 = g_dds_params[0];
      p1 = g_dds_params[1];
      flags = g_uart_rx_flags;
      g_uart_rx_flags = 0;
      g_uart_rx_pending--;
      microblaze_enable_interrupts();

      uint8_t mode = p0.ctrl & CTRL_MODE_SYNC;

      if (mode == 0) {
        // 独立模式: 仅更新标记的通道, 另一通道保持原有参数继续运行
        if (flags & 0x01) {
          dds_set_params(0, p0.frequency, p0.amplitude, p0.wave_type);
          display_sync_from_uart(0); // 同步 LED/数码管
        }
        if (flags & 0x02) {
          dds_set_params(1, p1.frequency, p1.amplitude, p1.wave_type);
          display_sync_from_uart(1); // 同步 LED/数码管
        }
      } else {
        // 同步模式: 两通道同频同幅同波形, 对齐相位
        dds_set_params(0, p0.frequency, p0.amplitude, p0.wave_type);
        dds_set_params(1, p1.frequency, p1.amplitude, p1.wave_type);
        dds_align_phase();
        display_sync_from_uart(0); // 同步模式: 显示跟随 CH1
      }

      uart_send_ack();
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
  // GPIO3 键盘中断
  if ((inst_status & (1 << XPAR_FABRIC_AXI_GPIO_3_INTR))) {
    key_scan_isr();
  }
  // GPIO0 拨码开关中断
  if ((inst_status & (1 << XPAR_FABRIC_AXI_GPIO_0_INTR))) {
    sw_isr();
  }
  // GPIO2 模式按键中断
  if ((inst_status & (1 << XPAR_FABRIC_AXI_GPIO_2_INTR))) {
    mode_btn_isr();
  }

  // 写IAR,清除标志位
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IAR_OFFSET, inst_status);
}

void Tim_Handler(void) {
  // DDS 相位累加 + 波形生成 → 更新 volt_ch1/volt_ch2
  dds_update();

  // 双通道始终同步输出 (独立/同步模式仅影响参数管理策略)
  dac_write_both(volt_ch1, volt_ch2);

  kb_display_tick(); // 数码管动态扫描

  // 清除定时器中断标志位 (TCSR 中 INT_OCCURED 是 TOW 位, 写回原值即可翻转清除)
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
            Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET));
}

void Uart_Handler(void) {
  static uint8_t state = 0;
  static uint8_t rx_buffer[13]; // 13 字节协议帧
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
      if (data_idx < 12) { // 前 10 字节参与异或校验 (索引 2~11)
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
        uint8_t ctrl = rx_buffer[2];
        uint8_t mode = ctrl & CTRL_MODE_SYNC; // bit[7]: 0=独立, 1=同步
        uint8_t ch = ctrl & CTRL_CH_MASK;     // bit[1:0]: 通道

        // 解析波形参数
        uint8_t wave_type = rx_buffer[3];

        // 4字节高位在前拼接 频率 (Hz)
        uint32_t frequency =
            ((uint32_t)rx_buffer[4] << 24) | ((uint32_t)rx_buffer[5] << 16) |
            ((uint32_t)rx_buffer[6] << 8) | ((uint32_t)rx_buffer[7]);

        // 4字节高位在前拼接 幅度 (mV)
        uint32_t amplitude =
            ((uint32_t)rx_buffer[8] << 24) | ((uint32_t)rx_buffer[9] << 16) |
            ((uint32_t)rx_buffer[10] << 8) | ((uint32_t)rx_buffer[11]);

        if (mode == 0) {
          // 独立模式：仅写入选中通道
          if (ch <= CTRL_CH_CH2) {
            g_dds_params[ch].ctrl = ctrl;
            g_dds_params[ch].wave_type = wave_type;
            g_dds_params[ch].frequency = frequency;
            g_dds_params[ch].amplitude = amplitude;
            g_uart_rx_flags |= (1 << ch); // 标记对应通道待处理
          }
        } else {
          // 同步模式：同时写入两个通道
          g_dds_params[0].ctrl = ctrl;
          g_dds_params[0].wave_type = wave_type;
          g_dds_params[0].frequency = frequency;
          g_dds_params[0].amplitude = amplitude;
          g_dds_params[1].ctrl = ctrl;
          g_dds_params[1].wave_type = wave_type;
          g_dds_params[1].frequency = frequency;
          g_dds_params[1].amplitude = amplitude;
          g_uart_rx_flags |= 0x03; // 标记两通道均待处理
        }

        if (g_uart_rx_pending < 255)
          g_uart_rx_pending++;
      }
      state = 0; // 复位状态机
      break;

    default:
      state = 0;
      break;
    }
  }
}