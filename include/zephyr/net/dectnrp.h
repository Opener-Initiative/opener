/*
 * Copyright (c) 2026 Codium Electronique
 * Copyright (c) 2026 Deveritec GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public DECT NR+ API as part of network L2 interface.
 */

#ifndef ZEPHYR_NET_DECTNRP_H_
#define ZEPHYR_NET_DECTNRP_H_

#include <zephyr/net/net_l2.h>
#include <zephyr/net/dectnrp_net_l2.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup dectnrp DECT NR+
 * @version 0.0.1
 * @ingroup connectivity
 * 
 * @brief DECT NR+ native L2, configuration, management and driver APIs
 * 
 */

/**
 * @defgroup dectnrp_l2 DECT NR+ L2
 * @version 0.0.1
 * @ingroup dectnrp
 *
 * @brief DECT NR+ L2 APIs
 *
 * @details This API provides integration with Zephyr's sockets and network
 * contexts. **Application and driver developers should never interface directly
 * with this API.** It is of interest to subsystem maintainers only.
 *
 * @note All section, table and figure references are to the ETSI TS 103 636 DECTNRP NR+ V2.2.1 (2026-05)
 * standard.
 *
 * @{
 */

/** 
 * @brief DECT NR+ "hardware" MTU (not to be confused with L3/IP MTU), i.e.
 * the actual payload available to the next higher layer.
 * 
 * @details This is equivalent to the maximum supported DECT NR+ frame length including PHY and MAC.
 * 
 * @note This is currently a pre-liminary value and may need improvement.
 */
#define DECTNRP_MTU 1024

/** @brief Size in byte of PHY header of type 1. */
#define DECTNRP_PHY_HEADER_TYPE1_SIZE 5
/** @brief Size in byte of PHY header of type 2. */
#define DECTNRP_PHY_HEADER_TYPE2_SIZE 10

/** @brief Long broadcast address.
 *  @ref ETSI TS 103 636-4, Table 4.2.3.2-1: Use of Long RD ID address space */
#define DECTNRP_LONG_BROADCAST_ADDRESS       UINT_MAX
/** @brief Short broadcast address.
 *  @ref ETSI TS 103 636-4, Table 4.2.3.3-1: Use of Short RD ID address space */
#define DECTNRP_SHORT_BROADCAST_ADDRESS      USHRT_MAX

/** @brief Minimum supported RSSI1/2 value.
 *  @ref ETSI TS 103 636-2, Table 8.3.3-1: RSSI-2 measurement report mapping */
#define DECTNRP_RSSI_MIN -140
/** @brief Maximum supported RSSI1/2 value.
 *  @ref ETSI TS 103 636-2, Table 8.3.3-1: RSSI-2 measurement report mapping */
#define DECTNRP_RSSI_MAX -1
/** @brief Minimum supported SNR value.
 *  @ref ETSI TS 103 636-2, Table 8.4.3-1: Demodulated signal to noise quality */
#define DECTNRP_SNR_MIN  -16
/** @brief Maximum supported SNR value.
 *  @ref ETSI TS 103 636-2, Table 8.4.3-1: Demodulated signal to noise quality */
#define DECTNRP_SNR_MAX  64

/** @brief The number of slots contained in any frame. */
#define DECTNRP_SLOTS_PER_FRAME     24U
/** @brief Number of sub-slots per slot.
 *  @todo make dynamic - number of sub-slots per slot depends on µ, we assume fixed µ=1 here,
 * should be dynamically dependend on selected µ. */
#define DECTNRP_SUBSLOTS_PER_SLOT   2U
/** @brief Number of symbols per sub-slot */
#define DECTNRP_SYMBOLS_PER_SUBSLOT 5U
/** @brief Number of sub-slots per frame
 *  @todo make dynamic - number of sub-slots per frame depends on µ, we assume fixed µ=1 here,
 * should be dynamically dependend on selected µ. */
#define DECTNRP_SUBSLOTS_PER_FRAME  (DECTNRP_SLOTS_PER_FRAME * DECTNRP_SUBSLOTS_PER_SLOT)
/** @brief Number of symbols per frame
 *  @todo make dynamic - number of sub-slots per frame depends on µ, we assume fixed µ=1 here,
 * should be dynamically dependend on selected µ. */
#define DECTNRP_SYMBOLS_PER_FRAME   (DECTNRP_SUBSLOTS_PER_FRAME * DECTNRP_SYMBOLS_PER_SUBSLOT)

/**
 * @brief Number of modem ticks per symbol
 * @todo make dynamic - number of sub-slots per frame depends on µ, we assume fixed µ=1 here,
 * should be dynamically dependend on selected µ. */
#define DECTNRP_MODEMTICKS_PER_SYMBOL   CONFIG_DECTNRP_DRIVER_MODEMTICKS_PER_SYMBOL

/** @brief DECT NR+ L2 context. */
struct dectnrp_context {

    /** Related network interface. */
	struct net_if *iface;
    /** Flags marking supported features of l2 layer.  */
    enum net_l2_flags flags;
};

/** @cond INTERNAL_HIDDEN */

#define DECTNRP_L2_CTX_TYPE struct dectnrp_context

/** INTERNAL_HIDDEN @endcond */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_NET_DECTNRP_H_ */
