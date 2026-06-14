/*
 * 题目C — 轮询版本 (v1)
 * 有符号4位二进制运算 (符号-绝对值表示法)
 *
 * 读取最右边4位开关作为4-bit有符号数:
 *   bit3=符号位 (0=正,1=负), bit2-0=绝对值 (0~7)
 *
 *   左4位数码管: 始终显示4位二进制 (0/1)
 *   C: 右边2位显示有符号十进制值 (-7 ~ +7)
 *   L: 切换运算 (奇数按=低3位取反, 偶数按=原值)
 */

#include "xgpio_l.h"
#include "xil_io.h"

#define GPIO0_BASE XPAR_AXI_GPIO_0_BASEADDR
#define GPIO1_BASE XPAR_AXI_GPIO_1_BASEADDR
#define GPIO2_BASE XPAR_AXI_GPIO_2_BASEADDR

#define BTN_C (1 << 2)
#define BTN_L (1 << 1)

#define SEG_BLANK 0xFF
#define SEG_0     0xC0
#define SEG_1     0xF9
#define SEG_MINUS 0xBF  // 仅 g 段亮 (共阳极)

static const u8 SEG_NUM[10] = {0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90};

// 4-bit有符号(符号-绝对值) → 十进制值 (-7 ~ +7)
static int signed_val(u8 bits) {
  int sign = (bits & 0x08) ? -1 : 1;
  int mag = bits & 0x07;
  return sign * mag;
}

// 更新段码缓冲
static void update_seg(u8 *buf, u8 sw4, u8 show_decimal, u8 invert_low3) {
  // 左4位: 二进制 (bit3→pos7, bit2→pos6, bit1→pos5, bit0→pos4)
  buf[7] = (sw4 & 0x08) ? SEG_1 : SEG_0;
  buf[6] = (sw4 & 0x04) ? SEG_1 : SEG_0;
  buf[5] = (sw4 & 0x02) ? SEG_1 : SEG_0;
  buf[4] = (sw4 & 0x01) ? SEG_1 : SEG_0;

  // 右2位 + 中间2位
  buf[3] = SEG_BLANK;
  buf[2] = SEG_BLANK;

  if (show_decimal) {
    u8 val = sw4;
    if (invert_low3) val = (val & 0x08) | ((~val) & 0x07);  // 符号位不变, 低3位取反
    int dec = signed_val(val);
    int abs_v = (dec < 0) ? -dec : dec;
    if (dec < 0) {
      buf[1] = SEG_MINUS;
    } else {
      buf[1] = SEG_BLANK;
    }
    buf[0] = SEG_NUM[abs_v];
  } else {
    buf[1] = SEG_BLANK;
    buf[0] = SEG_BLANK;
  }
}

int main() {
  Xil_Out32(GPIO0_BASE + XGPIO_TRI_OFFSET, 0xFF);
  Xil_Out32(GPIO1_BASE + XGPIO_TRI_OFFSET, 0x00);
  Xil_Out32(GPIO1_BASE + XGPIO_TRI2_OFFSET, 0x00);
  Xil_Out32(GPIO2_BASE + XGPIO_TRI_OFFSET, 0xFF);

  u8 seg_buf[8];
  u8 show_decimal = 0;   // C按键后=1
  u8 invert_low3 = 0;    // L切换
  for (int i = 0; i < 8; i++) seg_buf[i] = SEG_BLANK;

  while (1) {
    u8 sw_raw = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
    u8 sw4 = sw_raw & 0x0F;
    update_seg(seg_buf, sw4, show_decimal, invert_low3);

    for (int digit = 0; digit < 8; digit++) {
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, (1 << digit));
      Xil_Out32(GPIO1_BASE + XGPIO_DATA_OFFSET, seg_buf[digit]);

      for (volatile int t = 0; t < 1200; t++) {
        u32 btn = Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F;
        if (btn != 0x1F) {
          u8 pressed = (u8)(btn ^ 0x1F);
          if (pressed & BTN_C) {
            show_decimal = 1;
            invert_low3 = 0;
          } else if (pressed & BTN_L) {
            show_decimal = 1;
            invert_low3 = !invert_low3;
          }
          while ((Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F) != 0x1F) {
            for (volatile int d = 0; d < 1000; d++) __asm__("nop");
          }
          for (volatile int d = 0; d < 5000; d++) __asm__("nop");
          break;
        }
      }
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, 0x00);
    }
  }
  return 0;
}
