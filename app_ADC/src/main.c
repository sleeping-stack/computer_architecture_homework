#include "xgpio_l.h"
#include "xil_exception.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "xintc_l.h"
#include "xparameters.h"
#include "xspi_l.h"
#include "xtmrctr_l.h"

#define BASE_TIME_0_1_S (10000000 - 2)

volatile int flag_sw_changed = 0;
volatile unsigned int current_sw_data = 0;

volatile int flag_adc_ready = 0;
volatile u16 current_adc_data = 0;

// --- 快速中断处理函数声明 ---
void switch_handler(void) __attribute__((fast_interrupt)); // Concat In4
void adc_handler(void) __attribute__((fast_interrupt));    // Concat In1
void timer_handler(void) __attribute__((fast_interrupt));  // Concat In6

void init_peripherals(void);
void setup_system_interrupts(void);

// 主函数
int main() {
  init_peripherals();
  setup_system_interrupts();
  xil_printf("\r\n==============================================\r\n");
  xil_printf("  MicroBlaze ADC Sampling System Initialized  \r\n");
  xil_printf("  Base Clock: 100 MHz                         \r\n");
  xil_printf("  Sampling Interval: 0.1s ~ 12.8s             \r\n");
  xil_printf("  Waiting for Switch input & ADC Data...      \r\n");
  xil_printf("==============================================\r\n\r\n");

  while (1) {
    if (flag_sw_changed == 1) {
      flag_sw_changed = 0;
      unsigned int time_ms = (current_sw_data + 1) * 100;
      unsigned int sec = time_ms / 1000;          // 整数秒
      unsigned int frac = (time_ms % 1000) / 100; // 小数点后一位

      xil_printf("\r\n[SYS INFO] Switch Toggled!\r\n");
      xil_printf("    -> Switch Raw Value: %d\r\n", current_sw_data);
      xil_printf("    -> New ADC Sampling Interval: %d.%d seconds\r\n", sec,
                 frac);
      xil_printf("----------------------------------------------\r\n");
    }

    // 2. 处理 ADC 数据就绪事件
    if (flag_adc_ready == 1) {
      flag_adc_ready = 0;

      unsigned int voltage_mv = (current_adc_data * 3300) / 4095 * 2;
      unsigned int vol_int = voltage_mv / 1000;
      unsigned int vol_frac = voltage_mv % 1000;
      xil_printf("[ADC DATA] Raw: 0x%04X -> Voltage: %d.%03d V\r\n",
                 current_adc_data, vol_int, vol_frac);
    }
  }

  return 0;
}

// 1. GPIO 中断：开关拨动改变频率 (硬件中断通道 In4)
void switch_handler() {
  // 读取 GPIO 数据
  current_sw_data = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);

  // 计算定时器加载值: (sw_data + 1) * 0.1s
  unsigned int timer_load_value = (BASE_TIME_0_1_S + 2) * (current_sw_data + 1) - 2;

  // 更新定时器
  int tcsr0 = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
            tcsr0 & ~XTC_CSR_ENABLE_TMR_MASK);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET, timer_load_value);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
            tcsr0 | XTC_CSR_LOAD_MASK);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
            (tcsr0 & ~XTC_CSR_LOAD_MASK) | XTC_CSR_ENABLE_TMR_MASK |
                XTC_CSR_AUTO_RELOAD_MASK | XTC_CSR_ENABLE_INT_MASK |
                XTC_CSR_DOWN_COUNT_MASK | XTC_CSR_INT_OCCURED_MASK);

  // 清除 GPIO 自身中断标志
  Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IAR_OFFSET, 0x10);
  flag_sw_changed = 1;
}

void timer_handler() {
  int status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET);
  if ((status & XTC_CSR_INT_OCCURED_MASK) == XTC_CSR_INT_OCCURED_MASK) {
    Xil_Out32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_SSR_OFFSET, 0xFFFFFFFE);
    Xil_Out32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_IIER_OFFSET,
              XSP_INTR_TX_EMPTY_MASK);
    Xil_Out32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_DTR_OFFSET, 0x0000);
    int spi_cr = Xil_In32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_CR_OFFSET);
    Xil_Out32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_CR_OFFSET,
              spi_cr & ~XSP_CR_TRANS_INHIBIT_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET, status);
    Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IAR_OFFSET, 0x40);
  }
}

void adc_handler() {
  Xil_Out32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_IIER_OFFSET, 0x0);
  Xil_Out32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_SSR_OFFSET, 0xFFFFFFFF);
  int spi_cr = Xil_In32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_CR_OFFSET);
  Xil_Out32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_CR_OFFSET,
            spi_cr | XSP_CR_TRANS_INHIBIT_MASK);
  current_adc_data = Xil_In16(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_DRR_OFFSET);
  Xil_Out32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_IISR_OFFSET,
            XSP_INTR_TX_EMPTY_MASK);
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IAR_OFFSET, 0x02);
  flag_adc_ready = 1;
}

// 外设初始配置函数
void init_peripherals(void) {
  Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET, 0x0000ffff);
  Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);
  Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,
            XGPIO_IR_CH1_MASK | XGPIO_IR_CH2_MASK);
  Xil_Out32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_SRR_OFFSET, XSP_SRR_RESET_MASK);
  Xil_Out32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_CR_OFFSET,
            XSP_CR_ENABLE_MASK | XSP_CR_MASTER_MODE_MASK |
                XSP_CR_CLK_POLARITY_MASK | XSP_CR_TRANS_INHIBIT_MASK);
  Xil_Out32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_SSR_OFFSET, 0xffffffff);
  Xil_Out32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_IIER_OFFSET, 0x0);
  Xil_Out32(XPAR_AXI_QUAD_SPI_0_BASEADDR + XSP_DGIER_OFFSET,
            XSP_GINTR_ENABLE_MASK);

  int tcsr0 = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
            tcsr0 & ~XTC_CSR_ENABLE_TMR_MASK);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET, BASE_TIME_0_1_S);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
            tcsr0 | XTC_CSR_LOAD_MASK);
  Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
            (tcsr0 & ~XTC_CSR_LOAD_MASK) | XTC_CSR_ENABLE_TMR_MASK |
                XTC_CSR_AUTO_RELOAD_MASK | XTC_CSR_ENABLE_INT_MASK |
                XTC_CSR_DOWN_COUNT_MASK | XTC_CSR_INT_OCCURED_MASK);
}

// 中断系统配置函数
void setup_system_interrupts(void) {
  Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
  Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_GIE_OFFSET,
            XGPIO_GIE_GINTR_ENABLE_MASK);
  unsigned int intr_mask = 0x52;
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IAR_OFFSET, intr_mask);
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IMR_OFFSET, intr_mask);
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IER_OFFSET, intr_mask);

  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IVAR_OFFSET + 0x10,
            (int)switch_handler);
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IVAR_OFFSET + 0x04,
            (int)adc_handler);
  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_IVAR_OFFSET + 0x18,
            (int)timer_handler);

  Xil_Out32(XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR + XIN_MER_OFFSET,
            XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);
  microblaze_enable_interrupts();
}
