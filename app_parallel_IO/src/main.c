#include "sleep.h"
#include "xgpio_l.h"
#include "xil_io.h"

// 自定义共阳数码管的字母段码 
#define SEG_C 0xC6     // 字母 C
#define SEG_U 0xC1     // 字母 U
#define SEG_L 0xC7     // 字母 L
#define SEG_D 0xA1     // 字母 d (小写更易分辨)
#define SEG_R 0xAF     // 字母 r (小写更易分辨)
#define SEG_BLANK 0xFF // 全熄灭（初始状态）

int main() {
  // 开关初始化
  unsigned short last_sw, current_sw;
  Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET, 0xffff);

  // 按键初始化
  char button;
  Xil_Out8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET, 0X1f);

  // led初始化
  unsigned short led;
  unsigned char byte;
  Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);

  // 数码管初始化
  char segcode[8] = {0xc0, 0xf9, 0xa4, 0xb0, 0x99, 0x92, 0x82, 0xf8};
  uint8_t pos = 0x7f;
  Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET, 0x0);
  Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);

  uint8_t current_seg_pattern = SEG_BLANK;

  while (1) {

    for (int i = 0; i < 8; i++) {
      pos = (1 << i);        

      // 【步骤 A】更新当前数码管的扫描状态
      Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,
               current_seg_pattern);
      Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, pos);

      // 【步骤 B】实时读取独立开关状态，并同步更新到 LED 灯上
      uint16_t sw_state =
          Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);
      Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, sw_state);

      // 【步骤 C】非阻塞式读取独立按键值，更新段码
      uint8_t btn_raw =
          Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1F;

      if (btn_raw != 0x1F) { // 检测到有按键按下（不等于常态 0x1F）
        // 转换为高电平有效形式，方便进行位与(&)判断
        uint8_t pressed_mask = btn_raw ^ 0x1F;

        // 根据你板子的具体管脚绑定(.xdc)，按下述掩码映射调整字符对应关系
        if (pressed_mask & 0x01) {
          current_seg_pattern = SEG_U; 
        } else if (pressed_mask & 0x02) {
          current_seg_pattern = SEG_L; 
        } else if (pressed_mask & 0x04) {
          current_seg_pattern = SEG_C; 
        } else if (pressed_mask & 0x08) {
          current_seg_pattern = SEG_R; 
        } else if (pressed_mask & 0x10) {
          current_seg_pattern = SEG_D; 
        }
      }

      usleep(2000);
    }
  }
  //     last_sw = current_sw;
  //     current_sw =
  //         Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET) & 0xffff;
  //     if (last_sw != current_sw) {
  //       xil_printf("The switches' code is 0x%x\r\n", current_sw);
  //     }

  //     while ((Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f)
  //     !=
  //            0x1f) {
  //       button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) &
  //       0x1f; while ((Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET)
  //       & 0x1f) !=
  //              0x1f)
  //         ;
  //       xil_printf("The pushed button's code is 0x%x\n", button);
  //     }

  //     xil_printf("input the 16 bits hexadecimal number to be
  //     displayed:\n"); led = 0; while (1) {
  //       byte = inbyte();
  //       if (byte == 0x0d) {
  //         break;
  //       } else {
  //         if (byte >= 'a')
  //           byte = byte - 0x57;
  //         else if (byte >= 'A')
  //           byte = byte - 0x37;
  //         else
  //           byte = byte - 0x30;
  //         led = (led << 4) + byte;
  //       }
  //     }
  //     Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, led);

  //     while (1) {
  //       for (int i = 0; i < 8; i++) {
  //         pos = (1 << i);
  //         Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,
  //         segcode[i]);
  //         Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR +
  //         XGPIO_DATA2_OFFSET, pos); usleep(2000);
  //       }
  //     }
  //   }
}