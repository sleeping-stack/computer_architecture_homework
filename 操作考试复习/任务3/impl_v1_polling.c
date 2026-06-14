/*
 * 任务3 — 轮询版本 (v1)
 * 功能:
 *   1. 扫描 4×4 矩阵键盘，识别键值 (0~F)
 *   2. 按键值按先后顺序显示在 8 位数码管上
 *   3. 先按的显示在左边，最后按的显示在右边（新键总是最右侧）
 * 控制方式: 主循环=数码管动态扫描 + 矩阵键盘扫描，纯轮询
 */

#include "xgpio_l.h"
#include "xil_io.h"

// 外设基地址
#define GPIO1_BASE XPAR_AXI_GPIO_1_BASEADDR  // SEG(Ch1) & AN(Ch2)
#define GPIO3_BASE XPAR_AXI_GPIO_3_BASEADDR  // 键盘行(Ch1, 输入) & 列(Ch2, 输出)

// 共阳极段码表 (0=亮): 0~F
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

#define SEG_BLANK 0xFF

// 4×4 矩阵键盘键值映射 (行, 列)
// ROW0: 1 2 3 4, ROW1: 5 6 7 8, ROW2: 9 0 A B, ROW3: C D E F
static const char KEY_MAP[4][4] = {
    {'1', '2', '3', '4'},
    {'5', '6', '7', '8'},
    {'9', '0', 'A', 'B'},
    {'C', 'D', 'E', 'F'},
};

// 字符转段码
static u8 char_to_seg(char c) {
  if (c >= '0' && c <= '9') return SEG_TABLE[c - '0'];
  if (c >= 'A' && c <= 'F') return SEG_TABLE[c - 'A' + 10];
  return SEG_BLANK;
}

// 忙等延时
static void delay_us(u32 us) {
  for (u32 i = 0; i < us * 8; i++) {
    __asm__("nop");
  }
}

/*
 * 扫描矩阵键盘，返回检测到的键值字符
 * 返回值: '0'~'9', 'A'~'F' 表示按键，0xFF 表示无按键
 */
static char scan_keyboard(void) {
  for (int col = 0; col < 4; col++) {
    // 驱动当前列为低电平 (0=选中)，其他列为高电平 (1=不选)
    u32 col_out = (~(1 << col)) & 0x0F;
    Xil_Out32(GPIO3_BASE + XGPIO_DATA2_OFFSET, col_out);

    // 短暂延时等待信号稳定
    delay_us(10);

    // 读取行状态 (按下=0, 上拉=1)
    u32 rows = Xil_In32(GPIO3_BASE + XGPIO_DATA_OFFSET) & 0x0F;

    // 检查每一行
    for (int row = 0; row < 4; row++) {
      if (!(rows & (1 << row))) {
        // 恢复列输出为全高 (释放键盘)
        Xil_Out32(GPIO3_BASE + XGPIO_DATA2_OFFSET, 0x0F);
        return KEY_MAP[row][col];
      }
    }
  }

  // 恢复列输出为全高
  Xil_Out32(GPIO3_BASE + XGPIO_DATA2_OFFSET, 0x0F);
  return 0xFF;  // 无按键按下
}

int main() {
  // GPIO 方向配置
  Xil_Out32(GPIO1_BASE + XGPIO_TRI_OFFSET, 0x00);    // SEG 段码输出
  Xil_Out32(GPIO1_BASE + XGPIO_TRI2_OFFSET, 0x00);   // AN 位选输出
  Xil_Out32(GPIO3_BASE + XGPIO_TRI_OFFSET, 0x0F);    // 键盘行: 输入 (低 4 位)
  Xil_Out32(GPIO3_BASE + XGPIO_TRI2_OFFSET, 0x00);   // 键盘列: 输出 (低 4 位)

  // 初始列输出全高 (不选中任何列)
  Xil_Out32(GPIO3_BASE + XGPIO_DATA2_OFFSET, 0x0F);

  char key_buffer[8];        // 按键 FIFO 缓冲 (最多存 8 个键)
  u8 key_count = 0;          // 已存储按键数
  u8 seg_buffer[8];          // 段码显示缓冲 (8 位数码管)

  char prev_key = 0xFF;      // 上一次扫描到的按键 (去抖用)
  int scan_tick = 0;         // 键盘扫描周期计数器

  // 初始显示全熄灭
  for (int i = 0; i < 8; i++) seg_buffer[i] = SEG_BLANK;

  while (1) {
    // === 动态扫描 8 位数码管 ===
    for (int digit = 0; digit < 8; digit++) {
      // 位选: bit7=最左, bit0=最右
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, (1 << digit));
      // 段码
      Xil_Out32(GPIO1_BASE + XGPIO_DATA_OFFSET, seg_buffer[digit]);

      // 延时 ~2ms, 同时在此间隙执行键盘扫描
      for (volatile int t = 0; t < 1200; t++) {
        __asm__("nop");
      }

      // 消隐
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, 0x00);
    }

    // === 每轮数码管扫描完后，扫描一次矩阵键盘 ===
    scan_tick++;
    if (scan_tick >= 4) {  // 约每 64ms 扫描一次键盘 (4 轮显示)
      scan_tick = 0;

      char cur_key = scan_keyboard();

      if (cur_key != 0xFF && cur_key != prev_key) {
        // 检测到新按键按下
        if (key_count < 8) {
          // 追加到缓冲末尾
          key_buffer[key_count++] = cur_key;
        } else {
          // 缓冲已满，左移一位，新键放入最右侧
          for (int i = 0; i < 7; i++) {
            key_buffer[i] = key_buffer[i + 1];
          }
          key_buffer[7] = cur_key;
        }

        // 更新段码显示缓冲: key_buffer[0]→最左位(digit7), 依此类推
        for (int d = 0; d < 8; d++) {
          seg_buffer[d] = SEG_BLANK;  // 先全部熄灭
        }
        for (int i = 0; i < key_count; i++) {
          seg_buffer[7 - i] = char_to_seg(key_buffer[i]);
        }
      }

      prev_key = cur_key;
    }
  }

  return 0;
}
