/* DECT NR+ driver API
 *
 * Copyright (c) 2026 Deveritec GmbH.
 * Copyright (c) 2026 Codium Electronique
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public DECT NR+ Radio Driver API
 *
 * @attention This file contains pseudo code for demonstration purposes.
 * Definitions may be missing and in that case are only place
 * holders for types representing related objects.
 */

#ifndef ZEPHYR_NET_DECTNRP_DRIVER_H_
#define ZEPHYR_NET_DECTNRP_DRIVER_H_

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/net_buf.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup dectnrp_driver DECT NR+ Driver API
 * @version 0.0.1
 * @ingroup dectnrp
 *
 * @brief DECT NR+ driver API
 *
 * @details This API provides a common representation of vendor-specific
 * hardware and firmware to the DECT NR+ L2.
 * **Application developers should never interface directly with this API.** It
 * is of interest to driver maintainers only.
 *
 * Implementing the basic driver API will ensure integration with the native L2
 * stack as well as basic support for DECT NR+.
 *
 * @note References are to the ETSI TS 103 636 DECTNRP NR+ V2.1.1 (2024-10) standard
 * If not further noted all references in this file refer to ETSI TS 103 636-4.
 *
 * @{
 */

/*
 * FIXME: Stub structs until proper definitions
 */
struct device_capabilities {
	uint8_t _dummy;
};

struct dectnrp_config {
	uint8_t _dummy;
};

enum dectnrp_config_type {
	DECTNRP_CONFIG_TYPE_UNKNOWN = 0,
};

struct dectnrp_rssi1_result {
	uint8_t _dummy;
};

struct dectnrp_rssi_mode {
	uint8_t _dummy;
};

/**
 * @brief Flags related to DECTNRP_DRIVER_OP_TX
 */
struct dectnrp_tx_mode {
	uint8_t is_scheduled: 1;
	uint8_t is_lbt: 1;
	uint8_t is_harq: 1;
	uint8_t is_beacon: 1;
};

/**
 * @brief Flags related to DECTNRP_DRIVER_OP_RX
 */
struct dectnrp_rx_mode {
	uint8_t is_scheduled: 1;
	uint8_t is_beacon: 1;
};

#ifdef CONFIG_DECTNRP_DRIVER_SCHEDULED_API

/**
 * @brief Enumerates all possible driver operation types.
 */
enum dectnrp_driver_op_type {
	DECTNRP_DRIVER_OP_TX,
	DECTNRP_DRIVER_OP_RX,
	DECTNRP_DRIVER_OP_RSSI1,
};

/**
 * @brief Descriptor of one driver operation.
 */
struct dectnrp_driver_op {

	/** Channel this operation should be scheduled into. */
	uint16_t channel;
	/** Start time this operation should be scheduled at in
	 *  modem ticks.
	 *  To be scheduled immediately if start_time == 0.
	 */
	net_time_t start_time;
	/** Operation type. */
	enum dectnrp_driver_op_type type;
	union {
		/** Parameters when type == DECTNRP_DRIVER_OP_TX. */
		struct {
			struct dectnrp_tx_mode mode;
			const void *resource;
			struct net_pkt *pkt;
			/** Parameters when mode.is_lbt == true. */
			struct {
				uint32_t period;
				uint8_t rssi_threshold;
				uint8_t retry_count;
			} lbt;
		} tx;
		/** Parameters when type == DECTNRP_DRIVER_OP_RX. */
		struct {
			struct dectnrp_rx_mode mode;
			const void *resource;
			uint32_t duration;
			uint8_t expected_rssi;
		} rx;
		/** Parameters when type == DECTNRP_DRIVER_OP_RSSI1. */
		struct {
			uint32_t subslots;
			struct dectnrp_rssi1_result *result;
		} rssi1;
	};

	/** Holds the status of this operation. */
	int status;
};

/**
 * @brief Enumerate all events notified via dectnrp_event_cb_t.
 */
enum dectnrp_event {
	/** An operation has been finished */
	DECTNRP_EVENT_OP_FINISHED,
	/** Provides RSSI1 results. */
	DECTNRP_EVENT_OP_RESULTS,
};

union dectnrp_event_data {
	union {
		/** DECTNRP_EVENT_OP_FINISHED */
		/** DECTNRP_EVENT_OP_RESULTS */
		struct {
			struct dectnrp_driver_op *op;
		} op_finished;
	};
};

/** Event callback function  */
typedef void (*dectnrp_event_cb_t)(const struct device *dev, enum dectnrp_event evt,
				   union dectnrp_event_data *event_data);

#endif /* CONFIG_DECTNRP_DRIVER_SCHEDULED_API */

/**
 * @brief DECTNRP driver interface API.
 *
 * @details While L1-level driver features are exclusively implemented by
 * drivers and MAY be mandatory to support certain application requirements, L2
 * features SHOULD be optional by default and only need to be implemented for
 * performance optimization or precise timing as deemed necessary by driver
 * maintainers. Fallback implementations ("Soft MAC") SHOULD be provided in the
 * driver-independent L2 layer for all L2/MAC features especially if these
 * features are not implemented in vendor hardware/firmware by a majority of
 * existing in-tree drivers. If, however, a driver offers offloading
 * opportunities then L2 implementations SHALL delegate performance critical or
 * resource intensive tasks to the driver.
 *
 * All drivers SHALL support two externally observable interface operational
 * states: "UP" and "DOWN". Drivers MAY additionally support a "TESTING"
 * interface state (see `continuous_carrier()`).
 *
 * The following rules apply:
 * * An interface is considered "UP" when it is able to transmit and receive
 *   packets, "DOWN" otherwise (see precise definitions of the corresponding
 *   ifOperStatus values in RFC 2863, section 3.1.14.
 * * Upper layers will assume that the interface managed by the driver is "UP"
 *   after a call to `start()` returned zero or `-EALREADY`. Upper layers assume
 *   that the interface is "DOWN" after calling `stop()` returned zero or
 *   `-EALREADY`.
 * * The driver SHALL block `start()`/`stop()` calls until the interface fully
 *   transitioned to the new state (e.g. the receiver is operational, ongoing
 *   transmissions were finished, etc.). Drivers SHOULD yield the calling thread
 *   (i.e. "sleep") if waiting for the new state without CPU interaction is
 *   possible.
 * * Drivers are responsible of guaranteeing atomicity of state changes.
 *   Appropriate means of synchronization SHALL be implemented (locking, atomic
 *   flags, ...).
 * * The driver SHALL NOT change the interface's "UP"/"DOWN" state on its own.
 *   Initially, the interface SHALL be in the "DOWN" state.
 * * If calls to `start()`/`stop()` return any other value than zero or
 *   `-EALREADY`, upper layers will consider the interface to be in a
 *   "lowerLayerDown" state as defined in RFC 2863.
 * * The RFC 2863 "dormant", "unknown" and "notPresent" ifOperStatus states are
 *   currently not supported.
 *
 */
struct dectnrp_driver_api {

	/**
	 * @brief network interface API
	 *
	 * @note Network devices must extend the network interface API. It is
	 * therefore mandatory to place it at the top of the driver API struct so
	 * that it can be cast to a network interface.
	 */
	struct net_if_api iface_api;

	/**
	 * @brief Start the device.
	 *
	 * @param dev pointer to DECTNRP driver device
	 *
	 * @retval 0 The driver was successfully started.
	 * @retval -EIO The driver could not be started.
	 */
	int (*start)(const struct device *dev);

	/**
	 * @brief Stop the device.
	 *
	 * @param dev pointer to DECTNRP driver device
	 *
	 * @retval 0 The driver was successfully stopped.
	 * @retval -EIO The driver could not be stopped.
	 */
	int (*stop)(const struct device *dev);

	/**
	 * @brief Get the device driver capabilities.
	 *
	 * @param dev pointer to DECTNRP driver device
	 * @param[out] caps pointer to capabilities provided by @p dev
	 *
	 * @retval 0 Capabilities successfully written to @p cap.
	 * @retval -EIO The driver has some error.
	 */
	int (*get_capabilities)(const struct device *dev, struct device_capabilities *caps);

	/**
	 * @brief Set or update driver configuration.
	 *
	 * @details The method blocks until the interface has been reconfigured
	 * atomically with respect to ongoing package reception, transmission or
	 * any other ongoing driver operation.
	 *
	 * @param dev pointer to DECTNRP driver device
	 * @param type the configuration type to be set
	 * @param config the configuration parameters to be set for the given
	 * configuration type
	 *
	 * @retval 0 configuration successful
	 * @retval -EINVAL The configuration parameters are invalid for the
	 * given configuration type.
	 * @retval -ENOTSUP The given configuration type is not supported by
	 * this driver.
	 * @retval -EACCES The given configuration type is supported by this
	 * driver but cannot be configured in the current interface operational
	 * state.
	 * @retval -ENOMEM The configuration cannot be saved due to missing
	 * memory resources.
	 * @retval -ENOENT The resource referenced in the configuration
	 * parameters cannot be found in the configuration.
	 * @retval -EIO An internal error occurred while trying to configure the
	 * given configuration parameter.
	 */
	int (*configure)(const struct device *dev, enum dectnrp_config_type type,
			 const struct dectnrp_config *config);

	/**
	 * @brief Get the current modem time.
	 *
	 * @param dev pointer to DECTNRP driver device
	 *
	 * @return nanoseconds relative to the network subsystem's local clock,
	 * -1 if an error occurred or the operation is not supported
	 */
	net_time_t (*get_time)(const struct device *dev);

#ifdef CONFIG_DECTNRP_DRIVER_SCHEDULED_API

	/**
	 * @brief Schedule given operation @p op.
	 *
	 * @note This function is non-blocking and just provides @p op to the driver which
	 * in turn notifies completion back via dectnrp_event_cb_t.
	 *
	 * @param dev pointer to DECTNRP driver device
	 * @param op the operation to be scheduled.
	 *
	 * @retval 0 Operation @p op successfully scheduled.
	 * @retval -EIO The driver has some error.
	 */
	int (*schedule)(const struct device *dev, struct dectnrp_driver_op *op);

#endif /* CONFIG_DECTNRP_DRIVER_SCHEDULED_API */

#ifdef CONFIG_DECTNRP_DRIVER_BLOCKING_API

	/**
	 * @brief Set current channel.
	 *
	 * @param dev pointer to DECTNRP driver device
	 * @param channel the number of the channel to be set.
	 *
	 * @retval 0 channel was successfully set
	 * @retval -EIO The channel could not be set.
	 */
	int (*set_channel)(const struct device *dev, uint16_t channel);

	/**
	 * @brief Receive a packet fragment as a single message.
	 *
	 * @note The radio channel must be set prior to calling this function.
	 *
	 * @param dev pointer to DECTNRP driver device
	 * @param mode the reception mode flags.
	 * @param start
	 * @param duration
	 * @param[in,out] pkt
	 *
	 */
	int (*rx)(const struct device *dev, struct dectnrp_rx_mode mode, net_time_t start,
		  uint32_t duration, struct net_pkt *pkt);

	/**
	 * @brief Transmit a packet fragment as a single message
	 *
	 * @note The radio channel must be set prior to calling this function.
	 *
	 * @param dev pointer to DECTNRP driver device
	 * @param mode the transmission mode flags.
	 * @param pkt pointer to the network packet to be transmitted.
	 * @param frag pointer to a network buffer containing a single fragment
	 * with the frame data to be transmitted
	 *
	 * @retval 0 The frame was successfully sent or scheduled.
	 * @retval -EIO The frame could not be sent due to some unspecified
	 * driver error (e.g. the driver being busy).
	 */
	int (*tx)(const struct device *dev, struct dectnrp_tx_mode mode, struct net_pkt *pkt,
		  struct net_buf *frag);

	/**
	 * @brief Run a RSSI-1 scan on current channel
	 *
	 * @note The radio channel must be set prior to calling this function.
	 *
	 * @param dev pointer to DECTNRP driver device
	 * @param mode the rssi1 mode flags.
	 * @param subslots.
	 * @param[out] result
	 *
	 * @retval 0 The rssi1 operation frame was successfully scheduled.
	 */
	int (*rssi1)(const struct device *dev, struct dectnrp_rssi_mode mode, uint32_t subslots,
		     struct dectnrp_rssi1_result *result);

#endif /* CONFIG_DECTNRP_DRIVER_BLOCKING_API */
};

/* Make sure that the network interface API is properly setup inside
 * DECTNRP driver API struct (it is the first one).
 */
BUILD_ASSERT(offsetof(struct dectnrp_driver_api, iface_api) == 0);

/**
 * @name DECTNRP driver callbacks
 * @{
 */

/**
 * @brief DECTNRP driver initialization callback into L2 called by drivers
 *        to initialize the active L2 stack for a given interface.
 *
 * @details Drivers must call this function as part of their own initialization
 *          routine.
 *
 *          Note: This function is part of Zephyr's DECTNRP stack driver -> L2
 *          "inversion-of-control" adaptation API and must be implemented by all
 *          DECTNRP L2 stacks.
 *
 * @param iface A valid pointer on a network interface
 */
void dectnrp_l2_init(struct net_if *iface);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_NET_DECTNRP_DRIVER_H_ */
