/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_msp430fxyz
 * @{
 *
 * @file
 * @brief       Low-level UART driver implementation
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */
#define pinUARTmap()      P1MAP6 = PM_UCA0TXD; P1MAP5 = PM_UCA0RXD
#define pinUARTconfig()   P1SEL |= BIT6 | BIT5      // Set P1.6 and P1.5 to USCI Mode
#define SYSTEM_CLK_FREQ 12000000L
#include "cpu.h"
#include "periph_cpu.h"
#include "periph_conf.h"
#include "periph/uart.h"
//#include <msp430.h>
//DEBUG
#include "cc430f5137.h"
/**
 * @brief   Keep track of the interrupt context
 * @{
 */
static uart_rx_cb_t ctx_rx_cb;
static void *ctx_isr_arg;
/** @} */
static int init_base(uart_t uart, uint32_t baudrate);
int uart_init(uart_t uart, uint32_t baudrate, uart_rx_cb_t rx_cb, void *arg)
{
    if (init_base(uart, baudrate) < 0) {
        return -1;
    }
    /* save interrupt context */
    ctx_rx_cb = rx_cb;
    ctx_isr_arg = arg;
    /* reset interrupt flags and enable RX interrupt */
    UCA0IFG &= ~(UCRXIE);
    UCA0IFG |= (UCTXIE);
    UCA0IE |=  (UCRXIE);
	UCA0IE &=  ~(UCTXIE);
    return 0;
}

static int init_base(uart_t uart, uint32_t baudrate)
{
	if (uart != 0) {
		return -1;
	}
	PMAPPWD = 0x02D52;                        // Get write-access to port mapping regs  
	P1MAP5 = PM_UCA0RXD;                      // Map UCA0RXD output to P1.5 
	P1MAP6 = PM_UCA0TXD;                      // Map UCA0TXD output to P1.6 
	PMAPPWD = 0;                              // Lock port mapping registers 
	
	P1DIR |= BIT6;                            // Set P1.6 as TX output
	P1SEL |= BIT5 + BIT6;                     // Select P1.5 & P1.6 to UART function
	
	UCA0CTL1 |= UCSWRST;                      // **Put state machine in reset**
	UCA0CTL1 |= UCSSEL_2;                     // SMCLK
	UCA0CTL0 = 0;
	UCA0STAT = 0;
	uint32_t base = ((CLOCK_CMCLK << 7)  / baudrate);
	uint16_t br = (uint16_t)(base >> 7);
	uint8_t brs = (((base & 0x3f) * 8) >> 7);
	UCA0BR0 = (uint8_t)br;                             // 1MHz 115200 (see User's Guide)
	UCA0BR1 = (uint8_t)(br >> 8);                             // 1MHz 115200
	UCA0MCTL = (brs << 1U);          // Modulation UCBRSx=1, UCBRFx=0

	UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
	return 0;
}

void uart_write(uart_t uart, const uint8_t *data, size_t len)
{
	(void)uart;
	for (size_t i = 0; i < len; i++) {
		while (!(UCA0IFG & UCTXIE));
			UCA0TXBUF = data[i];
	}
}

void uart_poweron(uart_t uart)
{
	(void)uart;
}

void uart_poweroff(uart_t uart)
{
	(void)uart;
}

__attribute__((interrupt(USCI_A0_VECTOR)))
void UART_ISR(void)
{
	__enter_isr();
	uint8_t stat = UCA0STAT;
	char data = (char)UCA0RXBUF;
	switch(UCA0IV)
	{
		case 2:
			break;                             // Vector 2 - RXIFG
		case 0:                                   
			if (stat & (UCFE | UCOE | UCPE | UCBRK)) { // Vector 0 - no interrupt?
					/* some error which we do not handle, just do a pseudo read to reset the
					 * status register */
				(void)data;
			}
			else {
				ctx_rx_cb(ctx_isr_arg, data);
			}
			break;
		case 4:
			break;                             // Vector 4 - TXIFG
		default: 
			break;
	}
	__exit_isr();
}