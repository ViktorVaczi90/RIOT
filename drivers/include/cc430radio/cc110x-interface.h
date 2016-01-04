/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
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
 *
 * @file
 * @brief       internal declarations for cc430radio driver
 *
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 */

#ifndef CC430RADIO_INTERFACE_H
#define CC430RADIO_INTERFACE_H

#include <stdint.h>
#include "cc430radio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name cc430radio raw low-level interface
 * @internal
 * @{
 */
char *cc430radio_get_marc_state(cc430radio_t *dev);
char *cc430radio_state_to_text(uint8_t state);
int cc430radio_rd_set_mode(cc430radio_t *dev, int mode);
uint8_t cc430radio_get_buffer_pos(cc430radio_t *dev);
void cc430radio_isr_handler(cc430radio_t *dev, void(*callback)(void*), void*arg);
void cc430radio_set_base_freq_raw(cc430radio_t *dev, const char* freq_array);
void cc430radio_setup_rx_mode(cc430radio_t *dev);
void cc430radio_switch_to_pwd(cc430radio_t *dev);
void cc430radio_switch_to_rx(cc430radio_t *dev);
void cc430radio_wakeup_from_rx(cc430radio_t *dev);
void cc430radio_write_register(cc430radio_t *dev, uint8_t r, uint8_t value);

extern const char cc430radio_default_conf[];
extern const uint8_t cc430radio_default_conf_size;
extern const uint8_t cc430radio_pa_table[];

#ifdef MODULE_CC430RADIO_HOOKS
void cc430radio_hooks_init(void);
void cc430radio_hook_rx(void);
void cc430radio_hook_tx(void);
void cc430radio_hook_off(void);
#endif
/* @} */

#ifdef __cplusplus
}
#endif

/** @} */
#endif /* CC430RADIO_INTERFACE_H */
