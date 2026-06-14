/*
 * 题目A — 轮询版本 (v1)
 * 合并原题目2+3+7: Hex显示 + 自动递增 + 手动/自动移位
 *
 * 功能:
 *   C: 读8位开关 → 2位Hex显示在最低2位数码管 (高6位灭)
 *   L: 启动/停止每秒+1递增 (0→255→0循环)
 *   R: 循环切换自动移位: 关→左移1Hz→右移1Hz→关
 *   U: 手动整体左移2位Hex (到最左回最右)
 *   D: 手动整体右移2位Hex (到最右回最左)
 */

#include "xgpio_l.h"
#include "xil_io.h"

#define GPIO0_BASE XPAR_AXI_GPIO_0_BASEADDR
#define GPIO1_BASE XPAR_AXI_GPIO_1_BASEADDR
#define GPIO2_BASE XPAR_AXI_GPIO_2_BASEADDR

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

// 更新段码缓冲: 在 display_pos 和 display_pos-1 处显示 value 的 Hex
static void update_seg(u8 *buf, u8 val, u8 pos) {
  for (int i = 0; i < 8; i++) buf[i] = SEG_BLANK;
  u8 hi = (val >> 4) & 0x0F;
  u8 lo = val & 0x0F;
  buf[pos] = SEG_TABLE[hi];
  buf[(pos + 7) % 8] = SEG_TABLE[lo];  // pos-1 with wrap
}

int main() {
  Xil_Out32(GPIO0_BASE + XGPIO_TRI_OFFSET, 0xFF);
  Xil_Out32(GPIO1_BASE + XGPIO_TRI_OFFSET, 0x00);
  Xil_Out32(GPIO1_BASE + XGPIO_TRI2_OFFSET, 0x00);
  Xil_Out32(GPIO2_BASE + XGPIO_TRI_OFFSET, 0xFF);

  u8 seg_buf[8];
  u8 sw_val = 0;
  u8 display_pos = 1;    // Hex低位在pos, 高位在pos+1; 初始在最低2位
  u8 inc_mode = 0;       // 0=停, 1=递增
  u8 shift_mode = 0;     // 0=关, 1=左移, 2=右移

  u32 tick_counter = 0;  // 忙等计时器
  u32 tick_1s = 0;       // 1s计时

  for (int i = 0; i < 8; i++) seg_buf[i] = SEG_BLANK;

  while (1) {
    tick_counter++;

    // === 1s 定时 ===
    if (tick_counter >= 8000) {  // ~1s (粗略)
      tick_counter = 0;
      tick_1s++;

      // 递增模式
      if (inc_mode) {
        sw_val++;
        update_seg(seg_buf, sw_val, display_pos);
      }

      // 自动移位模式
      if (shift_mode == 1) {  // 左移
        display_pos = (display_pos + 2) % 8;
        update_seg(seg_buf, sw_val, display_pos);
      } else if (shift_mode == 2) {  // 右移
        display_pos = (display_pos + 6) % 8;  // -2 mod 8
        update_seg(seg_buf, sw_val, display_pos);
      }
    }

    // === 动态扫描数码管 ===
    for (int digit = 0; digit < 8; digit++) {
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, (1 << digit));
      Xil_Out32(GPIO1_BASE + XGPIO_DATA_OFFSET, seg_buf[digit]);

      // 延时约 2ms + 轮询按键
      for (volatile int t = 0; t < 1200; t++) {
        u32 btn = Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F;
        if (btn != 0x1F) {
          u8 pressed = (u8)(btn ^ 0x1F);

          if (pressed & BTN_C) {
            sw_val = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
            inc_mode = 0;
            shift_mode = 0;
            display_pos = 1;  // 回到最低2位
            update_seg(seg_buf, sw_val, display_pos);
          } else if (pressed & BTN_L) {
            inc_mode = !inc_mode;
            shift_mode = 0;
            if (inc_mode) sw_val = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
            update_seg(seg_buf, sw_val, display_pos);
          } else if (pressed & BTN_R) {
            inc_mode = 0;
            shift_mode = (shift_mode + 1) % 3;  // 0→1→2→0
            update_seg(seg_buf, sw_val, display_pos);
          } else if (pressed & BTN_U) {
            inc_mode = 0;
            shift_mode = 0;
            display_pos = (display_pos + 2) % 8;
            update_seg(seg_buf, sw_val, display_pos);
          } else if (pressed & BTN_D) {
            inc_mode = 0;
            shift_mode = 0;
            display_pos = (display_pos + 6) % 8;  // -2 mod 8
            update_seg(seg_buf, sw_val, display_pos);
          }

          // 去抖: 等待释放
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
