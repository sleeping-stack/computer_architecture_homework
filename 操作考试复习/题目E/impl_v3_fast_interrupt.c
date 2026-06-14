/*
 * 题目E — 快速中断版本 (v3)
 * 综合多功能题: Timer快速中断(0.25s) + GPIO_2快速中断
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
#define SEG_0     0xC0
#define SEG_1     0xF9
#define SEG_MINUS 0xBF
static const u8 SEG_NUM[10] = {0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90};

#define MODE_FLOW   0
#define MODE_SHIFT  1
#define MODE_BIN    2
#define MODE_INV    3
#define MODE_SCROLL 4

volatile u8 g_mode = MODE_FLOW;
volatile u8 g_led_val = 0x01;
volatile u8 g_flow_pos = 0;
volatile u8 g_flow_speed = 0;
volatile u8 g_led_shifted = 0;
volatile u8 g_scroll_speed = 0;
volatile u8 g_scroll_offset = 0;
volatile u8 g_seg_buffer[8];
volatile u8 g_tick = 0;

static int signed_val(u8 bits) {
  return ((bits & 0x08) ? -1 : 1) * (int)(bits & 0x07);
}

static void DelayMs(u32 ms) {
  for (u32 i = 0; i < ms * 150; i++)
    for (volatile int j = 0; j < 10; j++);
}

void TimerFastHandler(void) __attribute__((fast_interrupt));
void TimerFastHandler(void) {
  g_tick++;
  if (g_mode == MODE_FLOW) {
    u32 div = (g_flow_speed == 2) ? 1 : (g_flow_speed == 1) ? 2 : 4;
    if (g_tick % div == 0) {
      g_flow_pos = (g_flow_pos + 1) % 8;
      g_led_val = 1 << g_flow_pos;
    }
  }
  if (g_mode == MODE_SCROLL) {
    u32 div = (g_scroll_speed == 1) ? 2 : 4;
    if (g_tick % div == 0)
      g_scroll_offset = (g_scroll_offset + 1) % 8;
  }
  u32 tcsr = Xil_In32(TIMER_BASE + XTC_TCSR_OFFSET);
  Xil_Out32(TIMER_BASE + XTC_TCSR_OFFSET, tcsr | XTC_CSR_INT_OCCURED_MASK);
}

void ButtonFastHandler(void) __attribute__((fast_interrupt));
void ButtonFastHandler(void) {
  u8 pressed = (u8)((Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F) ^ 0x1F);
  u8 sw = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
  if (pressed & BTN_C) { g_mode = MODE_FLOW; g_flow_speed = (g_flow_speed + 1) % 3; }
  else if (pressed & BTN_L) {
    g_mode = MODE_SHIFT;
    if (!g_led_shifted) { g_led_val = sw; g_led_shifted = 1; }
    else g_led_val = (g_led_val << 1) | ((g_led_val & 0x80) >> 7);
  } else if (pressed & BTN_D) { g_mode = MODE_BIN; g_led_val = 0xAA; g_led_shifted = 0; }
  else if (pressed & BTN_R) { g_mode = MODE_INV; g_led_val = 0x55; g_led_shifted = 0; }
  else if (pressed & BTN_U) { g_mode = MODE_SCROLL; g_scroll_speed = !g_scroll_speed; g_scroll_offset = 0; g_led_shifted = 0; }
  Xil_Out32(GPIO2_BASE + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
}

int main() {
  Xil_Out32(GPIO0_BASE + XGPIO_TRI_OFFSET, 0xFF);
  Xil_Out32(GPIO0_BASE + XGPIO_TRI2_OFFSET, 0x00);
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
  Xil_Out32(TIMER_BASE + XTC_TLR_OFFSET, 24999998);
  Xil_Out32(TIMER_BASE + XTC_TCSR_OFFSET, tcsr0 | XTC_CSR_LOAD_MASK);
  u32 tcsr = Xil_In32(TIMER_BASE + XTC_TCSR_OFFSET);
  Xil_Out32(TIMER_BASE + XTC_TCSR_OFFSET,
            tcsr | XTC_CSR_ENABLE_TMR_MASK | XTC_CSR_AUTO_RELOAD_MASK |
            XTC_CSR_ENABLE_INT_MASK | XTC_CSR_DOWN_COUNT_MASK |
            XTC_CSR_INT_OCCURED_MASK);

  u32 mask = TIMER_INTC_MASK | GPIO2_INTC_MASK;
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, 0xFFFFFFFF);
  Xil_Out32(INTC_BASE + XIN_IER_OFFSET, mask);
  Xil_Out32(INTC_BASE + XIN_MER_OFFSET, XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);
  Xil_Out32(INTC_BASE + XIN_IMR_OFFSET, mask);
  Xil_Out32(INTC_BASE + XIN_IVAR_OFFSET + XPAR_FABRIC_AXI_TIMER_0_INTR * 4, (u32)TimerFastHandler);
  Xil_Out32(INTC_BASE + XIN_IVAR_OFFSET + XPAR_FABRIC_AXI_GPIO_2_INTR * 4, (u32)ButtonFastHandler);
  microblaze_enable_interrupts();

  while (1) {
    u8 sw_raw = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
    u8 sw4 = sw_raw & 0x0F;
    for (int i = 0; i < 8; i++) g_seg_buffer[i] = SEG_BLANK;

    if (g_mode == MODE_BIN) {
      g_seg_buffer[7] = (sw4 & 8) ? SEG_1 : SEG_0;
      g_seg_buffer[6] = (sw4 & 4) ? SEG_1 : SEG_0;
      g_seg_buffer[5] = (sw4 & 2) ? SEG_1 : SEG_0;
      g_seg_buffer[4] = (sw4 & 1) ? SEG_1 : SEG_0;
    } else if (g_mode == MODE_INV) {
      u8 inv = (~sw4) & 0x0F;
      g_seg_buffer[3] = (inv & 8) ? SEG_1 : SEG_0;
      g_seg_buffer[2] = (inv & 4) ? SEG_1 : SEG_0;
      g_seg_buffer[1] = (inv & 2) ? SEG_1 : SEG_0;
      g_seg_buffer[0] = (inv & 1) ? SEG_1 : SEG_0;
    } else if (g_mode == MODE_SCROLL) {
      int dec = signed_val(sw4);
      char str[3]; int len = 0;
      if (dec < 0) { str[len++] = '-'; dec = -dec; }
      if (dec >= 10) { str[len++] = '0' + dec / 10; dec %= 10; }
      str[len++] = '0' + dec;
      for (int i = 0; i < len; i++) {
        u8 seg = (str[i] == '-') ? SEG_MINUS : SEG_NUM[str[i] - '0'];
        g_seg_buffer[(7 - g_scroll_offset - i + 8) % 8] = seg;
      }
      g_led_val = sw_raw;
    }

    Xil_Out32(GPIO0_BASE + XGPIO_DATA2_OFFSET, g_led_val);
    for (int digit = 0; digit < 8; digit++) {
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, (1 << digit));
      Xil_Out32(GPIO1_BASE + XGPIO_DATA_OFFSET, g_seg_buffer[digit]);
      DelayMs(2);
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, 0x00);
    }
  }
  return 0;
}
