#include "xgpio_l.h"
#include "xil_exception.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "xintc_l.h"
#include "xparameters.h"
#include "xspi_l.h"
#include "xtmrctr_l.h"

// 定时器
#define RESET_VALUE                                                            \
  (XPAR_AXI_TIMER_0_CLOCK_FREQUENCY / 1000 - 2) // 计时器时间:1ms

// 中断服务函数
void MyISR() __attribute__((interrupt_handler));
void Tim_Handler(void);

// DAC输出
float volt = 0;
int DAC_T = 500; // DAC输出信号周期

int main() {

  // 开关初始化
  Xil_Out8(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET, 0xff);

  // 按键初始化
  Xil_Out8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET, 0X1f);

  // led初始化
  Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);

  // 数码管初始化
  char segcode[8] = {0xc0, 0xf9, 0xa4, 0xb0, 0x99, 0x92, 0x82, 0xf8};
  uint8_t pos = 0x7f;
  Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET, 0x0);
  Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);

  // DAC SPI1 配置
  // 1. 软件复位
  Xil_Out32(XPAR_AXI_QUAD_SPI_1_BASEADDR + XSP_SRR_OFFSET, XSP_SRR_RESET_MASK);
  // 2. SPI配置：主设备，CPOL=1，CPHA=0，自助选择从设备，高位优先
  Xil_Out32(XPAR_AXI_QUAD_SPI_1_BASEADDR + XSP_CR_OFFSET,
            XSP_CR_ENABLE_MASK | XSP_CR_MASTER_MODE_MASK |
                XSP_CR_CLK_POLARITY_MASK);
  // 3. 配置SSR寄存器:需要选择的置0，其他的置1，只能选择最多1个
  Xil_Out32(XPAR_AXI_QUAD_SPI_1_BASEADDR + XSP_SSR_OFFSET, 0xfffe);

  // 定时器配置
  int status;
  status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET);
  status = status & (~XTC_CSR_ENABLE_TMR_MASK);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET, status);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET, RESET_VALUE);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
            status | XTC_CSR_LOAD_MASK);
  // 配置计时器各个模式：使能装载，开启中断，开启计时，向下计数，自动重装载，清除中断标志位
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
            status | XTC_CSR_INT_OCCURED_MASK | XTC_CSR_AUTO_RELOAD_MASK |
                XTC_CSR_DOWN_COUNT_MASK | XTC_CSR_ENABLE_INT_MASK |
                XTC_CSR_ENABLE_TMR_MASK);

  // 中断配置
  // 设置所有中断为普通中断模式
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IMR_OFFSET, 0x00000000);
  // 清除原先中断标志:IAR
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IAR_OFFSET, 0xffff);
  // 使能定时器中断
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IER_OFFSET,
            (1 << XPAR_FABRIC_AXI_TIMER_0_INTR));
  // 使能主中断
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_MER_OFFSET,
            XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);
  microblaze_enable_interrupts();

  while (1) {
  }

  return 0;
}

void MyISR() {
  int inst_status =
      Xil_In32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_ISR_OFFSET); // 读取ISR
  // 定时中断
  if ((inst_status & (1 << XPAR_FABRIC_AXI_TIMER_0_INTR))) {
    Tim_Handler();
  }
  // 写IAR,清除标志位
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IAR_OFFSET, inst_status);
}

void Tim_Handler(void) {
  //：等 TX FIFO 不满
  float DAC_Step = 4096.0 / DAC_T;
  volt += DAC_Step;
  if (volt >= 4096.0f) {
    volt = 0;
  }
  if (!(Xil_In32(XPAR_AXI_QUAD_SPI_1_BASEADDR + XSP_SR_OFFSET) &
        XSP_SR_TX_FULL_MASK)) {
    Xil_Out32(XPAR_AXI_QUAD_SPI_1_BASEADDR + XSP_DTR_OFFSET,
              (int)volt & 0x0fff);
  }
  // 清除标志位
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
            Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET));
}