/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup   driver_cc430radio
 * @{
 *
 * @file
 * @brief     cc430radio default settings override
 *
 * By setting either CC430RADIO_DEFAULT_PATABLE or CC430RADIO_DEFAULT_FREQ in board.h,
 * it is possible to override the default pa table or base frequency registers
 * on a per-device basis.
 *
 * @author    Kaspar Schleiser <kaspar@schleiser.de>
 */
#ifndef CC430RADIO_DEFAULTSETTINGS_H
#define CC430RADIO_DEFAULTSETTINGS_H

#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CC430RADIO_DEFAULT_PATABLE
#define CC430RADIO_DEFAULT_PATABLE cc430radio_default_pa_table
extern const char cc430radio_default_pa_table[8];
#endif

#ifndef CC430RADIO_DEFAULT_FREQ
#define CC430RADIO_DEFAULT_FREQ cc430radio_default_base_freq
extern const char cc430radio_default_base_freq[3];
#endif

#ifdef __cplusplus
}
#endif

#endif /* CC430RADIO_DEFAULTSETTINGS_H */
/** @} */
