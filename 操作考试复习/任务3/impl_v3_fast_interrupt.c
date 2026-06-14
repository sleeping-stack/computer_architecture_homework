/*
 * 任务3 — 快速中断版本 (v3)
 * 功能:
 *   1. 扫描 4×4 矩阵键盘，识别键值 (0~F)
 *   2. 按键值按先后顺序显示在 8 位数码管上
 *   3. 先按的显示在左边，最后按的显示在右边
 * 控制方式: 主循环进行数码管动态扫描 + 键盘扫描
 *           Timer 快速中断定时触发键盘扫描标志
 *           快速中断: 配置 IMR + IVAR，ISR 仅清除 Timer CSR
 */

#include "mb_interface.h"
#include "xgpio_l.h"
#include "xil_io.h"
#include "xintc_l.h"
#include "xparameters.h"
#include "xtmrctr_l.h"

// 外设基地址
#define GPIO1_BASE XPAR_AXI_GPIO_1_BASEADDR   // SEG(Ch1) & AN(Ch2)
#define GPIO3_BASE XPAR_AXI_GPIO_3_BASEADDR   // 键盘行(Ch1) & 列(Ch2)
#define TIMER_BASE XPAR_AXI_TIMER_0_BASEADDR  // 定时器
#define INTC_BASE  XPAR_XINTC_0_BASEADDR      // 中断控制器

// Timer 在 INTC 中的中断位掩码 (INT ID = 6)
#define TIMER_INTC_MASK (1 << XPAR_FABRIC_AXI_TIMER_0_INTR)

// 共阳极段码表: 0~F
static const u8 SEG_TABLE[16] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8,
    0x80, 0x90, 0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E,
};

#define SEG_BLANK 0xFF

// 矩阵键盘键值映射
static const char KEY_MAP[4][4] = {
    {'1', '2', '3', '4'},
    {'5', '6', '7', '8'},
    {'9', '0', 'A', 'B'},
    {'C', 'D', 'E', 'F'},
};

// 全局变量
volatile u8 g_seg_buffer[8];
volatile u8 g_key_scan_flag = 0;

// 字符转段码
static u8 char_to_seg(char c) {
  if (c >= '0' && c <= '9') return SEG_TABLE[c - '0'];
  if (c >= 'A' && c <= 'F') return SEG_TABLE[c - 'A' + 10];
  return SEG_BLANK;
}

// 忙等微秒延时
static void delay_us(u32 us) {
  for (u32 i = 0; i < us * 8; i++) {
    __asm__("nop");
  }
}

// 毫秒延时
static void DelayMs(u32 ms) {
  for (u32 i = 0; i < ms * 150; i++) {
    for (volatile int j = 0; j < 10; j++)
      ;
  }
}

// 扫描矩阵键盘
static char scan_keyboard(void) {
  for (int col = 0; col < 4; col++) {
    u32 col_out = (~(1 << col)) & 0x0F;
    Xil_Out32(GPIO3_BASE + XGPIO_DATA2_OFFSET, col_out);
    delay_us(10);
    u32 rows = Xil_In32(GPIO3_BASE + XGPIO_DATA_OFFSET) & 0x0F;
    for (int row = 0; row < 4; row++) {
      if (!(rows & (1 << row))) {
        Xil_Out32(GPIO3_BASE + XGPIO_DATA2_OFFSET, 0x0F);
        return KEY_MAP[row][col];
      }
    }
  }
  Xil_Out32(GPIO3_BASE + XGPIO_DATA2_OFFSET, 0x0F);
  return 0xFF;
}

// 快速中断服务函数: Timer 定时触发键盘扫描
void TimerFastHandler(void) __attribute__((fast_interrupt));

void TimerFastHandler(void) {
  g_key_scan_flag = 1;

  // 清除 Timer 中断状态 (快速中断: 仅清除 Timer CSR, INTC 硬件自动应答)
  u32 tcsr = Xil_In32(TIMER_BASE + XTC_TCSR_OFFSET);
  Xil_Out32(TIMER_BASE + XTC_TCSR_OFFSET, tcsr | XTC_CSR_INT_OCCURED_MASK);
}

int main() {
  // GPIO 方向配置
  Xil_Out32(GPIO1_BASE + XGPIO_TRI_OFFSET, 0x00);
  Xil_Out32(GPIO1_BASE + XGPIO_TRI2_OFFSET, 0x00);
  Xil_Out32(GPIO3_BASE + XGPIO_TRI_OFFSET, 0x0F);
  Xil_Out32(GPIO3_BASE + XGPIO_TRI2_OFFSET, 0x00);

  // 初始列输出全高
  Xil_Out32(GPIO3_BASE + XGPIO_DATA2_OFFSET, 0x0F);

  char key_buffer[8];
  u8 key_count = 0;
  char prev_key = 0xFF;

  for (int i = 0; i < 8; i++) g_seg_buffer[i] = SEG_BLANK;

  // === 配置 Timer ===
  u32 tcsr0 = Xil_In32(TIMER_BASE + XTC_TCSR_OFFSET);
  tcsr0 &= ~XTC_CSR_ENABLE_TMR_MASK;
  Xil_Out32(TIMER_BASE + XTC_TCSR_OFFSET, tcsr0);

  // 定时周期: ~50ms (20Hz 键盘扫描)
  Xil_Out32(TIMER_BASE + XTC_TLR_OFFSET, 4999998);

  // 加载 TLR
  Xil_Out32(TIMER_BASE + XTC_TCSR_OFFSET,
            tcsr0 | XTC_CSR_LOAD_MASK);

  // 重新读取 TCSR 并配置
  u32 tcsr = Xil_In32(TIMER_BASE + XTC_TCSR_OFFSET);
  Xil_Out32(TIMER_BASE + XTC_TCSR_OFFSET,
            tcsr | XTC_CSR_ENABLE_TMR_MASK
                 | XTC_CSR_AUTO_RELOAD_MASK
                 | XTC_CSR_ENABLE_INT_MASK
                 | XTC_CSR_DOWN_COUNT_MASK
                 | XTC_CSR_INT_OCCURED_MASK);

  // === 配置 INTC (快速中断: 需要 IMR + IVAR) ===
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, 0xFFFFFFFF);
  Xil_Out32(INTC_BASE + XIN_IER_OFFSET, TIMER_INTC_MASK);
  Xil_Out32(INTC_BASE + XIN_MER_OFFSET,
            XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);
  // 快速中断特有: 中断模式寄存器
  Xil_Out32(INTC_BASE + XIN_IMR_OFFSET, TIMER_INTC_MASK);
  // 快速中断特有: 向量地址寄存器
  Xil_Out32(INTC_BASE + XIN_IVAR_OFFSET + XPAR_FABRIC_AXI_TIMER_0_INTR * 4,
            (u32)TimerFastHandler);

  // 开启 MicroBlaze 中断
  microblaze_enable_interrupts();

  // 主循环: 数码管动态扫描 + 键盘扫描处理
  while (1) {
    // === 处理键盘扫描 ===
    if (g_key_scan_flag) {
      g_key_scan_flag = 0;

      char cur_key = scan_keyboard();

      if (cur_key != 0xFF && cur_key != prev_key) {
        if (key_count < 8) {
          key_buffer[key_count++] = cur_key;
        } else {
          for (int i = 0; i < 7; i++) {
            key_buffer[i] = key_buffer[i + 1];
          }
          key_buffer[7] = cur_key;
        }

        for (int d = 0; d < 8; d++) g_seg_buffer[d] = SEG_BLANK;
        for (int i = 0; i < key_count; i++) {
          g_seg_buffer[7 - i] = char_to_seg(key_buffer[i]);
        }
      }
      prev_key = cur_key;
    }

    // === 动态扫描数码管 ===
    for (int digit = 0; digit < 8; digit++) {
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, (1 << digit));
      Xil_Out32(GPIO1_BASE + XGPIO_DATA_OFFSET, g_seg_buffer[digit]);
      DelayMs(2);
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, 0x00);
    }
  }

  return 0;
}
