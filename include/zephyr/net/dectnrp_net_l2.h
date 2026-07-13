/*
 * Copyright (c) 2026 Deveritec GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public DECT NR+ API as part of network L2 interface.
 * 
 * Dectnrp stack is not directly integrated into Zephyr but is implemented as a module.
 * Following code makes the dectnrp l2 known and would live in deps/zephyr/include/zephyr/net/net_l2.h.
 * For example IEEE802154_L2 is declared at
 * https://github.com/zephyrproject-rtos/zephyr/blob/a6eef0ba3755f2530c5ce93524e5ac4f5be30194/include/zephyr/net/net_l2.h#L114
 * 
 */

#ifndef ZEPHYR_NET_DECTNRP_NET_L2_H_
#define ZEPHYR_NET_DECTNRP_NET_L2_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup dectnrp DECT NR+
 * @version 0.0.1
 *
 * @brief DECT NR+
 */

#define DECTNRP_L2 DECTNRP
NET_L2_DECLARE_PUBLIC(DECTNRP_L2);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_NET_DECTNRP_NET_L2_H_ */