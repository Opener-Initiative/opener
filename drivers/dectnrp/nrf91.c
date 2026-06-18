/* Nordic nRF91x1 NR+ PHY driver
 *
 * Copyright (c) 2026 Codium Electronique
 * Copyright (c) 2026 Deveritech
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/net/net_time.h>
#include <zephyr/net/net_pkt.h>

#include <dectnrp/driver.h>

LOG_MODULE_REGISTER(dectnrp_driver_nrf91, CONFIG_DECTNRP_DRIVER_NRF91_LOG_LEVEL);

static int nrf91_start(const struct device *dev)
{
	ARG_UNUSED(dev);
	return -ENOSYS;
}

static int nrf91_stop(const struct device *dev)
{
	ARG_UNUSED(dev);
	return -ENOSYS;
}

static int nrf91_get_capabilities(const struct device *dev, struct device_capabilities *caps)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(caps);
	return -ENOSYS;
}

static int nrf91_configure(const struct device *dev, enum dectnrp_config_type type,
			   const struct dectnrp_config *config)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(type);
	ARG_UNUSED(config);
	return -ENOSYS;
}

static net_time_t nrf91_get_time(const struct device *dev)
{
	ARG_UNUSED(dev);
	return -ENOSYS;
}

#if CONFIG_DECTNRP_DRIVER_SCHEDULED_API

static int nrf91_schedule(const struct device *dev, struct dectnrp_driver_op *op)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(op);
	return -ENOSYS;
}

#endif

#if CONFIG_DECTNRP_DRIVER_BLOCKING_API

static int nrf91_set_channel(const struct device *dev, uint16_t channel)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(channel);
	return -ENOSYS;
}

static int nrf91_rx(const struct device *dev, struct dectnrp_rx_mode mode, net_time_t start,
		    uint32_t duration, struct net_pkt *pkt)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(mode);
	ARG_UNUSED(start);
	ARG_UNUSED(duration);
	ARG_UNUSED(pkt);
	return -ENOSYS;
}

static int nrf91_tx(const struct device *dev, struct dectnrp_tx_mode mode, struct net_pkt *pkt,
		    struct net_buf *frag)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(mode);
	ARG_UNUSED(pkt);
	ARG_UNUSED(frag);
	return -ENOSYS;
}

static int nrf91_rssi1(const struct device *dev, struct dectnrp_rssi_mode mode, uint32_t subslots,
		       struct dectnrp_rssi1_result *result)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(mode);
	ARG_UNUSED(subslots);
	ARG_UNUSED(result);
	return -ENOSYS;
}

#endif /* CONFIG_DECTNRP_DRIVER_BLOCKING_API */

struct nrf91_context {
	struct net_if *iface;
};

static struct nrf91_context context = {0};

static void nrf91_net_if_init(struct net_if *iface)
{
	if (context.iface != NULL) {
		return;
	}

	LOG_DBG("nRF91 initializing...");
	context.iface = iface;
	dectnrp_l2_init(context.iface);

	return;
}

static const struct dectnrp_driver_api nrf91_api = {
	.iface_api =
		{
			.init = nrf91_net_if_init,
		},
	.start = nrf91_start,
	.stop = nrf91_stop,
	.get_capabilities = nrf91_get_capabilities,
	.configure = nrf91_configure,
	.get_time = nrf91_get_time,
#if CONFIG_DECTNRP_DRIVER_SCHEDULED_API
	.schedule = nrf91_schedule,
#endif
#if CONFIG_DECTNRP_DRIVER_BLOCKING_API
	.set_channel = nrf91_set_channel,
	.rx = nrf91_rx,
	.tx = nrf91_tx,
	.rssi1 = nrf91_rssi1,
#endif
};

/* FIXME: dummy value */
#define NRF91_MTU 1024

NET_DEVICE_INIT(nrf91_dectnrp, "nrf91_dectnrp", NULL, NULL, &context, NULL,
		CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &nrf91_api, DUMMY_L2,
		NET_L2_GET_CTX_TYPE(DUMMY_L2), NRF91_MTU);
