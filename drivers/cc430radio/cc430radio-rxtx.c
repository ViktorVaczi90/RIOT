/*
 * Copyright (C) 2014 Freie Universität Berlin
 * Copyright (C) 2013 INRIA
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_cc430radio
 * @{
 * @file
 * @brief       Functions for packet reception and transmission on cc430radio devices
 *
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 * @author      Fabian Nack <nack@inf.fu-berlin.de>
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @}
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "cc430radio.h"
#include "cc430radio/cc430radio-spi.h"
#include "cc430radio/cc430radio-internal.h"
#include "cc430radio/cc430radio-interface.h"
#include "cc430radio/cc430radio-defines.h"

//#include "periph/gpio.h"
#include "irq.h"

#include "kernel_types.h"
#include "msg.h"

#include "cpu_conf.h"
#include "cpu.h"

#include "log.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

#include "cc430f5137.h"
static void _rx_abort(cc430radio_t *dev)
{
    turnOffGIE2Interrupt();

    cc430radio_strobe(dev, CC430RADIO_SIDLE);    /* Switch to IDLE (should already be)... */
    cc430radio_strobe(dev, CC430RADIO_SFRX);     /* ...for flushing the RX FIFO */

    cc430radio_switch_to_rx(dev);
}

static void _rx_start(cc430radio_t *dev)
{
    dev->radio_state = RADIO_RX_BUSY;

    cc430radio_pkt_buf_t *pkt_buf = &dev->pkt_buf;
    pkt_buf->pos = 0;

    turnOffGIE2Interrupt();
    cc430radio_write_reg(dev, CC430RADIO_IOCFG2, 0x01);
    turnOnGIE2Interrupt();
}

static void _rx_read_data(cc430radio_t *dev, void(*callback)(void*), void*arg)
{
    int fifo = cc430radio_get_reg_robust(dev, 0xfb);

    if (fifo & 0x80) {
        DEBUG("%s:%s:%u rx overflow\n", RIOT_FILE_RELATIVE, __func__, __LINE__);
        _rx_abort(dev);
        return;
    }

    if (!fifo) {
        turnOnGIE2Interrupt();
        return;
    }

    cc430radio_pkt_buf_t *pkt_buf = &dev->pkt_buf;
    if (!pkt_buf->pos) {
        pkt_buf->pos = 1;
        pkt_buf->packet.length = cc430radio_read_reg(dev, CC430RADIO_RXFIFO);

        /* Possible packet received, RX -> IDLE (0.1 us) */
        dev->cc430radio_statistic.packets_in++;
    }

    int left = pkt_buf->packet.length+1 - pkt_buf->pos;

    /* if the fifo doesn't contain the rest of the packet,
     * leav at least one byte as per spec sheet. */
    int to_read = (fifo < left) ? (fifo-1) : fifo;
    if (to_read > left) {
        to_read = left;
    }

    if (to_read) {
        cc430radio_readburst_reg(dev, CC430RADIO_RXFIFO,
                ((char *)&pkt_buf->packet)+pkt_buf->pos, to_read);
        pkt_buf->pos += to_read;
    }

    if (to_read == left) {
        uint8_t status[2];
        /* full packet received. */
        /* Read the 2 appended status bytes (status[0] = RSSI, status[1] = LQI) */
        cc430radio_readburst_reg(dev, CC430RADIO_RXFIFO, (char *)status, 2);

        /* Store RSSI value of packet */
        pkt_buf->rssi = status[I_RSSI];

        /* Bit 0-6 of LQI indicates the link quality (LQI) */
        pkt_buf->lqi = status[I_LQI] & LQI_EST;

        /* MSB of LQI is the CRC_OK bit */
        int crc_ok = (status[I_LQI] & CRC_OK) >> 7;

        if (crc_ok) {
                    LOG_DEBUG("cc430radio: received packet from=%u to=%u payload "
                            "len=%u\n",
                    (unsigned)pkt_buf->packet.phy_src,
                    (unsigned)pkt_buf->packet.address,
                    pkt_buf->packet.length-3);
            /* let someone know that we've got a packet */
            callback(arg);

            cc430radio_switch_to_rx(dev);
        }
        else {
            DEBUG("%s:%s:%u crc-error\n", RIOT_FILE_RELATIVE, __func__, __LINE__);
            dev->cc430radio_statistic.packets_in_crc_fail++;
            _rx_abort(dev);
        }
    }
}

static void _rx_continue(cc430radio_t *dev, void(*callback)(void*), void*arg)
{

    if (dev->radio_state != RADIO_RX_BUSY) {
        DEBUG("%s:%s:%u _rx_continue in invalid state\n", RIOT_FILE_RELATIVE,
                __func__, __LINE__);
        _rx_abort(dev);
        return;
    }

    turnOffGIE2Interrupt();

    do {
        _rx_read_data(dev, callback, arg);
    }
    while (PKTSTATUS | BIT2);
}

static void _tx_abort(cc430radio_t *dev)
{
    cc430radio_switch_to_rx(dev);
}

static void _tx_continue(cc430radio_t *dev)
{
    turnOffGIE2Interrupt();

    cc430radio_pkt_t *pkt = &dev->pkt_buf.packet;
    int size = pkt->length + 1;
    int left = size - dev->pkt_buf.pos;

    if (!left) {
        dev->cc430radio_statistic.raw_packets_out++;

        LOG_DEBUG("cc430radio: packet successfully sent.\n");

        cc430radio_switch_to_rx(dev);
        return;
    }

    int fifo = 64 - cc430radio_get_reg_robust(dev, 0xfa);

    if (fifo & 0x80) {
        DEBUG("%s:%s:%u tx underflow!\n", RIOT_FILE_RELATIVE, __func__, __LINE__);
        _tx_abort(dev);
        return;
    }

    if (!fifo) {
        DEBUG("%s:%s:%u fifo full!?\n", RIOT_FILE_RELATIVE, __func__, __LINE__);
        _tx_abort(dev);
        return;
    }

    int to_send = left > fifo ? fifo : left;

    /* Write packet into TX FIFO */
    cc430radio_writeburst_reg(dev, CC430RADIO_TXFIFO, ((char *)pkt)+dev->pkt_buf.pos, to_send);
    dev->pkt_buf.pos += to_send;

    if (left == size) {
        /* Switch to TX mode */
        cc430radio_strobe(dev, CC430RADIO_STX);
    }

    if (to_send < left) {
        /* set GDO2 to 0x2 -> will deassert at TX FIFO below threshold */
        turnOnGIE2Interrupt();
        cc430radio_write_reg(dev, CC430RADIO_IOCFG2, 0x02);
    }
    else {
        /* set GDO2 to 0x6 -> will deassert at packet end */
        cc430radio_write_reg(dev, CC430RADIO_IOCFG2, 0x06);
        turnOnGIE2Interrupt();
    }
}

void cc430radio_isr_handler(cc430radio_t *dev, void(*callback)(void*), void*arg)
{
    switch (dev->radio_state) {
        case RADIO_RX:
            if (PKTSTATUS | BIT2) {
                _rx_start(dev);
            }
            else {
                DEBUG("cc430radio_isr_handler((): isr handled too slow?\n");
                _rx_abort(dev);
            }
            break;
        case RADIO_RX_BUSY:
            _rx_continue(dev, callback, arg);
            break;
        case RADIO_TX_BUSY:
            if (!(PKTSTATUS | BIT2)) {
                _tx_continue(dev);
            }
            else {
                DEBUG("cc430radio_isr_handler() RADIO_TX_BUSY + GDO2\n");
            }
            break;
        default:
            DEBUG("%s:%s:%u: unhandled mode\n", RIOT_FILE_RELATIVE,
                    __func__, __LINE__);
    }
}

int cc430radio_send(cc430radio_t *dev, cc430radio_pkt_t *packet)
{
    DEBUG("cc430radio: snd pkt to %u payload_length=%u\n",
            (unsigned)packet->address, (unsigned)packet->length-3);
    uint8_t size;

    switch (dev->radio_state) {
        case RADIO_RX_BUSY:
        case RADIO_TX_BUSY:
            DEBUG("cc430radio: invalid state for sending: %s\n",
                    cc430radio_state_to_text(dev->radio_state));
            return -EAGAIN;
    }

    /*
     * Number of bytes to send is:
     * length of phy payload (packet->length)
     * + size of length field (1 byte)
     */
    size = packet->length + 1;

    if (size > CC430RADIO_PACKET_LENGTH) {
        DEBUG("%s:%s:%u trying to send oversized packet\n",
                RIOT_FILE_RELATIVE, __func__, __LINE__);
        return -ENOSPC;
    }

    /* set source address */
    packet->phy_src = dev->radio_address;

    /* Disable RX interrupt */
    turnOffGIE2Interrupt();
    dev->radio_state = RADIO_TX_BUSY;

#ifdef MODULE_CC430RADIO_HOOKS
    cc430radio_hook_tx();
#endif

    cc430radio_write_reg(dev, CC430RADIO_IOCFG2, 0x02);

    /* Put CC110x in IDLE mode to flush the FIFO */
    cc430radio_strobe(dev, CC430RADIO_SIDLE);
    /* Flush TX FIFO to be sure it is empty */
    cc430radio_strobe(dev, CC430RADIO_SFTX);

    memcpy((char*)&dev->pkt_buf.packet, packet, size);
    dev->pkt_buf.pos = 0;

    _tx_continue(dev);

    return size;
}
