/*
 * 任务2 — 普通中断版本 (v2)
 * 功能:
 *   BTNC: 读开关 → 二进制 0/1 显示到全部 8 位数码管
 *   BTNU: 读开关 → 十六进制 0~F 显示到低 2 位数码管 (高 6 位熄灭)
 *   BTND: 读开关 → 无符号十进制 0~255 显示到低 3 位数码管 (高 5 位熄灭)
 * 控制方式: 主循环动态扫描数码管，GPIO_2 中断处理按键
 *           普通中断: ISR 清除 GPIO ISR + INTC IAR
 */

#include "mb_interface.h"
#include "xgpio_l.h"
#include "xil_io.h"
#include "xintc_l.h"
#include "xparameters.h"

// 外设基地址
#define GPIO0_BASE XPAR_AXI_GPIO_0_BASEADDR  // SW(Ch1)
#define GPIO1_BASE XPAR_AXI_GPIO_1_BASEADDR  // SEG(Ch1) & AN(Ch2)
#define GPIO2_BASE XPAR_AXI_GPIO_2_BASEADDR  // BTN(Ch1)
#define INTC_BASE  XPAR_XINTC_0_BASEADDR     // 中断控制器

// GPIO_2 在 INTC 中的中断位掩码 (INT ID = 3)
#define GPIO2_INTC_MASK (1 << XPAR_FABRIC_AXI_GPIO_2_INTR)

// 按键位掩码 (按下=0, 上拉=1)
#define BTN_U (1 << 0)  // BTNU
#define BTN_C (1 << 2)  // BTNC
#define BTN_D (1 << 4)  // BTND

// 显示模式
#define MODE_BINARY 0
#define MODE_HEX    1
#define MODE_DEC    2

// 共阳极段码表: 0~F
static const u8 SEG_TABLE[16] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8,
    0x80, 0x90, 0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E,
};

#define SEG_0     0xC0
#define SEG_1     0xF9
#define SEG_BLANK 0xFF

// 全局变量
volatile u8 g_sw_value = 0;         // 保存的开关值
volatile u8 g_display_mode = MODE_BINARY;  // 当前显示模式
volatile u8 g_seg_buffer[8];        // 段码缓冲
volatile u8 g_update_flag = 0;      // 主循环更新标志

// 根据模式和开关值更新段码缓冲
static void compute_seg_buffer(void) {
  u8 val = g_sw_value;
  u8 mode = g_display_mode;

  if (mode == MODE_BINARY) {
    for (int i = 0; i < 8; i++) {
      g_seg_buffer[i] = (val & (1 << i)) ? SEG_1 : SEG_0;
    }
  } else if (mode == MODE_HEX) {
    g_seg_buffer[0] = SEG_TABLE[val & 0x0F];
    g_seg_buffer[1] = SEG_TABLE[(val >> 4) & 0x0F];
    for (int i = 2; i < 8; i++) g_seg_buffer[i] = SEG_BLANK;
  } else {  // MODE_DEC
    g_seg_buffer[0] = SEG_TABLE[val % 10];
    val /= 10;
    g_seg_buffer[1] = (val > 0) ? SEG_TABLE[val % 10] : SEG_BLANK;
    val /= 10;
    g_seg_buffer[2] = (val > 0) ? SEG_TABLE[val % 10] : SEG_BLANK;
    for (int i = 3; i < 8; i++) g_seg_buffer[i] = SEG_BLANK;
  }
}

// 延时函数: 维持数码管亮起，同时输出 LED (本任务不使用 LED)
static void DelayMs(u32 ms) {
  for (u32 i = 0; i < ms * 150; i++) {
    for (volatile int j = 0; j < 10; j++)
      ;
  }
}

// 普通中断服务函数
void ButtonISR(void) __attribute__((interrupt_handler));

void ButtonISR(void) {
  u32 btn_raw = Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F;
  u8 pressed = (u8)(btn_raw ^ 0x1F);

  if (pressed & BTN_C) {
    g_sw_value = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
    g_display_mode = MODE_BINARY;
    g_update_flag = 1;
  } else if (pressed & BTN_U) {
    g_sw_value = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
    g_display_mode = MODE_HEX;
    g_update_flag = 1;
  } else if (pressed & BTN_D) {
    g_sw_value = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
    g_display_mode = MODE_DEC;
    g_update_flag = 1;
  }

  // 清除 GPIO 中断状态
  Xil_Out32(GPIO2_BASE + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
  // 清除 INTC 中断状态
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, GPIO2_INTC_MASK);
}

int main() {
  // GPIO 方向配置
  Xil_Out32(GPIO0_BASE + XGPIO_TRI_OFFSET, 0xFF);
  Xil_Out32(GPIO1_BASE + XGPIO_TRI_OFFSET, 0x00);
  Xil_Out32(GPIO1_BASE + XGPIO_TRI2_OFFSET, 0x00);
  Xil_Out32(GPIO2_BASE + XGPIO_TRI_OFFSET, 0xFF);

  // 初始段码全熄灭
  for (int i = 0; i < 8; i++) g_seg_buffer[i] = SEG_BLANK;

  // 配置 GPIO_2 中断
  Xil_Out32(GPIO2_BASE + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
  Xil_Out32(GPIO2_BASE + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
  Xil_Out32(GPIO2_BASE + XGPIO_GIE_OFFSET, XGPIO_GIE_GINTR_ENABLE_MASK);

  // 配置 INTC (普通中断)
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, 0xFFFFFFFF);
  Xil_Out32(INTC_BASE + XIN_IER_OFFSET, GPIO2_INTC_MASK);
  Xil_Out32(INTC_BASE + XIN_MER_OFFSET,
            XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

  // 开启 MicroBlaze 中断
  microblaze_enable_interrupts();

  // 主循环: 动态扫描数码管
  while (1) {
    // ISR 设置了更新标志，在临界区内更新段码缓冲
    if (g_update_flag) {
      g_update_flag = 0;
      compute_seg_buffer();
    }

    for (int digit = 0; digit < 8; digit++) {
      // 位选
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, (1 << digit));
      // 段码
      Xil_Out32(GPIO1_BASE + XGPIO_DATA_OFFSET, g_seg_buffer[digit]);

      // 延时维持显示
      DelayMs(2);

      // 消隐
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, 0x00);
    }
  }

  return 0;
}
