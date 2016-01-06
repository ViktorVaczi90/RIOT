/*
 * Copyright (C) 2014 Freie Universität Berlin
 *               2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_cc430radio
 * @{
 * @file
 * @brief       Implementation of netdev2 interface for cc430radio
 *
 * @author      Fabian Nack <nack@inf.fu-berlin.de>
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @}
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "cc430radio.h"
#include "cc430radio/cc430radio-netdev2.h"
#include "cc430radio/cc430radio-internal.h"
#include "cc430radio/cc430radio-interface.h"
#include "net/eui64.h"

#include "periph/cpuid.h"
//#include "periph/gpio.h"
#include "net/netdev2.h"
#include "net/gnrc/nettype.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

static int _send(netdev2_t *dev, const struct iovec *vector, int count)
{
    DEBUG("%s:%u\n", __func__, __LINE__);

    netdev2_cc430radio_t *netdev2_cc430radio = (netdev2_cc430radio_t*) dev;
    cc430radio_pkt_t *cc430radio_pkt = vector[0].iov_base;

    return cc430radio_send(&netdev2_cc430radio->cc430radio, cc430radio_pkt);
}

static int _recv(netdev2_t *dev, char* buf, int len)
{
    DEBUG("%s:%u\n", __func__, __LINE__);

    cc430radio_t *cc430radio = &((netdev2_cc430radio_t*) dev)->cc430radio;

    cc430radio_pkt_t *cc430radio_pkt = &cc430radio->pkt_buf.packet;
    if (cc430radio_pkt->length > len) {
        return -ENOSPC;
    }

    memcpy(buf, (void*)cc430radio_pkt, cc430radio_pkt->length);
    return cc430radio_pkt->length;
}

static inline int _get_iid(netdev2_t *netdev, eui64_t *value, size_t max_len)
{
    if (max_len < sizeof(eui64_t)) {
        return -EOVERFLOW;
    }

    uint8_t *eui64 = (uint8_t*) value;
#ifdef CPUID_ID_LEN
    int n = (CPUID_ID_LEN < sizeof(eui64_t))
        ? CPUID_ID_LEN
        : sizeof(eui64_t);

    char cpuid[CPUID_ID_LEN];
    cpuid_get(cpuid);

    memcpy(eui64 + 8 - n, cpuid, n);

#else
    for (int i = 0; i < 8; i++) {
        eui64[i] = i;
    }
#endif

    /* make sure we mark the address as non-multicast and not globally unique */
    eui64[0] &= ~(0x01);
    eui64[0] |= 0x02;

    return sizeof(eui64_t);
}

static int _get(netdev2_t *dev, netopt_t opt, void *value, size_t value_len)
{
    cc430radio_t *cc430radio = &((netdev2_cc430radio_t*) dev)->cc430radio;

    switch (opt) {
        case NETOPT_DEVICE_TYPE:
            assert(value_len == 2);
            *((uint16_t *) value) = NETDEV2_TYPE_CC430RADIO;
            return 2;
        case NETOPT_PROTO:
            assert(value_len == sizeof(gnrc_nettype_t));
#ifdef MODULE_GNRC_SIXLOWPAN
            *((gnrc_nettype_t*)value) = GNRC_NETTYPE_SIXLOWPAN;
#else
            *((gnrc_nettype_t*)value) = GNRC_NETTYPE_UNDEF;
#endif
            return sizeof(gnrc_nettype_t);
        case NETOPT_CHANNEL:
            assert(value_len > 1);
            *((uint16_t *)value) = (uint16_t)cc430radio->radio_channel;
            return 2;
        case NETOPT_ADDRESS:
            assert(value_len > 0);
            *((uint8_t *)value) = cc430radio->radio_address;
            return 1;
        case NETOPT_MAX_PACKET_SIZE:
            assert(value_len > 0);
            *((uint8_t *)value) = CC430RADIO_PACKET_LENGTH;
            return 1;
        case NETOPT_IPV6_IID:
            return _get_iid(dev, value, value_len);
        default:
            break;
    }

    return -ENOTSUP;
}

static int _set(netdev2_t *dev, netopt_t opt, void *value, size_t value_len)
{
    cc430radio_t *cc430radio = &((netdev2_cc430radio_t*) dev)->cc430radio;

    switch (opt) {
        case NETOPT_CHANNEL:
            {
                uint8_t *arg = (uint8_t*)value;
                uint8_t channel = arg[value_len-1];
                if ((channel < CC430RADIO_MIN_CHANNR) || (channel > CC430RADIO_MAX_CHANNR)) {
                    return -EINVAL;
                }
                if (cc430radio_set_channel(cc430radio, channel) == -1) {
                    return -EINVAL;
                }
                return 1;
            }
        case NETOPT_ADDRESS:
            if (value_len < 1) {
                return -EINVAL;
            }
            if (!cc430radio_set_address(cc430radio, *(uint8_t*)value)) {
                return -EINVAL;
            }
            return 1;
        default:
            return -ENOTSUP;
    }

    return 0;
}

/*static*/ void _netdev2_cc430radio_isr(void *arg)
{
    netdev2_t *netdev2 = (netdev2_t*) arg;
    netdev2->event_callback(netdev2, NETDEV2_EVENT_ISR, netdev2->isr_arg);
}

static void _netdev2_cc430radio_rx_callback(void *arg)
{
    netdev2_t *netdev2 = (netdev2_t*) arg;
    //cc430radio_t *cc430radio = &((netdev2_cc430radio_t*) arg)->cc430radio;
    turnOffGIE2Interrupt();
    netdev2->event_callback(netdev2, NETDEV2_EVENT_RX_COMPLETE, netdev2->isr_arg);
}

static void _isr(netdev2_t *dev)
{
    cc430radio_t *cc430radio = &((netdev2_cc430radio_t*) dev)->cc430radio;
    cc430radio_isr_handler(cc430radio, _netdev2_cc430radio_rx_callback, (void*)dev);
}

static int _init(netdev2_t *dev)
{
    DEBUG("%s:%u\n", __func__, __LINE__);
	setupInterrupt((void*)dev);
	
    cc430radio_t *cc430radio = &((netdev2_cc430radio_t*) dev)->cc430radio;
	/*
    gpio_init_int(cc430radio->params.gdo2, GPIO_NOPULL, GPIO_BOTH,
            &_netdev2_cc430radio_isr, (void*)dev);

    gpio_set(cc430radio->params.gdo2);
    turnOffGIE2Interrupt();
*/
    /* Switch to RX mode */
    cc430radio_rd_set_mode(cc430radio, RADIO_MODE_ON);

    return 0;
}

const netdev2_driver_t netdev2_cc430radio_driver = {
    .send=_send,
    .recv=_recv,
    .init=_init,
    .get=_get,
    .set=_set,
    .isr=_isr
};

int netdev2_cc430radio_setup(netdev2_cc430radio_t *netdev2_cc430radio, const cc430radio_params_t *params)
{
    DEBUG("netdev2_cc430radio_setup()\n");
    netdev2_cc430radio->netdev.driver = &netdev2_cc430radio_driver;

    return cc430radio_setup(&netdev2_cc430radio->cc430radio, params);
}
