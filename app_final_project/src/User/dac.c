#include "dac.h"
#include "xil_io.h"
#include "xparameters.h"
#include "xspi_l.h"

// SPI 基地址 (对应 AXI Quad SPI 1)
#define DAC_SPI_BASEADDR    XPAR_AXI_QUAD_SPI_1_BASEADDR

// 等待 TX FIFO 不满后写入 16 位数据
static void dac_spi_send(uint16_t frame) {
  // 等待 TX FIFO 不满 (SR bit 1 == 0)
  while (Xil_In32(DAC_SPI_BASEADDR + XSP_SR_OFFSET) & XSP_SR_TX_FULL_MASK);
  Xil_Out32(DAC_SPI_BASEADDR + XSP_DTR_OFFSET, frame);
}

// 写 DAC A (R1=1, R0=0): 更新 A 输出, 同时 B 从缓冲区更新
void dac_write_A(uint16_t data) {
  uint16_t frame = DAC_DACA_FAST | (data & DAC_DATA_MASK);
  dac_spi_send(frame);
}

// 写 DAC B (R1=0, R0=0): 直接更新 B 输出
void dac_write_B(uint16_t data) {
  uint16_t frame = DAC_DACB_FAST | (data & DAC_DATA_MASK);
  dac_spi_send(frame);
}

// 同步更新双通道
// 1. 先将 DAC B 的值写入缓冲区 (R1=0,R0=1), B 输出暂不更新
// 2. 再写 DAC A (R1=1,R0=0), A 更新输出的同时 B 从缓冲区自动更新
void dac_write_both(uint16_t data_a, uint16_t data_b) {
  // 第一步: 预装 B 到缓冲区
  uint16_t frame_buf = DAC_BUF_FAST | (data_b & DAC_DATA_MASK);
  dac_spi_send(frame_buf);

  // 第二步: 写 A 并触发 B 同步更新
  uint16_t frame_a = DAC_DACA_FAST | (data_a & DAC_DATA_MASK);
  dac_spi_send(frame_a);
}
