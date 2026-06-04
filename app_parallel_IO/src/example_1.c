// 这个文件用于测试第一个教学视频中实现的各个功能，使用时请注释没有使用到的代码
#include "sleep.h"
#include "xgpio_l.h"
#include "xil_io.h"

int main() {
  // 开关初始化
  unsigned short last_sw, current_sw;
  Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET, 0xffff);

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

  while (1) {
    // 测试按钮
    {
      last_sw = current_sw;
      current_sw =
          Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET) & 0xffff;
      if (last_sw != current_sw) {
        xil_printf("The switches' code is 0x%x\r\n", current_sw);
      }
    }
    // 测试拨码开关
    {
      while ((Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f) !=
             0x1f) {
        button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;
        while ((Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f) !=
               0x1f)
          ;
        xil_printf("The pushed button's code is 0x%x\n", button);
      }
    }
    // 测试led灯
    {
      xil_printf("input the 16 bits hexadecimal number to be displayed:\n");
      led = 0;
      while (1) {
        byte = inbyte();
        if (byte == 0x0d) {
          break;
        } else {
          if (byte >= 'a')
            byte = byte - 0x57;
          else if (byte >= 'A')
            byte = byte - 0x37;
          else
            byte = byte - 0x30;
          led = (led << 4) + byte;
        }
      }
      Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, led);
    }
    // 测试数码管
    {
      while (1) {
        for (int i = 0; i < 8; i++) {
          pos = (1 << i);
          Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, segcode[i]);
          Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, pos);
          usleep(2000);
        }
      }
    }
  }
}