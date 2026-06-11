/**
 * kb_display.h
 * 按键 + 数码管 + LED + 拨码开关 外设模块接口
 * 平台: AC850开发板 (MicroBlaze, XC7Z020)
 *
 * 作为 app_final_project 的子模块，提供:
 *   - GPIO0 Ch1: SW拨码开关输入 (波形选择)
 *   - GPIO0 Ch2: LED输出 (通道指示)
 *   - GPIO1:     8位数码管显示 (频率+幅度)
 *   - GPIO3:     4x4矩阵键盘输入
 */

#ifndef KB_DISPLAY_H
#define KB_DISPLAY_H

#include "xil_types.h"

/* 初始化: GPIO + INTC 中断注册 + 默认LED */
void kb_display_init(void);

/* 主循环调用: 检查并处理按键事件 */
void kb_display_poll(void);

/* 定时器 ISR 中调用 (100kHz): 数码管动态扫描 (内部分频到 ~100Hz) */
void kb_display_tick(void);

/* GPIO3 键盘中断 ISR */
void key_scan_isr(void);

/* GPIO0 开关中断 ISR */
void sw_isr(void);

/* UART 改变参数后调用: 同步 LED 和数码管到当前通道 */
void display_sync_from_uart(uint8_t ch);

/* GPIO2 模式按键中断 ISR: 切换独立/同步输出模式 */
void mode_btn_isr(void);
#endif
