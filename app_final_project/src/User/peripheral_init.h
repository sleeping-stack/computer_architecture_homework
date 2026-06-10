#ifndef PERIPHERAL_INIT_H
#define PERIPHERAL_INIT_H

#include "xil_io.h"
#include "xparameters.h"
#include "xspi_l.h"
#include "xtmrctr_l.h"
#include "xuartlite_l.h"
#include "xintc_l.h"

void spi_init(void);
void timer_init(uint32_t freq);
void uart_init(void);
void interrupt_init(void);

#endif