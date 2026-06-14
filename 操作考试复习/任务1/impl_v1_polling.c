/*
 * 任务1 — 轮询版本 (v1)
 * 功能:
 *   BTNC: 读开关 → 存为 data1 → 显示到 LED
 *   BTNR: 读开关 → 存为 data2 → 显示到 LED
 *   BTNU: data1 + data2 → 显示到 LED (低 8 位)
 *   BTND: data1 * data2 → 显示到 LED (低 8 位)
 * 控制方式: 主循环轮询按键，检测按下后等待释放完成去抖
 */

#include "xgpio_l.h"
#include "xil_io.h"

// 外设基地址
#define GPIO0_BASE XPAR_AXI_GPIO_0_BASEADDR  // SW(Ch1) & LED(Ch2)
#define GPIO2_BASE XPAR_AXI_GPIO_2_BASEADDR  // BTN(Ch1)

// 按键位掩码 (按下=0, 上拉=1)
#define BTN_U (1 << 0)  // BTNU
#define BTN_L (1 << 1)  // BTNL
#define BTN_C (1 << 2)  // BTNC
#define BTN_R (1 << 3)  // BTNR
#define BTN_D (1 << 4)  // BTND

// 短暂延时 (忙等)
static void delay_ms(u32 ms) {
  for (u32 i = 0; i < ms * 8000; i++) {
    __asm__("nop");
  }
}

int main() {
  // GPIO 方向配置
  Xil_Out32(GPIO0_BASE + XGPIO_TRI_OFFSET, 0xFF);   // SW 通道1: 输入
  Xil_Out32(GPIO0_BASE + XGPIO_TRI2_OFFSET, 0x00);   // LED 通道2: 输出
  Xil_Out32(GPIO2_BASE + XGPIO_TRI_OFFSET, 0xFF);    // BTN 通道1: 输入

  u8 data1 = 0;
  u8 data2 = 0;
  u8 led_value = 0;

  // 初始显示 data1=0, data2=0
  Xil_Out32(GPIO0_BASE + XGPIO_DATA2_OFFSET, 0x00);

  while (1) {
    // 轮询读取按键状态 (低 5 位有效)
    u32 btn_raw = Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F;

    // 没有按键按下时，同步开关状态到 LED 以便观察
    if (btn_raw == 0x1F) {
      Xil_Out32(GPIO0_BASE + XGPIO_DATA2_OFFSET, led_value);
      continue;
    }

    // 转换为高电平有效掩码 (按下为 1)
    u8 pressed = (u8)(btn_raw ^ 0x1F);

    if (pressed & BTN_C) {
      // BTNC: 读开关 → data1 → 显示
      data1 = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
      led_value = data1;
    } else if (pressed & BTN_R) {
      // BTNR: 读开关 → data2 → 显示
      data2 = (u8)(Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF);
      led_value = data2;
    } else if (pressed & BTN_U) {
      // BTNU: data1 + data2 → LED
      led_value = data1 + data2;
    } else if (pressed & BTN_D) {
      // BTND: data1 * data2 → LED (低 8 位)
      led_value = (u8)(data1 * data2);
    }

    // 更新 LED 显示
    Xil_Out32(GPIO0_BASE + XGPIO_DATA2_OFFSET, led_value);

    // 去抖: 等待按键释放
    while ((Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F) != 0x1F) {
      delay_ms(1);
    }
    // 释放后再等一小段，防止抖动
    delay_ms(10);
  }

  return 0;
}
