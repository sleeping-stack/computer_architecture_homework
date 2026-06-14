/*
 * 任务1 — 普通中断版本 (v2)
 * 功能:
 *   BTNC: 读开关 → 存为 data1 → 显示到 LED
 *   BTNR: 读开关 → 存为 data2 → 显示到 LED
 *   BTNU: data1 + data2 → 显示到 LED (低 8 位)
 *   BTND: data1 * data2 → 显示到 LED (低 8 位)
 * 控制方式: GPIO_2 按键中断驱动，ISR 中处理按键逻辑
 *           普通中断: ISR 清除 GPIO ISR + INTC IAR
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

// 普通中断服务函数
void ButtonISR(void) __attribute__((interrupt_handler));

void ButtonISR(void) {
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

  // 清除 GPIO 中断状态 (必须清除，否则持续触发)
  Xil_Out32(GPIO2_BASE + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);

  // 清除 INTC 中断状态
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, GPIO2_INTC_MASK);
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

  // 配置 INTC (普通中断，不需要 IMR/IVAR)
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, 0xFFFFFFFF);
  Xil_Out32(INTC_BASE + XIN_IER_OFFSET, GPIO2_INTC_MASK);
  Xil_Out32(INTC_BASE + XIN_MER_OFFSET,
            XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

  // 开启 MicroBlaze 中断
  microblaze_enable_interrupts();

  // 主循环: 持续将 LED 值输出，同时同步开关到 LED 便于观察输入
  while (1) {
    Xil_Out32(GPIO0_BASE + XGPIO_DATA2_OFFSET, g_led_value);
    // 短暂延时防止过于频繁的总线访问
    for (volatile int i = 0; i < 1000; i++) {
      __asm__("nop");
    }
  }

  return 0;
}
