#include "dac.h"
#include "xil_io.h"
#include "xparameters.h"
#include "xspi_l.h"

// 阻塞 SPI 写:
// 自动片选模式下硬件自动在传输时拉低 CS、空闲时拉高 CS,
// TLV5618 在 CS↑ 时锁存数据到 DAC 输出。
// 写入前等 TX_EMPTY 确保上一帧已完成 (CS 已拉高),
// 写入后等 TX_EMPTY 确保本帧已完成 (CS 已拉高), 再返回供下一帧使用。
static void dac_spi_send(uint16_t frame) {
  // 等待上一帧传输完成 (TX FIFO + 移位寄存器全空, CS 已自动拉高)
  while (!(Xil_In32(XPAR_AXI_QUAD_SPI_1_BASEADDR + XSP_SR_OFFSET) &
           XSP_SR_TX_EMPTY_MASK)) {
  }
  // 写入 DTR, 硬件自动拉低 CS 并开始传输 (12.5MHz, 16-bit ≈ 1.3µs)
  Xil_Out32(XPAR_AXI_QUAD_SPI_1_BASEADDR + XSP_DTR_OFFSET, frame);

  // 等待本帧传输完成 (CS 自动拉高, TLV5618 锁存输出)
  while (!(Xil_In32(XPAR_AXI_QUAD_SPI_1_BASEADDR + XSP_SR_OFFSET) &
           XSP_SR_TX_EMPTY_MASK)) {
  }
}

// 单通道写: value=12-bit数据, ctrl=控制字 (DAC_CTRL_A 或 DAC_CTRL_B)
void dac_write_ch(uint16_t value, uint16_t ctrl) {
  uint16_t frame = ctrl | (value & DAC_DATA_MASK);
  dac_spi_send(frame);
}

// 写 DAC A (R1=1,R0=0): 更新 A 输出, 同时 B 从缓冲区更新
void dac_write_A(uint16_t data) { dac_write_ch(data, DAC_DACA_FAST); }

// 写 DAC B (R1=0,R0=0): 直接更新 B 输出 + 更新缓冲区
void dac_write_B(uint16_t data) { dac_write_ch(data, DAC_DACB_FAST); }

// 同步双通道更新: 快速模式 (SPD=1) 两步协议
// Step 1: 将 B 通道数据写入 BUFFER (不改变输出)
// Step 2: 写入 A 通道数据, 同时将 BUFFER 内容转移到 B 输出
//         两个通道在 Step 2 的 D0 上升沿同步更新 (~3µs 建立, 适配 20µs ISR)
//
// 两帧之间需要 CS 上升沿来锁存 Step 1 的数据到 BUFFER。
// 自动片选模式下 TX_EMPTY 仅表示 TX FIFO 为空,
// 移位寄存器可能仍在发送 (CS 仍为低电平),
// 因此两帧之间必须加延时等待移位完成 + CS 自动拉高。
void dac_write_both(uint16_t data_a, uint16_t data_b) {
  dac_write_ch(data_b, DAC_BUF_FAST);  // Step 1: B→BUFFER, 不改变输出

  // 等待 Step 1 移位完成 + CS 拉高:
  // 16 SCLK × 80ns = 1.28µs, 留余量 ~2µs
  // 确保 CS 已经拉高，Step 1 的数据已锁存到 BUFFER，才能进行 Step 2 的同步更新。
  for (volatile int i = 0; i < 5; i++) {
  }

  dac_write_ch(data_a, DAC_DACA_FAST); // Step 2: A + BUFFER→B 同步触发
}

