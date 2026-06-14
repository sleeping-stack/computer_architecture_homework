/*
 * 任务2 — 轮询版本 (v1)
 * 功能:
 *   BTNC: 读开关 → 二进制 0/1 显示到全部 8 位数码管
 *   BTNU: 读开关 → 十六进制 0~F 显示到低 2 位数码管 (高 6 位熄灭)
 *   BTND: 读开关 → 无符号十进制 0~255 显示到低 3 位数码管 (高 5 位熄灭)
 * 控制方式: 数码管动态扫描为主循环，延时内轮询按键和开关
 */

#include "xgpio_l.h"
#include "xil_io.h"

// 外设基地址
#define GPIO0_BASE XPAR_AXI_GPIO_0_BASEADDR  // SW(Ch1)
#define GPIO1_BASE XPAR_AXI_GPIO_1_BASEADDR  // SEG(Ch1) & AN(Ch2)
#define GPIO2_BASE XPAR_AXI_GPIO_2_BASEADDR  // BTN(Ch1)

// 按键位掩码 (按下=0, 上拉=1)
#define BTN_U (1 << 0)  // BTNU
#define BTN_C (1 << 2)  // BTNC
#define BTN_D (1 << 4)  // BTND

// 显示模式
#define MODE_BINARY 0  // 二进制
#define MODE_HEX    1  // 十六进制
#define MODE_DEC    2  // 十进制

// 共阳极段码表 (0=亮, 1=灭): 0~F
static const u8 SEG_TABLE[16] = {
    0xC0,  // 0
    0xF9,  // 1
    0xA4,  // 2
    0xB0,  // 3
    0x99,  // 4
    0x92,  // 5
    0x82,  // 6
    0xF8,  // 7
    0x80,  // 8
    0x90,  // 9
    0x88,  // A
    0x83,  // B
    0xC6,  // C
    0xA1,  // D
    0x86,  // E
    0x8E,  // F
};

#define SEG_0     0xC0
#define SEG_1     0xF9
#define SEG_BLANK 0xFF

// 忙等延时 (约 ms 毫秒)
static void delay_ms(u32 ms) {
  for (u32 i = 0; i < ms * 8000; i++) {
    __asm__("nop");
  }
}

// 根据模式和开关值更新段码缓冲
static void update_seg_buffer(u8 seg_buf[8], u8 sw_val, u8 mode) {
  if (mode == MODE_BINARY) {
    // 二进制: 每位对应一个数码管, bit7→最左, bit0→最右
    for (int i = 0; i < 8; i++) {
      seg_buf[i] = (sw_val & (1 << i)) ? SEG_1 : SEG_0;
    }
  } else if (mode == MODE_HEX) {
    // 十六进制: 仅低 2 位显示, 高位熄灭
    seg_buf[0] = SEG_TABLE[sw_val & 0x0F];
    seg_buf[1] = SEG_TABLE[(sw_val >> 4) & 0x0F];
    for (int i = 2; i < 8; i++) {
      seg_buf[i] = SEG_BLANK;
    }
  } else {  // MODE_DEC
    // 十进制: 仅低 3 位显示, 高位熄灭
    u8 val = sw_val;
    seg_buf[0] = SEG_TABLE[val % 10];
    val /= 10;
    seg_buf[1] = (val > 0) ? SEG_TABLE[val % 10] : SEG_BLANK;
    val /= 10;
    seg_buf[2] = (val > 0) ? SEG_TABLE[val % 10] : SEG_BLANK;
    for (int i = 3; i < 8; i++) {
      seg_buf[i] = SEG_BLANK;
    }
  }
}

int main() {
  // GPIO 方向配置
  Xil_Out32(GPIO0_BASE + XGPIO_TRI_OFFSET, 0xFF);   // SW 输入
  Xil_Out32(GPIO1_BASE + XGPIO_TRI_OFFSET, 0x00);    // SEG 段码输出
  Xil_Out32(GPIO1_BASE + XGPIO_TRI2_OFFSET, 0x00);   // AN 位选输出
  Xil_Out32(GPIO2_BASE + XGPIO_TRI_OFFSET, 0xFF);    // BTN 输入

  u8 seg_buffer[8];       // 8 位数码管段码缓冲
  u8 sw_value = 0;        // 保存的开关值
  u8 display_mode = MODE_BINARY;  // 当前显示模式

  // 初始化为全熄灭
  for (int i = 0; i < 8; i++) seg_buffer[i] = SEG_BLANK;

  while (1) {
    // 动态扫描 8 位数码管
    for (int digit = 0; digit < 8; digit++) {
      // 位选: bit7=最左, bit0=最右
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, (1 << digit));
      // 段码
      Xil_Out32(GPIO1_BASE + XGPIO_DATA_OFFSET, seg_buffer[digit]);

      // 在延时期间轮询按键和开关 (延时约 2ms)
      for (volatile int t = 0; t < 1200; t++) {
        // 读取开关
        u32 sw_raw = Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF;
        // 读取按键
        u32 btn_raw = Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F;

        if (btn_raw != 0x1F) {
          u8 pressed = (u8)(btn_raw ^ 0x1F);

          if (pressed & BTN_C) {
            sw_value = (u8)sw_raw;
            display_mode = MODE_BINARY;
          } else if (pressed & BTN_U) {
            sw_value = (u8)sw_raw;
            display_mode = MODE_HEX;
          } else if (pressed & BTN_D) {
            sw_value = (u8)sw_raw;
            display_mode = MODE_DEC;
          }

          // 更新段码缓冲
          update_seg_buffer(seg_buffer, sw_value, display_mode);

          // 去抖: 等待按键释放
          while ((Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F) != 0x1F) {
            delay_ms(1);
          }
          delay_ms(10);
          break;  // 跳出内层延时循环
        }
      }

      // 消隐 (防止鬼影)
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, 0x00);
    }
  }

  return 0;
}
