#include "xgpio.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xintc.h"
#include "xparameters.h"

#define SEG_C 0xC6     // 字母 C
#define SEG_U 0xC1     // 字母 U
#define SEG_L 0xC7     // 字母 L
#define SEG_D 0xA1     // 字母 d (小写更易分辨)
#define SEG_R 0xAF     // 字母 r (小写更易分辨)
#define SEG_BLANK 0xFF // 全熄灭（初始状态）

// 全局外设实例
XGpio Gpio0; // SW & LED
XGpio Gpio1; // SEG & AN
XGpio Gpio2; // BTN

XIntc Intc; // 中断控制器

volatile u8 current_seg_code = SEG_BLANK; // 当前需要显示的段码

// 提示要求的延时刷新函数：在延时过程中实时同步开关与 LED
void DelayAndRefresh(u32 ms) {
  for (u32 i = 0; i < ms * 150; i++) { // 粗略延时计数，可根据主频微调
    // 读取开关值 (GPIO_0 通道 1)
    u32 sw_val = XGpio_DiscreteRead(&Gpio0, 1);
    // 更新 LED 灯 (GPIO_0 通道 2)
    XGpio_DiscreteWrite(&Gpio0, 2, sw_val);

    // 极微小的级联延迟
    for (volatile int j = 0; j < 10; j++)
      ;
  }
}
// 1. 快速中断服务函数声明与定义 (带有快速中断属性，无参数)
void ButtonFastHandler(void) __attribute__((fast_interrupt));

void ButtonFastHandler(void) {
  // 直接读取全局外设实例 Gpio2
  u32 btn_val = XGpio_DiscreteRead(&Gpio2, 1);

  uint8_t pressed_mask = btn_val ^ 0x1F;

  // 根据你板子的具体管脚绑定(.xdc)，按下述掩码映射调整字符对应关系
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

  // 清除 GPIO 中断状态
  XGpio_InterruptClear(&Gpio2, XGPIO_IR_CH1_MASK);
}

int main() {
  // 2. 初始化 GPIO 外设
  XGpio_Initialize(&Gpio0, XPAR_AXI_GPIO_0_BASEADDR);
  XGpio_Initialize(&Gpio1, XPAR_AXI_GPIO_1_BASEADDR);
  XGpio_Initialize(&Gpio2, XPAR_AXI_GPIO_2_BASEADDR);

  XGpio_SetDataDirection(&Gpio0, 1, 0xFF);
  XGpio_SetDataDirection(&Gpio0, 2, 0x00);
  XGpio_SetDataDirection(&Gpio1, 1, 0x00);
  XGpio_SetDataDirection(&Gpio1, 2, 0x00);
  XGpio_SetDataDirection(&Gpio2, 1, 0xFF);

  // 3. 开启 GPIO 外设中断
  XGpio_InterruptEnable(&Gpio2, XGPIO_IR_CH1_MASK);
  XGpio_InterruptGlobalEnable(&Gpio2);

  // 4. 初始化并配置 INTC
  XIntc_Initialize(&Intc, XPAR_XINTC_0_BASEADDR);

  // 【核心变化】使用 ConnectFastHandler 注册快速中断，不需要传实例指针参数
  XIntc_ConnectFastHandler(&Intc, XPAR_FABRIC_AXI_GPIO_2_INTR,
                           (XFastInterruptHandler)ButtonFastHandler);

  XIntc_Start(&Intc, XIN_REAL_MODE);
  XIntc_Enable(&Intc, XPAR_FABRIC_AXI_GPIO_2_INTR);

  // 5. 初始化 MicroBlaze 异常处理
  Xil_ExceptionInit();
  Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
                               (Xil_ExceptionHandler)XIntc_InterruptHandler,
                               &Intc);
  Xil_ExceptionEnable();

  // 6. 主循环：数码管动态扫描主体
  while (1) {
    for (int digit = 0; digit < 8; digit++) {
      XGpio_DiscreteWrite(&Gpio1, 2, (1 << digit));
      XGpio_DiscreteWrite(&Gpio1, 1, current_seg_code);

      // 延时中包含开关->LED 实时联动
      DelayAndRefresh(2);

      XGpio_DiscreteWrite(&Gpio1, 2, 0x00);
    }
  }
  return 0;
}