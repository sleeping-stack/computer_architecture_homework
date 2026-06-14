/*
 * 题目B — 轮询版本 (v1)
 * 合并原题目5+9: 8位数码管滚动显示数字序列"3456"
 *
 * 功能:
 *   初始: 数码管全熄灭，不滚动
 *   C: 开始/暂停滚动
 *   L: 切换为向左滚动
 *   R: 切换为向右滚动 (默认方向)
 *   滚动周期: 1秒/帧，8帧一个循环
 */

#include "xgpio_l.h"
#include "xil_io.h"

#define GPIO1_BASE XPAR_AXI_GPIO_1_BASEADDR
#define GPIO2_BASE XPAR_AXI_GPIO_2_BASEADDR

#define BTN_C (1 << 2)
#define BTN_L (1 << 1)
#define BTN_R (1 << 3)

#define SEG_BLANK 0xFF
#define SEG_3     0xB0
#define SEG_4     0x99
#define SEG_5     0x92
#define SEG_6     0x82

int main() {
  Xil_Out32(GPIO1_BASE + XGPIO_TRI_OFFSET, 0x00);
  Xil_Out32(GPIO1_BASE + XGPIO_TRI2_OFFSET, 0x00);
  Xil_Out32(GPIO2_BASE + XGPIO_TRI_OFFSET, 0xFF);

  static const u8 SEQ[4] = {SEG_3, SEG_4, SEG_5, SEG_6};
  u8 seg_buf[8];
  u8 offset = 0;          // 序列起始偏移 (0=最左, 递增=右移)
  u8 running = 0;         // 0=暂停, 1=运行
  u8 direction = 0;       // 0=右滚, 1=左滚

  u32 tick_count = 0;

  // 初始全熄灭
  for (int i = 0; i < 8; i++) seg_buf[i] = SEG_BLANK;

  while (1) {
    tick_count++;

    // 1s 定时
    if (running && tick_count >= 8000) {
      tick_count = 0;
      if (direction == 0) offset = (offset + 1) % 8;       // 右滚
      else                offset = (offset + 7) % 8;       // 左滚

      // 更新段码缓冲
      for (int i = 0; i < 8; i++) seg_buf[i] = SEG_BLANK;
      for (int i = 0; i < 4; i++) {
        seg_buf[(7 - offset - i + 8) % 8] = SEQ[i];
      }
    }

    // 动态扫描
    for (int digit = 0; digit < 8; digit++) {
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, (1 << digit));
      Xil_Out32(GPIO1_BASE + XGPIO_DATA_OFFSET, seg_buf[digit]);

      // 延时 + 轮询按键
      for (volatile int t = 0; t < 1200; t++) {
        u32 btn = Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F;
        if (btn != 0x1F) {
          u8 pressed = (u8)(btn ^ 0x1F);

          if (pressed & BTN_C) {
            running = !running;
          } else if (pressed & BTN_L) {
            direction = 1;  // 左滚
          } else if (pressed & BTN_R) {
            direction = 0;  // 右滚
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
