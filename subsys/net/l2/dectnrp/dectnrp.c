/*
 * Copyright (c) 2026 Deveritec GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief DECT NR+ MAC layer implementation
 *
 * @note All section, table and figure references are to the ETSI TS 103 636 DECTNRP NR+ V2.2.1
 * (2026-05) standard.
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_dectnrp, CONFIG_NET_L2_DECTNRP_LOG_LEVEL);

#include <sys/errno.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_l2.h>
#include <zephyr/net/net_log.h>
#include <zephyr/net/dectnrp_net_l2.h>

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
 * @brief DECT NR+ L2 Implementation
 *
 * @details
 *
 * @note All section, table and figure references are to the ETSI TS 103 636 DECTNRP NR+ V2.2.1
 * (2026-05) standard.
 *
 * @{
 */

enum net_verdict dectnrp_recv(struct net_if *iface, struct net_pkt *pkt)
{
	NET_DBG("dectnrp_recv(%p)", iface);
	return NET_DROP;
}

static int dectnrp_send(struct net_if *iface, struct net_pkt *pkt)
{
	NET_DBG("dectnrp_send(%p)", iface);
	return -EIO;
}

static int dectnrp_enable(struct net_if *iface, bool state)
{
	NET_DBG("iface %p %s", iface, state ? "up" : "down");
	return -EIO;
}

static enum net_l2_flags dectnrp_flags(struct net_if *iface)
{
	return 0x0;
}

NET_L2_INIT(DECTNRP_L2, dectnrp_recv, dectnrp_send, dectnrp_enable, dectnrp_flags);

void dectnrp_l2_init(struct net_if *iface)
{
	NET_DBG("dectnrp_l2_init(%p)", iface);

	NET_DBG("L2 interface initialized");
	return;
}

/**
 * @}
 */
