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
 *
 * @file
 * @brief       Variables for the cc430radio ng_netdev base interface
 *
 * @author      Fabian Nack <nack@inf.fu-berlin.de>
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 */

#ifndef CC430RADIO_NETDEV_H
#define CC430RADIO_NETDEV_H

#include "periph/gpio.h"
#include "periph/spi.h"
#include "net/netdev2.h"
#include "cc430radio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Implementation of netdev2_driver_t for CC430RADIO device
 */
extern const netdev2_driver_t netdev2_cc430radio_driver;

/**
 * @brief cc430radio netdev2 struct
 */
typedef struct netdev2_cc430radio {
    netdev2_t netdev;       /**< writing obious */
    cc430radio_t cc430radio;        /**< documentation here */
} netdev2_cc430radio_t;


/**
 * @brief netdev2 <-> cc430radio glue code initialization function
 *
 * @param[out]      netdev2_cc430radio  ptr to netdev2_cc430radio struct ti initialize
 * @param[in]       params          cc430radio IO parameter struct to use
 *
 * @return          0               on success
 * @return          -1              on error
 */
int netdev2_cc430radio_setup(netdev2_cc430radio_t *netdev2_cc430radio, const cc430radio_params_t *params);

#ifdef __cplusplus
}
#endif

#endif /* CC430RADIO_NETDEV_H */
