/*
 * 题目A — 快速中断版本 (v3)
 * 合并原题目2+3+7: Hex显示 + 自动递增 + 手动/自动移位
 *
 * Timer快速中断(1s周期) + GPIO_2快速中断(按键)
 * 快速中断: 配置 IMR + IVAR, ISR 仅清除外设中断
 */

#include "mb_interface.h"
#include "xgpio_l.h"
#include "xil_io.h"
#include "xintc_l.h"
#include "xparameters.h"
#include "xtmrctr_l.h"

#define GPIO0_BASE XPAR_AXI_GPIO_0_BASEADDR
#define GPIO1_BASE XPAR_AXI_GPIO_1_BASEADDR
#define GPIO2_BASE XPAR_AXI_GPIO_2_BASEADDR
#define TIMER_BASE XPAR_AXI_TIMER_0_BASEADDR
#define INTC_BASE  XPAR_XINTC_0_BASEADDR

#define TIMER_INTC_MASK (1 << XPAR_FABRIC_AXI_TIMER_0_INTR)
#define GPIO2_INTC_MASK (1 << XPAR_FABRIC_AXI_GPIO_2_INTR)

#define BTN_U (1 << 0)
#define BTN_L (1 << 1)
#define BTN_C (1 << 2)
#define BTN_R (1 << 3)
#define BTN_D (1 << 4)

#define SEG_BLANK 0xFF

static const u8 SEG_TABLE[16] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8,
    0x80, 0x90, 0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E,
};

volatile u8 g_sw_value = 0;
volatile u8 g_display_pos = 1;
volatile u8 g_inc_mode = 0;
volatile u8 g_shift_mode = 0;
volatile u8 g_seg_buffer[8];
volatile u8 g_update_display = 0;

static void update_seg(void) {
  for (int i = 0; i < 8; i++) g_seg_buffer[i] = SEG_BLANK;
  u8 hi = (g_sw_value >> 4) & 0x0F;
  u8 lo = g_sw_value & 0x0F;
  g_seg_buffer[g_display_pos] = SEG_TABLE[hi];
  g_seg_buffer[(g_display_pos + 7) % 8] = SEG_TABLE[lo];
}

static void DelayMs(u32 ms) {
  for (u32 i = 0; i < ms * 150; i++)
    for (volatile int j = 0; j < 10; j++);
}

// Timer 快速中断
void TimerFastHandler(void) __attribute__((fast_interrupt));
void TimerFastHandler(void) {
  if (g_inc_mode) { g_sw_value++; g_update_display = 1; }
  if (g_shift_mode == 1) {
    g_display_pos = (g_display_pos + 2) % 8; g_update_display = 1;
  } else if (g_shift_mode == 2) {
    g_display_pos = (g_display_pos + 6) % 8; g_update_display = 1;
  }
  u32 tcsr = Xil_In32(TIMER_BASE + XTC_TCSR_OFFSET);
  Xil_Out32(TIMER_BASE + XTC_TCSR_OFFSET, tcsr | XTC_CSR_INT_OCCURED_MASK);
}

// Button 快速中断
void ButtonFastHandler(void) __attribute__((fast_interrupt));
void ButtonFastHandler(void) {
  u32 btn = Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F;
  u8 pressed = (u8)(btn ^ 0x1F);

  if (pressed & BTN_C) {
    g_sw_value = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
    g_inc_mode = 0; g_shift_mode = 0; g_display_pos = 1; g_update_display = 1;
  } else if (pressed & BTN_L) {
    g_inc_mode = !g_inc_mode; g_shift_mode = 0;
    if (g_inc_mode) g_sw_value = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
    g_update_display = 1;
  } else if (pressed & BTN_R) {
    g_inc_mode = 0; g_shift_mode = (g_shift_mode + 1) % 3; g_update_display = 1;
  } else if (pressed & BTN_U) {
    g_inc_mode = 0; g_shift_mode = 0;
    g_display_pos = (g_display_pos + 2) % 8; g_update_display = 1;
  } else if (pressed & BTN_D) {
    g_inc_mode = 0; g_shift_mode = 0;
    g_display_pos = (g_display_pos + 6) % 8; g_update_display = 1;
  }
  Xil_Out32(GPIO2_BASE + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
}

int main() {
  Xil_Out32(GPIO0_BASE + XGPIO_TRI_OFFSET, 0xFF);
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

  u32 intr_mask = TIMER_INTC_MASK | GPIO2_INTC_MASK;
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, 0xFFFFFFFF);
  Xil_Out32(INTC_BASE + XIN_IER_OFFSET, intr_mask);
  Xil_Out32(INTC_BASE + XIN_MER_OFFSET,
            XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);
  // 快速中断 IMR
  Xil_Out32(INTC_BASE + XIN_IMR_OFFSET, intr_mask);
  // 快速中断 IVAR
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
