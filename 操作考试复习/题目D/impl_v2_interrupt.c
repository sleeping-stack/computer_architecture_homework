/*
 * 题目D — 普通中断版本 (v2)
 * LED移位: GPIO_2普通中断处理C/R按键
 */

#include "mb_interface.h"
#include "xgpio_l.h"
#include "xil_io.h"
#include "xintc_l.h"
#include "xparameters.h"

#define GPIO0_BASE XPAR_AXI_GPIO_0_BASEADDR
#define GPIO2_BASE XPAR_AXI_GPIO_2_BASEADDR
#define INTC_BASE  XPAR_XINTC_0_BASEADDR
#define GPIO2_INTC_MASK (1 << XPAR_FABRIC_AXI_GPIO_2_INTR)

#define BTN_C (1 << 2)
#define BTN_R (1 << 3)

volatile u8 g_led_val = 0;

void ButtonISR(void) __attribute__((interrupt_handler));
void ButtonISR(void) {
  u8 pressed = (u8)((Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F) ^ 0x1F);
  if (pressed & BTN_C)
    g_led_val = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
  else if (pressed & BTN_R)
    g_led_val = (g_led_val >> 1) | ((g_led_val & 0x01) << 7);
  Xil_Out32(GPIO2_BASE + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, GPIO2_INTC_MASK);
}

int main() {
  Xil_Out32(GPIO0_BASE + XGPIO_TRI_OFFSET, 0xFF);
  Xil_Out32(GPIO0_BASE + XGPIO_TRI2_OFFSET, 0x00);
  Xil_Out32(GPIO2_BASE + XGPIO_TRI_OFFSET, 0xFF);

  Xil_Out32(GPIO2_BASE + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
  Xil_Out32(GPIO2_BASE + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
  Xil_Out32(GPIO2_BASE + XGPIO_GIE_OFFSET, XGPIO_GIE_GINTR_ENABLE_MASK);
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, 0xFFFFFFFF);
  Xil_Out32(INTC_BASE + XIN_IER_OFFSET, GPIO2_INTC_MASK);
  Xil_Out32(INTC_BASE + XIN_MER_OFFSET,
            XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);
  microblaze_enable_interrupts();

  while (1) {
    Xil_Out32(GPIO0_BASE + XGPIO_DATA2_OFFSET, g_led_val);
    for (volatile int i = 0; i < 50000; i++) __asm__("nop");
  }
  return 0;
}
