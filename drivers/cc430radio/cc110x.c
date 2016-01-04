/*
 * Copyright (C) 2014 Freie Universität Berlin
 * Copyright (C) 2013 INRIA
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_cc430radio
 * @{
 * @file
 * @brief       Basic functionality of cc430radio driver
 *
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 * @author      Fabian Nack <nack@inf.fu-berlin.de>
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @}
 */

#include "board.h"
#include "periph/cpuid.h"
#include "periph/gpio.h"
#include "periph/spi.h"
#include "xtimer.h"
#include "cpu.h"
#include "log.h"

#include "cc430radio.h"
#include "cc430radio/cc430radio-defaultsettings.h"
#include "cc430radio/cc430radio-defines.h"
#include "cc430radio/cc430radio-interface.h"
#include "cc430radio/cc430radio-internal.h"
#include "cc430radio/cc430radio-spi.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

/* Internal function prototypes */
#ifndef CC430RADIO_DONT_RESET
static void _reset(cc430radio_t *dev);
static void _power_up_reset(cc430radio_t *dev);
#endif

int cc430radio_setup(cc430radio_t *dev, const cc430radio_params_t *params)
{
    DEBUG("%s:%s:%u\n", RIOT_FILE_RELATIVE, __func__, __LINE__);

#ifdef MODULE_CC430RADIO_HOOKS
    cc430radio_hooks_init();
#endif

    dev->params = *params;

    /* Configure chip-select */
    gpio_init(dev->params.cs, GPIO_DIR_OUT, GPIO_NOPULL);
    gpio_set(dev->params.cs);

    /* Configure GDO1 */
    gpio_init(dev->params.gdo1, GPIO_DIR_IN, GPIO_NOPULL);

    /* Configure SPI */
    spi_acquire(dev->params.spi);
    spi_init_master(dev->params.spi, SPI_CONF_FIRST_RISING, SPI_SPEED_5MHZ);
    spi_release(dev->params.spi);

#ifndef CC430RADIO_DONT_RESET
    /* reset device*/
    _power_up_reset(dev);
#endif

    /* set default state */
    dev->radio_state = RADIO_IDLE;

    /* Write configuration to configuration registers */
    cc430radio_writeburst_reg(dev, 0x00, cc430radio_default_conf, cc430radio_default_conf_size);

    /* Write PATABLE (power settings) */
    cc430radio_writeburst_reg(dev, CC430RADIO_PATABLE, CC430RADIO_DEFAULT_PATABLE, 8);

    /* set base frequency */
    cc430radio_set_base_freq_raw(dev, CC430RADIO_DEFAULT_FREQ);

    /* Set default channel number */
    cc430radio_set_channel(dev, CC430RADIO_DEFAULT_CHANNEL);

    /* set default node id */
#ifdef CPUID_ID_LEN
    if (CPUID_ID_LEN>0) {
        char cpuid[CPUID_ID_LEN];
        cpuid_get(cpuid);
        cc430radio_set_address(dev, (uint8_t) cpuid[CPUID_ID_LEN-1]);
    }
#endif

    LOG_INFO("cc430radio: initialized with address=%u and channel=%i\n",
            (unsigned)dev->radio_address,
            dev->radio_channel);

    return 0;
}

uint8_t cc430radio_set_address(cc430radio_t *dev, uint8_t address)
{
    DEBUG("%s:%s:%u setting address %u\n", RIOT_FILE_RELATIVE, __func__,
            __LINE__, (unsigned)address);
    if (!(address < MIN_UID) || (address > MAX_UID)) {
        if (dev->radio_state != RADIO_UNKNOWN) {
            cc430radio_write_register(dev, CC430RADIO_ADDR, address);
            dev->radio_address = address;
            return address;
        }
    }

    return 0;
}

void cc430radio_set_base_freq_raw(cc430radio_t *dev, const char* freq_array)
{
#if ENABLE_DEBUG == 1
    uint8_t _tmp[] = { freq_array[2], freq_array[1], freq_array[0], 0x00};
    uint32_t *FREQ = (uint32_t*) _tmp;

    DEBUG("cc430radio_set_base_freq_raw(): setting base frequency to %uHz\n",
            (26000000>>16) * (unsigned)(*FREQ));
#endif
    cc430radio_writeburst_reg(dev, CC430RADIO_FREQ2, freq_array, 3);
}

void cc430radio_set_monitor(cc430radio_t *dev, uint8_t mode)
{
    DEBUG("%s:%s:%u\n", RIOT_FILE_RELATIVE, __func__, __LINE__);

    cc430radio_write_register(dev, CC430RADIO_PKTCTRL1, mode ? 0x04 : 0x06);
}

void cc430radio_setup_rx_mode(cc430radio_t *dev)
{
    DEBUG("%s:%s:%u\n", RIOT_FILE_RELATIVE, __func__, __LINE__);

    /* Stay in RX mode until end of packet */
    cc430radio_write_reg(dev, CC430RADIO_MCSM2, 0x07);
    cc430radio_switch_to_rx(dev);
}

void cc430radio_switch_to_rx(cc430radio_t *dev)
{
    DEBUG("%s:%s:%u\n", RIOT_FILE_RELATIVE, __func__, __LINE__);

#ifdef MODULE_CC430RADIO_HOOKS
    cc430radio_hook_rx();
#endif

    gpio_irq_disable(dev->params.gdo2);

    /* flush RX fifo */
    cc430radio_strobe(dev, CC430RADIO_SIDLE);
    cc430radio_strobe(dev, CC430RADIO_SFRX);

    dev->radio_state = RADIO_RX;

    cc430radio_write_reg(dev, CC430RADIO_IOCFG2, 0x6);
    cc430radio_strobe(dev, CC430RADIO_SRX);

    gpio_irq_enable(dev->params.gdo2);
}

void cc430radio_wakeup_from_rx(cc430radio_t *dev)
{
    if (dev->radio_state != RADIO_RX) {
        return;
    }

    LOG_DEBUG("cc430radio: switching to idle mode\n");

    cc430radio_strobe(dev, CC430RADIO_SIDLE);
    dev->radio_state = RADIO_IDLE;
}

void cc430radio_switch_to_pwd(cc430radio_t *dev)
{
    LOG_DEBUG("cc430radio: switching to powerdown mode\n");
    cc430radio_wakeup_from_rx(dev);
    cc430radio_strobe(dev, CC430RADIO_SPWD);
    dev->radio_state = RADIO_PWD;

#ifdef MODULE_CC430RADIO_HOOKS
     cc430radio_hook_off();
#endif
}

#ifndef MODULE_CC430RADIO_HOOKS
int16_t cc430radio_set_channel(cc430radio_t *dev, uint8_t channr)
{
    DEBUG("%s:%s:%u\n", RIOT_FILE_RELATIVE, __func__, __LINE__);

    if (channr > MAX_CHANNR) {
        return -1;
    }

    cc430radio_write_register(dev, CC430RADIO_CHANNR, channr * 10);
    dev->radio_channel = channr;

    return channr;
}
#endif

#ifndef CC430RADIO_DONT_RESET
static void _reset(cc430radio_t *dev)
{
    DEBUG("%s:%s:%u\n", RIOT_FILE_RELATIVE, __func__, __LINE__);
    cc430radio_wakeup_from_rx(dev);
    cc430radio_cs(dev);
    cc430radio_strobe(dev, CC430RADIO_SRES);
    xtimer_usleep(100);
}

static void _power_up_reset(cc430radio_t *dev)
{
    DEBUG("%s:%s:%u\n", RIOT_FILE_RELATIVE, __func__, __LINE__);
    gpio_set(dev->params.cs);
    gpio_clear(dev->params.cs);
    gpio_set(dev->params.cs);
    xtimer_usleep(RESET_WAIT_TIME);
    _reset(dev);
}
#endif

void cc430radio_write_register(cc430radio_t *dev, uint8_t r, uint8_t value)
{
    /* Save old radio state */
    uint8_t old_state = dev->radio_state;

    /* Wake up from RX (no effect if in other mode) */
    cc430radio_wakeup_from_rx(dev);
    cc430radio_write_reg(dev, r, value);

    /* Have to put radio back to RX if old radio state
     * was RX, otherwise no action is necessary */
    if (old_state == RADIO_RX) {
        cc430radio_switch_to_rx(dev);
    }
}

int cc430radio_rd_set_mode(cc430radio_t *dev, int mode)
{
    DEBUG("%s:%s:%u\n", RIOT_FILE_RELATIVE, __func__, __LINE__);

    int result;

    /* Get current radio mode */
    if ((dev->radio_state == RADIO_UNKNOWN) || (dev->radio_state == RADIO_PWD)) {
        result = RADIO_MODE_OFF;
    }
    else {
        result = RADIO_MODE_ON;
    }

    switch(mode) {
        case RADIO_MODE_ON:
            LOG_DEBUG("cc430radio: switching to RX mode\n");
            cc430radio_setup_rx_mode(dev);          /* Set chip to desired mode */
            break;

        case RADIO_MODE_OFF:
            gpio_irq_disable(dev->params.gdo2); /* Disable interrupts */
            cc430radio_switch_to_pwd(dev);          /* Set chip to power down mode */
            break;

        case RADIO_MODE_GET:
            /* do nothing, just return current mode */
        default:
            /* do nothing */
            break;
    }

    /* Return previous mode */
    return result;
}
