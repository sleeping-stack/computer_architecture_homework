/*
 * 题目E — 轮询版本 (v1)
 * 综合多功能题: 5个按键分别控制5种独立功能
 *
 * C(A): 流水灯速度切换 (1s→0.5s→0.25s→1s), 数码管熄灭
 * L(B): 第1次→LED=开关, 后续→LED左移(循环)
 * D(C): 左4位=开关低4位二进制, 右4位熄灭, LED奇数位亮
 * R(D): 右4位=开关低4位反码, 左4位熄灭, LED偶数位亮
 * U(E): 开关低4位有符号十进制滚动, 速度1s/0.5s切换, LED=开关
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
#define SEG_0     0xC0
#define SEG_1     0xF9
#define SEG_MINUS 0xBF

static const u8 SEG_NUM[10] = {0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,0x80,0x90};

#define MODE_FLOW   0  // C: 流水灯
#define MODE_SHIFT  1  // L: LED移位
#define MODE_BIN    2  // D: 开关二进制+LED奇偶
#define MODE_INV    3  // R: 反码+LED偶奇
#define MODE_SCROLL 4  // U: 有符号十进制滚动

static int signed_val(u8 bits) {
  return ((bits & 0x08) ? -1 : 1) * (int)(bits & 0x07);
}

int main() {
  Xil_Out32(GPIO0_BASE + XGPIO_TRI_OFFSET, 0xFF);
  Xil_Out32(GPIO0_BASE + XGPIO_TRI2_OFFSET, 0x00);
  Xil_Out32(GPIO1_BASE + XGPIO_TRI_OFFSET, 0x00);
  Xil_Out32(GPIO1_BASE + XGPIO_TRI2_OFFSET, 0x00);
  Xil_Out32(GPIO2_BASE + XGPIO_TRI_OFFSET, 0xFF);

  u8 seg_buf[8];
  u8 mode = MODE_FLOW;
  u8 led_val = 0x01;       // 流水灯起始
  u8 flow_pos = 0;
  u8 flow_speed = 0;       // 0=1s, 1=0.5s, 2=0.25s
  u8 led_shifted = 0;      // L按键: LED是否已读入开关
  u8 scroll_speed = 0;     // 0=1s, 1=0.5s
  u8 scroll_offset = 0;

  u32 tick = 0;            // ~16ms per tick (1 full scan cycle)
  u32 flow_tick = 0;
  u32 scroll_tick = 0;

  for (int i = 0; i < 8; i++) seg_buf[i] = SEG_BLANK;

  while (1) {
    tick++;

    // 流水灯定时
    u32 flow_period = (flow_speed == 2) ? 1 : (flow_speed == 1) ? 2 : 4;
    if (tick - flow_tick >= flow_period) {
      flow_tick = tick;
      if (mode == MODE_FLOW) {
        flow_pos = (flow_pos + 1) % 8;
        led_val = 1 << flow_pos;
      }
    }

    // 滚动定时
    u32 scroll_period = (scroll_speed == 1) ? 2 : 4;
    if (tick - scroll_tick >= scroll_period) {
      scroll_tick = tick;
      if (mode == MODE_SCROLL)
        scroll_offset = (scroll_offset + 1) % 8;
    }

    // 计算段码缓冲
    u8 sw_raw = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
    u8 sw4 = sw_raw & 0x0F;

    for (int i = 0; i < 8; i++) seg_buf[i] = SEG_BLANK;

    if (mode == MODE_BIN) {
      // 左4位=开关低4位二进制
      seg_buf[7] = (sw4 & 0x08) ? SEG_1 : SEG_0;
      seg_buf[6] = (sw4 & 0x04) ? SEG_1 : SEG_0;
      seg_buf[5] = (sw4 & 0x02) ? SEG_1 : SEG_0;
      seg_buf[4] = (sw4 & 0x01) ? SEG_1 : SEG_0;
      led_val = 0xAA;  // 奇数位亮
    } else if (mode == MODE_INV) {
      // 右4位=反码
      u8 inv = (~sw4) & 0x0F;
      seg_buf[3] = (inv & 0x08) ? SEG_1 : SEG_0;
      seg_buf[2] = (inv & 0x04) ? SEG_1 : SEG_0;
      seg_buf[1] = (inv & 0x02) ? SEG_1 : SEG_0;
      seg_buf[0] = (inv & 0x01) ? SEG_1 : SEG_0;
      led_val = 0x55;  // 偶数位亮
    } else if (mode == MODE_SCROLL) {
      // 有符号十进制滚动
      int dec = signed_val(sw4);
      char str[3]; int len = 0;
      if (dec < 0) { str[len++] = '-'; dec = -dec; }
      if (dec >= 10) { str[len++] = '0' + dec / 10; dec %= 10; }
      str[len++] = '0' + dec;
      for (int i = 0; i < len; i++) {
        u8 seg = (str[i] == '-') ? SEG_MINUS : SEG_NUM[str[i] - '0'];
        seg_buf[(7 - scroll_offset - i + 8) % 8] = seg;
      }
      led_val = sw_raw;  // LED=开关
    } else if (mode == MODE_FLOW) {
      for (int i = 0; i < 8; i++) seg_buf[i] = SEG_BLANK;
    }
    // MODE_SHIFT: seg_buf blank, led_val handled by L button

    // 输出LED
    Xil_Out32(GPIO0_BASE + XGPIO_DATA2_OFFSET, led_val);

    // 动态扫描
    for (int digit = 0; digit < 8; digit++) {
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, (1 << digit));
      Xil_Out32(GPIO1_BASE + XGPIO_DATA_OFFSET, seg_buf[digit]);

      for (volatile int t = 0; t < 1200; t++) {
        u32 btn = Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F;
        if (btn != 0x1F) {
          u8 pressed = (u8)(btn ^ 0x1F);

          if (pressed & BTN_C) {
            mode = MODE_FLOW;
            flow_speed = (flow_speed + 1) % 3;
            led_val = 1 << flow_pos;
          } else if (pressed & BTN_L) {
            mode = MODE_SHIFT;
            if (!led_shifted) {
              led_val = sw_raw;
              led_shifted = 1;
            } else {
              led_val = (led_val << 1) | ((led_val & 0x80) >> 7);
            }
          } else if (pressed & BTN_D) {
            mode = MODE_BIN;
            led_shifted = 0;
          } else if (pressed & BTN_R) {
            mode = MODE_INV;
            led_shifted = 0;
          } else if (pressed & BTN_U) {
            mode = MODE_SCROLL;
            scroll_speed = !scroll_speed;
            scroll_offset = 0;
            led_shifted = 0;
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
