/*
 * Copyright (c) 2026 Codium Electronique
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/net/net_if.h>

#include <opener/build_info.h>

#include <zephyr/net/dectnrp/driver.h>

LOG_MODULE_REGISTER(dectnrp, CONFIG_SAMPLE_DRIVER_DECTNRP_LOG_LEVEL);

void dectnrp_l2_init(struct net_if *iface)
{
	LOG_DBG("L2 interface initialized");
	return;
}

int main()
{
	LOG_DBG("Opener version %d.%d.%d-%s%c", OPENER_VERSION_MAJOR, OPENER_VERSION_MINOR,
		OPENER_PATCHLEVEL, STRINGIFY(OPENER_GIT_COMMIT), OPENER_DIRTY ? '+' : '=');

	LOG_DBG("Opener dectnrp driver sample started");
	return 0;
}
