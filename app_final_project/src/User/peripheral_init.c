#include "peripheral_init.h"

void spi_init(void) {
  // DAC SPI1 配置
  // 1. 软件复位
  Xil_Out32(XPAR_AXI_QUAD_SPI_1_BASEADDR + XSP_SRR_OFFSET, XSP_SRR_RESET_MASK);
  // 等待复位完成 (SRR 自清除, 回读确认)
  volatile int timeout = 100000;
  while (Xil_In32(XPAR_AXI_QUAD_SPI_1_BASEADDR + XSP_SRR_OFFSET) && --timeout)
    ;

  // 2. 复位 TX/RX FIFO (清除热启动残留)
  Xil_Out32(XPAR_AXI_QUAD_SPI_1_BASEADDR + XSP_CR_OFFSET,
            XSP_CR_TXFIFO_RESET_MASK | XSP_CR_RXFIFO_RESET_MASK);

  // 3. SPI 配置: 主设备, CPOL=1, CPHA=0, 自动片选, 不抑制传输
  //    自动片选模式下硬件自动在传输时拉低 CS、空闲时拉高 CS
  Xil_Out32(XPAR_AXI_QUAD_SPI_1_BASEADDR + XSP_CR_OFFSET,
            XSP_CR_ENABLE_MASK | XSP_CR_MASTER_MODE_MASK |
                XSP_CR_CLK_POLARITY_MASK);

  // 4. 配置 SSR 寄存器: 需要选择的置 0, 其他的置 1, 只能选择最多 1 个
  Xil_Out32(XPAR_AXI_QUAD_SPI_1_BASEADDR + XSP_SSR_OFFSET, 0xfffe);
}

void timer_init(uint32_t load_value) {
  // 定时器配置
  int status;
  status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET);
  status = status & (~XTC_CSR_ENABLE_TMR_MASK);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET, status);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET, load_value);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
            status | XTC_CSR_LOAD_MASK);
  // 配置计时器各个模式：使能装载，开启中断，开启计时，向下计数，自动重装载，清除中断标志位
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
            status | XTC_CSR_INT_OCCURED_MASK | XTC_CSR_AUTO_RELOAD_MASK |
                XTC_CSR_DOWN_COUNT_MASK | XTC_CSR_ENABLE_INT_MASK |
                XTC_CSR_ENABLE_TMR_MASK);
}

void uart_init(void) {
  // 串口初始化: 先关中断 → 复位 FIFO → 再开中断 (解决热启动残留问题)
  Xil_Out32(XPAR_AXI_UARTLITE_0_BASEADDR + XUL_CONTROL_REG_OFFSET, 0);
  Xil_Out32(XPAR_AXI_UARTLITE_0_BASEADDR + XUL_CONTROL_REG_OFFSET,
            XUL_CR_FIFO_RX_RESET | XUL_CR_FIFO_TX_RESET);
  Xil_Out32(XPAR_AXI_UARTLITE_0_BASEADDR + XUL_CONTROL_REG_OFFSET,
            XUL_CR_ENABLE_INTR);
}

void interrupt_init(void) {
  // 中断配置
  // 1. 设置所有中断为普通中断模式
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IMR_OFFSET, 0x00000000);
  // 2. 清除中断控制器中全部挂起的中断标志 (解决调试重启残留状态)
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IAR_OFFSET, 0xffffffff);

  // 3. 同时使能定时器和串口中断 (合并为一次写入, 避免后写覆盖先写)
  uint32_t ier_val = (1 << XPAR_FABRIC_AXI_TIMER_0_INTR) |
                     (1 << XPAR_FABRIC_AXI_UARTLITE_0_INTR);
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IER_OFFSET, ier_val);

  // 4. 使能主中断
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_MER_OFFSET,
            XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);
}