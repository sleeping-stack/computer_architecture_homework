/*
 * 题目D — 轮询版本 (v1)
 * LED移位: C=读开关→LED, R=LED循环右移一位
 */

#include "xgpio_l.h"
#include "xil_io.h"

#define GPIO0_BASE XPAR_AXI_GPIO_0_BASEADDR
#define GPIO2_BASE XPAR_AXI_GPIO_2_BASEADDR

#define BTN_C (1 << 2)
#define BTN_R (1 << 3)

int main() {
  Xil_Out32(GPIO0_BASE + XGPIO_TRI_OFFSET, 0xFF);
  Xil_Out32(GPIO0_BASE + XGPIO_TRI2_OFFSET, 0x00);
  Xil_Out32(GPIO2_BASE + XGPIO_TRI_OFFSET, 0xFF);

  u8 led_val = 0;
  Xil_Out32(GPIO0_BASE + XGPIO_DATA2_OFFSET, 0x00);

  while (1) {
    // 输出 LED
    Xil_Out32(GPIO0_BASE + XGPIO_DATA2_OFFSET, led_val);

    // 轮询按键
    u32 btn = Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F;
    if (btn != 0x1F) {
      u8 pressed = (u8)(btn ^ 0x1F);

      if (pressed & BTN_C) {
        led_val = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
      } else if (pressed & BTN_R) {
        // 循环右移: LSB→bit7, 其余右移
        u8 lsb = led_val & 0x01;
        led_val = (led_val >> 1) | (lsb << 7);
      }

      // 去抖
      while ((Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F) != 0x1F) {
        for (volatile int d = 0; d < 1000; d++) __asm__("nop");
      }
      for (volatile int d = 0; d < 5000; d++) __asm__("nop");
    }

    for (volatile int d = 0; d < 10000; d++) __asm__("nop");
  }
  return 0;
}
