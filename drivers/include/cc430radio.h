/*
 * Copyright (C) 2014 Freie Universität Berlin
 *               2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    drivers_cc430radio cc430radio
 * @ingroup     drivers_netdev
 * @brief       TI cc430radio
 * @{
 * @file
 * @brief       Public interface for cc430radio driver
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 */

#ifndef CC430RADIO_H
#define CC430RADIO_H

#ifdef __cplusplus
extern "C" {
#endif

//#include "periph/spi.h"
//#include "periph/gpio.h"
#include "cc430radio/cc430radio-internal.h"

/**
 * @brief Struct for holding cc430radio IO parameters
 */
typedef struct cc430radio_params {
    int spi;          /**< what */
    int cs;          /**< does */
    int gdo0;        /**< this */
    int gdo1;        /**< look */
    int gdo2;        /**< like */
} cc430radio_params_t;

/**
 * @brief forward declaration
 */
typedef struct cc430radio cc430radio_t;

/**
 * @brief Struct for holding cc430radio device state
 */
struct cc430radio {
    cc430radio_params_t params;                     /**< cc430radio IO configuration */

    cc430radio_statistic_t cc430radio_statistic;        /**< Statistic values for
                                                     debugging */

    uint8_t radio_state;                        /**< Radio state */
    uint8_t radio_channel;                      /**< current Radio channel */
    uint8_t radio_address;                      /**< current Radio address */

    cc430radio_pkt_buf_t pkt_buf;                   /**< RX/TX buffer */
    void (*isr_cb)(cc430radio_t *dev, void* arg);   /**< isr callback */
    void *isr_cb_arg;                           /**< isr callback argument */
};

/**
 * @brief Setup cc430radio device parameters
 *
 * @param[in] dev       device struct to set up
 * @param[in] params    struct holding parameters
 *
 * @return always succeeds
 */
int cc430radio_setup(cc430radio_t *dev, const cc430radio_params_t *params);

/**
 * @brief Set cc430radio channel number
 *
 * @param[in] dev       device to work on
 * @param[in] channr    guess what
 *
 * @return nr of set channel on success
 * @return -1 on error
 */
int16_t cc430radio_set_channel(cc430radio_t *dev, uint8_t channr);


/**
 * @brief Send raw cc430radio packet
 *
 * @param[in] dev       Device to send on
 * @param[in] packet    ptr to packet to be sent
 *
 * @return size of packet on success
 * @return <0 on error
 */
int cc430radio_send(cc430radio_t *dev, cc430radio_pkt_t *packet);

/**
 * @brief Set cc430radio radio address
 *
 * @param[in] dev   device to query
 *
 * @return nr of currently set address
 */
uint8_t cc430radio_get_address(cc430radio_t *dev);

/**
 * @brief Set cc430radio radio address
 *
 * @param[in] dev       device to work on
 * @param[in] address   new address
 *
 * @return address set on success
 * @return 0 on error
 */
uint8_t cc430radio_set_address(cc430radio_t *dev, uint8_t address);


/**
 * @brief Set cc430radio monitor mode setting
 *
 * @param[in] dev   device to work on
 * @param[in] mode  mode to set (0 or 1)
 */
void cc430radio_set_monitor(cc430radio_t *dev, uint8_t mode);

#ifdef __cplusplus
}
#endif

#endif /* CC430RADIO_H */
/** @} */
