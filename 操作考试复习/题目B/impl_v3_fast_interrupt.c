/*
 * 题目B — 快速中断版本 (v3)
 * 合并原题目5+9: 8位数码管滚动显示"3456"
 * Timer快速中断 + GPIO_2快速中断
 */

#include "mb_interface.h"
#include "xgpio_l.h"
#include "xil_io.h"
#include "xintc_l.h"
#include "xparameters.h"
#include "xtmrctr_l.h"

#define GPIO1_BASE XPAR_AXI_GPIO_1_BASEADDR
#define GPIO2_BASE XPAR_AXI_GPIO_2_BASEADDR
#define TIMER_BASE XPAR_AXI_TIMER_0_BASEADDR
#define INTC_BASE  XPAR_XINTC_0_BASEADDR

#define TIMER_INTC_MASK (1 << XPAR_FABRIC_AXI_TIMER_0_INTR)
#define GPIO2_INTC_MASK (1 << XPAR_FABRIC_AXI_GPIO_2_INTR)

#define BTN_C (1 << 2)
#define BTN_L (1 << 1)
#define BTN_R (1 << 3)

#define SEG_BLANK 0xFF
#define SEG_3     0xB0
#define SEG_4     0x99
#define SEG_5     0x92
#define SEG_6     0x82

static const u8 SEQ[4] = {SEG_3, SEG_4, SEG_5, SEG_6};

volatile u8 g_offset = 0;
volatile u8 g_running = 0;
volatile u8 g_direction = 0;
volatile u8 g_seg_buffer[8];
volatile u8 g_update_display = 0;

static void update_seg(void) {
  for (int i = 0; i < 8; i++) g_seg_buffer[i] = SEG_BLANK;
  if (g_running)
    for (int i = 0; i < 4; i++)
      g_seg_buffer[(7 - g_offset - i + 8) % 8] = SEQ[i];
}

static void DelayMs(u32 ms) {
  for (u32 i = 0; i < ms * 150; i++)
    for (volatile int j = 0; j < 10; j++);
}

void TimerFastHandler(void) __attribute__((fast_interrupt));
void TimerFastHandler(void) {
  if (g_running) {
    if (g_direction == 0) g_offset = (g_offset + 1) % 8;
    else                  g_offset = (g_offset + 7) % 8;
    g_update_display = 1;
  }
  u32 tcsr = Xil_In32(TIMER_BASE + XTC_TCSR_OFFSET);
  Xil_Out32(TIMER_BASE + XTC_TCSR_OFFSET, tcsr | XTC_CSR_INT_OCCURED_MASK);
}

void ButtonFastHandler(void) __attribute__((fast_interrupt));
void ButtonFastHandler(void) {
  u8 pressed = (u8)((Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F) ^ 0x1F);
  if (pressed & BTN_C) { g_running = !g_running; g_update_display = 1; }
  else if (pressed & BTN_L) g_direction = 1;
  else if (pressed & BTN_R) g_direction = 0;
  Xil_Out32(GPIO2_BASE + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
}

int main() {
  Xil_Out32(GPIO1_BASE + XGPIO_TRI_OFFSET, 0x00);
  Xil_Out32(GPIO1_BASE + XGPIO_TRI2_OFFSET, 0x00);
  Xil_Out32(GPIO2_BASE + XGPIO_TRI_OFFSET, 0xFF);
  for (int i = 0; i < 8; i++) g_seg_buffer[i] = SEG_BLANK;

  Xil_Out32(GPIO2_BASE + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
  Xil_Out32(GPIO2_BASE + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
  Xil_Out32(GPIO2_BASE + XGPIO_GIE_OFFSET, XGPIO_GIE_GINTR_ENABLE_MASK);

  u32 tcsr0 = Xil_In32(TIMER_BASE + XTC_TCSR_OFFSET);
  tcsr0 &= ~XTC_CSR_ENABLE_TMR_MASK;
  Xil_Out32(TIMER_BASE + XTC_TCSR_OFFSET, tcsr0);
  Xil_Out32(TIMER_BASE + XTC_TLR_OFFSET, 99999998);
  Xil_Out32(TIMER_BASE + XTC_TCSR_OFFSET, tcsr0 | XTC_CSR_LOAD_MASK);
  u32 tcsr = Xil_In32(TIMER_BASE + XTC_TCSR_OFFSET);
  Xil_Out32(TIMER_BASE + XTC_TCSR_OFFSET,
            tcsr | XTC_CSR_ENABLE_TMR_MASK | XTC_CSR_AUTO_RELOAD_MASK |
            XTC_CSR_ENABLE_INT_MASK | XTC_CSR_DOWN_COUNT_MASK |
            XTC_CSR_INT_OCCURED_MASK);

  u32 mask = TIMER_INTC_MASK | GPIO2_INTC_MASK;
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, 0xFFFFFFFF);
  Xil_Out32(INTC_BASE + XIN_IER_OFFSET, mask);
  Xil_Out32(INTC_BASE + XIN_MER_OFFSET,
            XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);
  Xil_Out32(INTC_BASE + XIN_IMR_OFFSET, mask);
  Xil_Out32(INTC_BASE + XIN_IVAR_OFFSET + XPAR_FABRIC_AXI_TIMER_0_INTR * 4,
            (u32)TimerFastHandler);
  Xil_Out32(INTC_BASE + XIN_IVAR_OFFSET + XPAR_FABRIC_AXI_GPIO_2_INTR * 4,
            (u32)ButtonFastHandler);
  microblaze_enable_interrupts();

  while (1) {
    if (g_update_display) { g_update_display = 0; update_seg(); }
    for (int digit = 0; digit < 8; digit++) {
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, (1 << digit));
      Xil_Out32(GPIO1_BASE + XGPIO_DATA_OFFSET, g_seg_buffer[digit]);
      DelayMs(2);
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, 0x00);
    }
  }
  return 0;
}
