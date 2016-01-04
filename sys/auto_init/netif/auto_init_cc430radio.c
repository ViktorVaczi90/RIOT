/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 */

/*
 * @ingroup auto_init_ng_netif
 * @{
 *
 * @file
 * @brief   Auto initialization for cc430radio network interfaces
 *
 * @author  Kaspar Schleiser <kaspar@schleiser.de>
 */

#ifdef MODULE_CC430RADIO

#include "board.h"
#include "net/gnrc/gnrc_netdev2.h"
#include "cc430radio/gnrc_netdev2_cc430radio.h"
#include "net/gnrc.h"

#include "cc430radio.h"
#include "cc430radio_params.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

/**
 * @brief   Define stack parameters for the MAC layer thread
 * @{
 */
#define CC430RADIO_MAC_STACKSIZE     (THREAD_STACKSIZE_DEFAULT + DEBUG_EXTRA_STACKSIZE)
#define CC430RADIO_MAC_PRIO          (THREAD_PRIORITY_MAIN - 3)

#define CC430RADIO_NUM (sizeof(cc430radio_params)/sizeof(cc430radio_params[0]))

static netdev2_cc430radio_t cc430radio_devs[CC430RADIO_NUM];
static char _stacks[CC430RADIO_MAC_STACKSIZE][CC430RADIO_NUM];

static gnrc_netdev2_t _gnrc_netdev2_devs[CC430RADIO_NUM];

void auto_init_cc430radio(void)
{
    for (int i = 0; i < CC430RADIO_NUM; i++) {
        const cc430radio_params_t *p = &cc430radio_params[i];
        DEBUG("Initializing CC430RADIO radio at SPI_%i\n", p->spi);
        int res = netdev2_cc430radio_setup(&cc430radio_devs[i], p);
        if (res < 0) {
            DEBUG("Error initializing CC430RADIO radio device!");
        }
        else {
            gnrc_netdev2_cc430radio_init(&_gnrc_netdev2_devs[i], &cc430radio_devs[i]);
            res = gnrc_netdev2_init(_stacks[i], CC430RADIO_MAC_STACKSIZE,
                    CC430RADIO_MAC_PRIO, "cc430radio", &_gnrc_netdev2_devs[i]);
            if (res < 0) {
                DEBUG("Error starting gnrc_cc430radio thread for CC430RADIO!");
            }
        }
    }
}
#else
typedef int dont_be_pedantic;
#endif /* MODULE_CC430RADIO */

/** @} */
