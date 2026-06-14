#include "mb_interface.h"
#include "xgpio_l.h"
#include "xil_io.h"
#include "xintc_l.h"
#include "xparameters.h"

#define SEG_C 0xC6     // 字母 C
#define SEG_U 0xC1     // 字母 U
#define SEG_L 0xC7     // 字母 L
#define SEG_D 0xA1     // 字母 d (小写更易分辨)
#define SEG_R 0xAF     // 字母 r (小写更易分辨)
#define SEG_BLANK 0xFF // 全熄灭（初始状态）

// 外设基地址
#define GPIO0_BASE XPAR_AXI_GPIO_0_BASEADDR // SW & LED
#define GPIO1_BASE XPAR_AXI_GPIO_1_BASEADDR // SEG & AN
#define GPIO2_BASE XPAR_AXI_GPIO_2_BASEADDR // BTN
#define INTC_BASE  XPAR_XINTC_0_BASEADDR    // 中断控制器

// GPIO_2 在 INTC 中的中断位掩码 (INT ID = 3)
#define GPIO2_INTC_MASK (1 << XPAR_FABRIC_AXI_GPIO_2_INTR)

volatile u8 current_seg_code = SEG_BLANK; // 当前需要显示的段码

// 提示要求的延时刷新函数：在延时过程中实时同步开关与 LED
void DelayAndRefresh(u32 ms) {
  for (u32 i = 0; i < ms * 150; i++) { // 粗略延时计数
    // 读取开关值 (GPIO_0 通道 1)
    u32 sw_val = Xil_In32(GPIO0_BASE + XGPIO_DATA_OFFSET);
    // 更新 LED 灯 (GPIO_0 通道 2)
    Xil_Out32(GPIO0_BASE + XGPIO_DATA2_OFFSET, sw_val);

    // 极微小的级联延迟
    for (volatile int j = 0; j < 10; j++)
      ;
  }
}

// 1. 普通中断服务函数
void ButtonISR(void) __attribute__((interrupt_handler));

void ButtonISR(void) {
  // 读取按键状态 (GPIO_2 通道 1)，仅低 5 位有效
  u32 btn_val = Xil_In32(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F;

  // 转换为高电平有效掩码（按键上拉，按下为 0）
  u8 pressed_mask = (u8)(btn_val ^ 0x1F);

  // 根据板子管脚绑定(.xdc)，按下述掩码映射调整字符对应关系
  if (pressed_mask & 0x01) {
    current_seg_code = SEG_U;
  } else if (pressed_mask & 0x02) {
    current_seg_code = SEG_L;
  } else if (pressed_mask & 0x04) {
    current_seg_code = SEG_C;
  } else if (pressed_mask & 0x08) {
    current_seg_code = SEG_R;
  } else if (pressed_mask & 0x10) {
    current_seg_code = SEG_D;
  }

  // 清除 GPIO 中断状态（必须清除，否则会持续触发）
  Xil_Out32(GPIO2_BASE + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);

  // 清除 INTC 中断状态
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, GPIO2_INTC_MASK);
}

int main() {
  // 2. 设置 GPIO 输入输出方向
  Xil_Out32(GPIO0_BASE + XGPIO_TRI_OFFSET, 0xFF);   // SW 为输入
  Xil_Out32(GPIO0_BASE + XGPIO_TRI2_OFFSET, 0x00);   // LED 为输出
  Xil_Out32(GPIO1_BASE + XGPIO_TRI_OFFSET, 0x00);    // SEG 段码为输出
  Xil_Out32(GPIO1_BASE + XGPIO_TRI2_OFFSET, 0x00);   // AN 位选为输出
  Xil_Out32(GPIO2_BASE + XGPIO_TRI_OFFSET, 0xFF);    // BTN 为输入

  // 3. 开启 GPIO 外设中断
  Xil_Out32(GPIO2_BASE + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK); // 先清除 GPIO 中断状态
  Xil_Out32(GPIO2_BASE + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK); // 使能通道 1 中断
  Xil_Out32(GPIO2_BASE + XGPIO_GIE_OFFSET,
            XGPIO_GIE_GINTR_ENABLE_MASK); // 使能 GPIO 全局中断输出

  // 4. 配置 INTC（普通中断方式，不使用 IMR）
  Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, 0xFFFFFFFF);           // 清除所有 INTC 中断状态
  Xil_Out32(INTC_BASE + XIN_IER_OFFSET, GPIO2_INTC_MASK);       // 使能 GPIO_2 对应的中断线
  Xil_Out32(INTC_BASE + XIN_MER_OFFSET,
            XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK); // 主使能

  // 5. 开启 MicroBlaze 中断
  microblaze_enable_interrupts();

  // 6. 主循环：动态扫描数码管
  while (1) {
    for (int digit = 0; digit < 8; digit++) {
      // 选择当前数码管位（高电平选中对应位）
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, (1 << digit));

      // 输出最新的全局键值段码
      Xil_Out32(GPIO1_BASE + XGPIO_DATA_OFFSET, current_seg_code);

      // 调用内嵌开关更新的延时函数（维持数码管亮起约 1-2ms 避免闪烁）
      DelayAndRefresh(2);

      // 消隐（防止鬼影）
      Xil_Out32(GPIO1_BASE + XGPIO_DATA2_OFFSET, 0x00);
    }
  }
  return 0;
}
