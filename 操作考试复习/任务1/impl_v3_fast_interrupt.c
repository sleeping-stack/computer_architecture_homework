/*
 * 任务1 — 快速中断版本 (v3)
 * 功能:
 *   BTNC: 读开关 → 存为 data1 → 显示到 LED
 *   BTNR: 读开关 → 存为 data2 → 显示到 LED
 *   BTNU: data1 + data2 → 显示到 LED (低 8 位)
 *   BTND: data1 * data2 → 显示到 LED (低 8 位)
 * 控制方式: GPIO_2 按键快速中断驱动
 *           快速中断: 配置 IMR + IVAR，ISR 仅清除 GPIO ISR
 *                     INTC 硬件自动应答，无需手动清除 IAR
 */

#include "mb_interface.h"
#include "xgpio_l.h"
#include "xil_io.h"
#include "xintc_l.h"
#include "xparameters.h"

// 外设基地址
#define GPIO0_BASE XPAR_AXI_GPIO_0_BASEADDR  // SW(Ch1) & LED(Ch2)
#define GPIO2_BASE XPAR_AXI_GPIO_2_BASEADDR  // BTN(Ch1)
#define INTC_BASE  XPAR_XINTC_0_BASEADDR     // 中断控制器

// GPIO_2 在 INTC 中的中断位掩码 (INT ID = 3)
#define GPIO2_INTC_MASK (1 << XPAR_FABRIC_AXI_GPIO_2_INTR)

// 按键位掩码 (按下=0, 上拉=1)
#define BTN_U (1 << 0)  // BTNU
#define BTN_L (1 << 1)  // BTNL
#define BTN_C (1 << 2)  // BTNC
#define BTN_R (1 << 3)  // BTNR
#define BTN_D (1 << 4)  // BTND

// 全局变量: 保存的两个数据和 LED 输出值
volatile u8 g_data1 = 0;
volatile u8 g_data2 = 0;
volatile u8 g_led_value = 0;

// 快速中断服务函数
void ButtonFastHandler(void) __attribute__((fast_interrupt));

void ButtonFastHandler(void) {
  // 读取按键状态 (低 5 位有效)
  u32 btn_raw = Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F;

  // 转换为高电平有效掩码
  u8 pressed = (u8)(btn_raw ^ 0x1F);

  // 读取开关当前值
  u8 sw_val = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);

  if (pressed & BTN_C) {
    // BTNC: 读开关 → data1 → 显示
    g_data1 = sw_val;
    g_led_value = g_data1;
  } else if (pressed & BTN_R) {
    // BTNR: 读开关 → data2 → 显示
    g_data2 = sw_val;
    g_led_value = g_data2;
  } else if (pressed & BTN_U) {
    // BTNU: data1 + data2 → LED
    g_led_value = g_data1 + g_data2;
  } else if (pressed & BTN_D) {
    // BTND: data1 * data2 → LED (低 8 位)
    g_led_value = (u8)(g_data1 * g_data2);
  }

  // 快速中断：仅清除 GPIO ISR (INTC 硬件自动应答)
  Xil_Out32(GPIO2_BASE + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
}

int main() {
  // GPIO 方向配置
  Xil_Out32(GPIO0_BASE + XGPIO_TRI_OFFSET, 0xFF);   // SW 通道1: 输入
  Xil_Out32(GPIO0_BASE + XGPIO_TRI2_OFFSET, 0x00);   // LED 通道2: 输出
  Xil_Out32(GPIO2_BASE + XGPIO_TRI_OFFSET, 0xFF);    // BTN 通道1: 输入

  // 初始显示
  Xil_Out32(GPIO0_BASE + XGPIO_DATA2_OFFSET, 0x00);

  // 配置 GPIO_2 中断
  Xil_Out32(GPIO2_BASE + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
  Xil_Out32(GPIO2_BASE + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
  Xil_Out32(GPIO2_BASE + XGPIO_GIE_OFFSET, XGPIO_GIE_GINTR_ENABLE_MASK);

  // 配置 INTC (快速中断: 需要 IMR + IVAR)
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, 0xFFFFFFFF);
  Xil_Out32(INTC_BASE + XIN_IER_OFFSET, GPIO2_INTC_MASK);
  Xil_Out32(INTC_BASE + XIN_MER_OFFSET,
            XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);
  // 快速中断特有: 设置中断模式寄存器
  Xil_Out32(INTC_BASE + XIN_IMR_OFFSET, GPIO2_INTC_MASK);
  // 快速中断特有: 写入中断向量地址 (IVAR + INT_ID * 4)
  Xil_Out32(INTC_BASE + XIN_IVAR_OFFSET + XPAR_FABRIC_AXI_GPIO_2_INTR * 4,
            (u32)ButtonFastHandler);

  // 开启 MicroBlaze 中断
  microblaze_enable_interrupts();

  // 主循环: 持续将 LED 值输出
  while (1) {
    Xil_Out32(GPIO0_BASE + XGPIO_DATA2_OFFSET, g_led_value);
    // 短暂延时防止过于频繁的总线访问
    for (volatile int i = 0; i < 1000; i++) {
      __asm__("nop");
    }
  }

  return 0;
}
