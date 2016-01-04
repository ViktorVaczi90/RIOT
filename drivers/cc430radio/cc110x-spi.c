/*
 * Copyright (C) 2014 Freie Universität Berlin
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_cc430radio
 * @{
 *
 * @file
 * @brief       TI Chipcon CC110x spi driver
 *
 * @author      Thomas Hillebrandt <hillebra@inf.fu-berlin.de>
 * @author      Heiko Will <hwill@inf.fu-berlin.de>
 * @author      Fabian Nack <nack@inf.fu-berlin.de>
 * @author      Joakim Gebart <joakim.gebart@eistec.se>
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @}
 */

#include <stdio.h>

#include "cc430radio.h"
#include "cc430radio/cc430radio-spi.h"
#include "cc430radio/cc430radio-internal.h"
#include "cc430radio/cc430radio-defines.h"

#include "periph/gpio.h"
#include "periph/spi.h"

#include "xtimer.h"
#include "irq.h"

/**********************************************************************
 *                      CC110x spi access
 **********************************************************************/

void cc430radio_cs(cc430radio_t *dev)
{
    volatile int retry_count = 0;
    /* Switch MISO/GDO1 to GPIO input mode */
#ifndef GPIO_READS_SPI_PINS
    gpio_init(dev->params.gdo1, GPIO_DIR_IN, GPIO_NOPULL);
#endif
    /* CS to low */
    gpio_clear(dev->params.cs);
    /* Wait for SO to go low (voltage regulator
     * has stabilized and the crystal is running) */
    while (gpio_read(dev->params.gdo1)) {
        /* Wait ~500us and try again */
        xtimer_usleep(CS_SO_WAIT_TIME);

        if (gpio_read(dev->params.gdo1)) {
            retry_count++;

            if (retry_count > CC430RADIO_GDO1_LOW_RETRY) {
                puts("[CC430RADIO spi] fatal error\n");
                break;
            }

            gpio_set(dev->params.cs);
            gpio_clear(dev->params.cs);
        }
    }
    /* Switch MISO/GDO1 to spi mode */
#ifndef GPIO_READS_SPI_PINS
    spi_conf_pins(dev->params.spi);
#endif
}

void cc430radio_writeburst_reg(cc430radio_t *dev, uint8_t addr, const char *src, uint8_t count)
{
    unsigned int cpsr;
    spi_acquire(dev->params.spi);
    cpsr = disableIRQ();
    cc430radio_cs(dev);
    spi_transfer_regs(dev->params.spi, addr | CC430RADIO_WRITE_BURST, (char *)src, 0, count);
    gpio_set(dev->params.cs);
    restoreIRQ(cpsr);
    spi_release(dev->params.spi);
}

void cc430radio_readburst_reg(cc430radio_t *dev, uint8_t addr, char *buffer, uint8_t count)
{
    int i = 0;
    unsigned int cpsr;
    spi_acquire(dev->params.spi);
    cpsr = disableIRQ();
    cc430radio_cs(dev);
    spi_transfer_byte(dev->params.spi, addr | CC430RADIO_READ_BURST, 0);
    while (i < count) {
        spi_transfer_byte(dev->params.spi, CC430RADIO_NOBYTE, &buffer[i]);
        i++;
    }
    gpio_set(dev->params.cs);
    restoreIRQ(cpsr);
    spi_release(dev->params.spi);
}

void cc430radio_write_reg(cc430radio_t *dev, uint8_t addr, uint8_t value)
{
    unsigned int cpsr;
    spi_acquire(dev->params.spi);
    cpsr = disableIRQ();
    cc430radio_cs(dev);
    spi_transfer_reg(dev->params.spi, addr, value, 0);
    gpio_set(dev->params.cs);
    restoreIRQ(cpsr);
    spi_release(dev->params.spi);
}

uint8_t cc430radio_read_reg(cc430radio_t *dev, uint8_t addr)
{
    char result;
    unsigned int cpsr;
    spi_acquire(dev->params.spi);
    cpsr = disableIRQ();
    cc430radio_cs(dev);
    spi_transfer_reg(dev->params.spi, addr | CC430RADIO_READ_SINGLE, CC430RADIO_NOBYTE, &result);
    gpio_set(dev->params.cs);
    restoreIRQ(cpsr);
    spi_release(dev->params.spi);
    return (uint8_t) result;
}

uint8_t cc430radio_read_status(cc430radio_t *dev, uint8_t addr)
{
    char result;
    unsigned int cpsr;
    spi_acquire(dev->params.spi);
    cpsr = disableIRQ();
    cc430radio_cs(dev);
    spi_transfer_reg(dev->params.spi, addr | CC430RADIO_READ_BURST, CC430RADIO_NOBYTE, &result);
    gpio_set(dev->params.cs);
    restoreIRQ(cpsr);
    spi_release(dev->params.spi);
    return (uint8_t) result;
}

uint8_t cc430radio_get_reg_robust(cc430radio_t *dev, uint8_t addr)
{
    char result, result2;
    unsigned int cpsr;
    spi_acquire(dev->params.spi);
    cpsr = disableIRQ();
    cc430radio_cs(dev);
    do {
        spi_transfer_reg(dev->params.spi, addr | CC430RADIO_READ_BURST, CC430RADIO_NOBYTE, &result);
        spi_transfer_reg(dev->params.spi, addr | CC430RADIO_READ_BURST, CC430RADIO_NOBYTE, &result2);
    } while (result != result2);
    gpio_set(dev->params.cs);
    restoreIRQ(cpsr);
    spi_release(dev->params.spi);
    return (uint8_t) result;
}

uint8_t cc430radio_strobe(cc430radio_t *dev, uint8_t c)
{
#ifdef CC430RADIO_DONT_RESET
    if (c == CC430RADIO_SRES) {
        return 0;
    }
#endif

    char result;
    unsigned int cpsr;
    spi_acquire(dev->params.spi);
    cpsr = disableIRQ();
    cc430radio_cs(dev);
    spi_transfer_byte(dev->params.spi, c, &result);
    gpio_set(dev->params.cs);
    restoreIRQ(cpsr);
    spi_release(dev->params.spi);
    return (uint8_t) result;
}
