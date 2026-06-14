/*
 * 题目C — 快速中断版本 (v3)
 * 有符号4位二进制运算 (符号-绝对值)
 * GPIO_2快速中断处理C/L按键
 */

#include "mb_interface.h"
#include "xgpio_l.h"
#include "xil_io.h"
#include "xintc_l.h"
#include "xparameters.h"

#define GPIO0_BASE XPAR_AXI_GPIO_0_BASEADDR
#define GPIO1_BASE XPAR_AXI_GPIO_1_BASEADDR
#define GPIO2_BASE XPAR_AXI_GPIO_2_BASEADDR
#define INTC_BASE  XPAR_XINTC_0_BASEADDR
#define GPIO2_INTC_MASK (1 << XPAR_FABRIC_AXI_GPIO_2_INTR)

#define BTN_C (1 << 2)
#define BTN_L (1 << 1)

#define SEG_BLANK 0xFF
#define SEG_0     0xC0
#define SEG_1     0xF9
#define SEG_MINUS 0xBF

static const u8 SEG_NUM[10] = {0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90};

volatile u8 g_show_decimal = 0;
volatile u8 g_invert_low3 = 0;
volatile u8 g_seg_buffer[8];

static int signed_val(u8 bits) {
  return ((bits & 0x08) ? -1 : 1) * (int)(bits & 0x07);
}

static void update_seg(u8 sw4) {
  g_seg_buffer[7] = (sw4 & 0x08) ? SEG_1 : SEG_0;
  g_seg_buffer[6] = (sw4 & 0x04) ? SEG_1 : SEG_0;
  g_seg_buffer[5] = (sw4 & 0x02) ? SEG_1 : SEG_0;
  g_seg_buffer[4] = (sw4 & 0x01) ? SEG_1 : SEG_0;
  g_seg_buffer[3] = SEG_BLANK; g_seg_buffer[2] = SEG_BLANK;
  if (g_show_decimal) {
    u8 val = g_invert_low3 ? ((sw4 & 0x08) | ((~sw4) & 0x07)) : sw4;
    int dec = signed_val(val);
    g_seg_buffer[1] = (dec < 0) ? SEG_MINUS : SEG_BLANK;
    g_seg_buffer[0] = SEG_NUM[(dec < 0) ? -dec : dec];
  } else { g_seg_buffer[1] = SEG_BLANK; g_seg_buffer[0] = SEG_BLANK; }
}

static void DelayMs(u32 ms) {
  for (u32 i = 0; i < ms * 150; i++)
    for (volatile int j = 0; j < 10; j++);
}

void ButtonFastHandler(void) __attribute__((fast_interrupt));
void ButtonFastHandler(void) {
  u8 pressed = (u8)((Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F) ^ 0x1F);
  if (pressed & BTN_C) { g_show_decimal = 1; g_invert_low3 = 0; }
  else if (pressed & BTN_L) { g_show_decimal = 1; g_invert_low3 = !g_invert_low3; }
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
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, 0xFFFFFFFF);
  Xil_Out32(INTC_BASE + XIN_IER_OFFSET, GPIO2_INTC_MASK);
  Xil_Out32(INTC_BASE + XIN_MER_OFFSET, XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);
  Xil_Out32(INTC_BASE + XIN_IMR_OFFSET, GPIO2_INTC_MASK);
  Xil_Out32(INTC_BASE + XIN_IVAR_OFFSET + XPAR_FABRIC_AXI_GPIO_2_INTR * 4,
            (u32)ButtonFastHandler);
  microblaze_enable_interrupts();

  while (1) {
    u8 sw4 = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0x0F);
    update_seg(sw4);
    for (int digit = 0; digit < 8; digit++) {
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, (1 << digit));
      Xil_Out32(GPIO1_BASE + XGPIO_DATA_OFFSET, g_seg_buffer[digit]);
      DelayMs(2);
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, 0x00);
    }
  }
  return 0;
}
