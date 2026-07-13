/* DECT NR+ driver API
 *
 * Copyright (c) 2026 Deveritec GmbH
 * Copyright (c) 2026 Codium Electronique
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public DECT NR+ Radio Driver API
 *
 * @note All references to the standard in this file cite ETSI TS 103 636 DECTNRP NR+ V2.2.1 (2026-05).
 * 
 * @attention This file contains code which is work in progress.
 * Interfaces and objects may be not fully consistent.
 * Be aware of some loose ends and unfinished features.
 */

#ifndef ZEPHYR_NET_DECTNRP_DRIVER_H_
#define ZEPHYR_NET_DECTNRP_DRIVER_H_

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/net_buf.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_time.h>
#include <zephyr/net/dectnrp.h>

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
 * is of interest to driver/stack maintainers only.
 *
 * Implementing the basic driver API will ensure integration with the native L2
 * stack as well as basic support for DECT NR+.
 *
 * @note References are to the ETSI TS 103 636 DECTNRP NR+ V2.2.1 (2026-05) standard
 * If not further noted all references in this file refer to ETSI TS 103 636-4.
 *
 * @{
 */

/**
 * @brief One RSSI1 measurement result.
 * 
 */
struct dectnrp_rssi1_result {
	/** The result holds the rssi1 dbm values for each sub-slot for one frame. */
	uint8_t subslot[DECTNRP_SUBSLOTS_PER_FRAME];
	/** Related channel */
	uint16_t channel;
};

/**
 * @brief Flags related to DECTNRP_DRIVER_OP_TX
 */
struct dectnrp_tx_mode {
	/** The tx operation is a scheduled one. If true a start-time has to be provided. */
	uint8_t is_scheduled: 1;
	/** The tx operation should done together with LBT. */
	uint8_t is_lbt: 1;
	/** The tx operation should done using harq. */
	uint8_t is_harq: 1;
	/** The tx operation sends a beacon. The driver may not be interested in this field. */
	uint8_t is_beacon: 1;
};

/**
 * @brief Flags related to DECTNRP_DRIVER_OP_RX
 */
struct dectnrp_rx_mode {
	/** The rx operation is a scheduled one. If true a start-time has to be provided. */
	uint8_t is_scheduled: 1;
	/** The rx operation wants to receive a beacon. The driver may not be interested in this field. */
	uint8_t is_beacon: 1;
};

/**
 * @brief Flags related to DECTNRP_DRIVER_OP_RSSI1
 */
struct dectnrp_rssi1_mode {
	/** The rssi1 operation is a scheduled one. If true a start-time has to be provided. */
	uint8_t is_scheduled: 1;
};

#ifdef CONFIG_DECTNRP_DRIVER_SCHEDULED_API

/**
 * @brief Enumerates all possible driver operation types.
 */
enum dectnrp_driver_op_type {
	/** The related operation is of tx type. */
	DECTNRP_DRIVER_OP_TX,
	/** The related operation is of rx type. */
	DECTNRP_DRIVER_OP_RX,
	/** The related operation is of rssi1 type. */
	DECTNRP_DRIVER_OP_RSSI1,
};

/**
 * List entry holds one parameter set (tbs, µ, beta, mcs, slots)
 *
 * See ETSI TS 103 636-3 V2.2.1 (2026-05) Table C.2-1 and Table C.3-1
 *
 */
struct dectnrp_transport_parameters {
	/** Transport Block Size [bytes] */
	uint16_t tbs;
	/** µ - 1 based */
	uint8_t mu;
	/** beta - 1 based */
	uint8_t beta;
	/** mcs - 0 based */
	uint8_t mcs;
	/** slots - 1 based */
	uint8_t slots;
};

/**
 * @brief Descriptor of one driver operation.
 */
struct dectnrp_driver_op {

	/** Channel this operation should be scheduled into. */
	uint16_t channel;
	/** Start time this operation should be scheduled at in
	 *  modem ticks.
	 *  If it should be scheduled immediately set start_time == 0.
	 */
	net_time_t start_time;
	/** Type of this operation. */
	enum dectnrp_driver_op_type type;
	union {
		/** Parameters when type == DECTNRP_DRIVER_OP_TX. */
		struct {
			/** Mode flags related to this tx operation. */
			struct dectnrp_tx_mode mode;
			/** Type of the PHY header encoded in pkt.
			 * * phy_header_type=0 - PHY type 1 (size=5 bytes)
			 * * phy_header_type=1 - PHY type 2 (size=10 bytes)
			 */
			uint8_t phy_header_type :1;
			/** Transport parameters related to this operation */
			struct dectnrp_transport_parameters transport_parameters;
			/** Packet to be sent. */
			struct net_pkt *pkt;
			/** Resource related to this operation - not used yet. */
			const void *resource;
			/** Parameters when mode.is_lbt == true. */
			struct {
				/** Period of LBT in modem ticks. */
				uint32_t period;
				/** RSSI threshold above a channel is detected as free/possible. */
				uint8_t rssi_threshold;
				/** Current count value. */
				uint8_t retry_count;
			} lbt;
		} tx;
		/** Parameters when type == DECTNRP_DRIVER_OP_RX. */
		struct {
			/** Mode flags related to this rx operation. */
			struct dectnrp_rx_mode mode;
			/** Resource related to this operation - not used yet. */
			const void *resource;
			/** The length of the RX window in modem ticks. */
			net_time_t duration;
			/** The RSSI level the modem can expect for this RX window.
			 *  @todo This is taken directly from Nordic API. A more generic 
			 *  approach should be elaborated.
			 */
			uint8_t expected_rssi;
		} rx;
		/** Parameters when type == DECTNRP_DRIVER_OP_RSSI1. */
		struct {
			/** Mode flags related to this RSSI1 operation. */
			struct dectnrp_rssi1_mode mode;
			/** Number of subslots to be measured. */
			uint32_t subslots;
			/** Reference to result storage where measured RSSI1 values should be stored.
			 * Memory has to be kept available by the caller until DECTNRP_EVENT_OP_RESULTS is notified.
			 */
			struct dectnrp_rssi1_result *result;
		} rssi1;
	};

	/** Holds the status of this operation. 
	 *  @todo use generic (dectnrp specific) but vendor independent error codes! 
	 */
	int status;
};

/**
 * @brief Enumerate all events notified via dectnrp_driver_event_cb_t.
 */
enum dectnrp_driver_event_code {
	DECTNRP_EVENT_NONE,
	/** An message error occured. */
	DECTNRP_EVENT_MSG_ERROR,
	/** An message has been rececived. */
	DECTNRP_EVENT_MSG_RECEIVED,
	/** An operation has been finished. */
	DECTNRP_EVENT_OP_FINISHED,
	/** Provides RSSI1 results. */
	DECTNRP_EVENT_OP_RESULTS,
};

enum dectnrp_driver_error {
	DECTNRP_ERROR_NO_ERROR,
	DECTNRP_ERROR_PCC_CRC,
	DECTNRP_ERROR_PDC_CRC,
};

struct dectnrp_driver_event {
	/** Holds the code related to this event. */
	enum dectnrp_driver_event_code code;

	union {

		/** DECTNRP_EVENT_MSG_ERROR */
		struct {
			/** Operation related to this message error. */
			struct dectnrp_driver_op *op;
			/** Holds the status of this operation. 
			 *  FIXME use generic (dectnrp specific) but vendor independent error codes! 
			 */
			int status;
			/** rssi2 contains a valid RSSI2 value. */
			uint8_t rssi2_valid :1;
			/** snr contains a valid RSSI2 value. */
			uint8_t snr_valid :1;
			/** pcc contains a valid PCC header value. */
			uint8_t pcc_valid :1;
			/** Type of the received PCC. */
			uint8_t phy_type :1;

			/** Related start time. */
			net_time_t start_time;
			/** Points to PCC if pcc_valid == true.  */
			const uint8_t *pcc;
			/** RSSI2 value of this message.
			 *  Encoded according to @ref ETSI TS 103 636-2 chapter 8.3.
			 * 
			 * @todo It needs to be decided whether we want to transmit the RSSI2 according to spec.
			 */
			uint8_t rssi2;
			/** SNR value of this message.
			 *  Encoded according to @ref ETSI TS 103 636-2 chapter 8.4.
			 * 
			 * @todo It needs to be decided whether we want to transmit the SNR according to sp
			 */
			int8_t snr;
		} message_error;

		/** DECTNRP_EVENT_MSG_RECEIVED */
		struct {
			struct dectnrp_driver_op *op;
			/** Holds the status of this operation. 
			 *  FIXME use generic (dectnrp specific) but vendor independent error codes! 
			 */
			int status;
			/** Type of the received PCC. */
			uint8_t phy_type :1;
			/** rssi2 contains a valid RSSI2 value. */
			uint8_t rssi2_valid :1;
			/** snr contains a valid RSSI2 value. */
			uint8_t snr_valid :1;

			/** Related start time. */
			net_time_t start_time;
			/** Points to PCC.  */
			const uint8_t *pcc;
			/** Points to PDC.  */
			const uint8_t *pdc;
			/** Number of bytes of PDC. */
			uint32_t pdc_len;
			/** RSSI2 value of this message.
			 *  Encoded according to @ref ETSI TS 103 636-2 chapter 8.3.
			 * 
			 * @todo It needs to be decided whether we want to transmit the RSSI2 according to spec.
			 */
			uint8_t rssi2;
			/** SNR value of this message.
			 *  Encoded according to @ref ETSI TS 103 636-2 chapter 8.4.
			 * 
			 * @todo It needs to be decided whether we want to transmit the SNR according to sp
			 */
			int8_t snr;

		} msg_received;

		/** DECTNRP_EVENT_OP_RESULTS */
		struct {
			struct dectnrp_driver_op *op;
			/** Holds the status of this operation. 
			 *  FIXME use generic (dectnrp specific) but vendor independent error codes! 
			 */
			int status;
		} rssi1_result;

		/** DECTNRP_EVENT_OP_FINISHED */
		/** DECTNRP_EVENT_OP_RESULTS */
		struct {
			struct dectnrp_driver_op *op;
		} op_finished;
	};
};

#endif /* CONFIG_DECTNRP_DRIVER_SCHEDULED_API */

/** Event callback function the dectnrp_driver is notifying events up to the stack.  */
typedef void (*dectnrp_driver_event_cb_t)(const struct device *dev, const struct dectnrp_driver_event *event);

/**
 * @brief This flags supported features.
 * 
 * @attention Work in progress. 
 * @todo We have to discuss the feature set.
 * 
 */
enum dectnrp_hw_caps {
	DECTNRP_HW_NO_CAPABILITY = 0x0,

	DECTNRP_HW_PROMISC = BIT(1),   /* Promiscuous mode supported. */
	DECTNRP_HW_FILTER = BIT(2),    /* Filter NETWORK ID, long/short addr. */
	DECTNRP_HW_TXTIME = BIT(8),    /* TX at specified time supported. */
	DECTNRP_HW_RXTIME = BIT(11),   /* RX at specified time supported. */
	DECTNRP_HW_SYNCTIME = BIT(12), /* Synchronization via GPIO supported. */
};

/**
 * @brief Keeps supported features and capabilities of the driver/radio.
 * 
 * @attention Work in progress.
 * @todo We have to add more capabilities/features of driver/radio 
 * which are/should be available/configurable at runtime.
 * 
 */
struct dectnrp_device_capabilities {
	enum dectnrp_hw_caps hw_caps;
};

enum dectnrp_config_type {
	/** Value to distinguish uninitialized configurations. */
	DECTNRP_CONFIG_TYPE_UNKNOWN = 0,
	/** Specifies new driver event handler. Specifying NULL as a handler
	 *  will disable driver events notification.
	 */
	DECTNRP_CONFIG_TYPE_EVENT_HANDLER,
	/** Configure network id this device should belong to. */
	DECTNRP_CONFIG_TYPE_NETWORK_ID,
	/** Configure network id filter. */
	DECTNRP_CONFIG_TYPE_FILTER_SHORT_NETWORK_ID,
	/** Configure short receiver address filter. */
	DECTNRP_CONFIG_TYPE_FILTER_SHORT_RECEIVER_ADDR,
};

/** dectnrp driver configuration data. */
struct dectnrp_config {
	/** Configuration data. */
	union {
		/** Parameters when type == DECTNRP_CONFIG_TYPE_EVENT_HANDLER. */
		dectnrp_driver_event_cb_t event_handler;
		/** Parameters when type == DECTNRP_CONFIG_TYPE_NETWORK_ID. */
		uint32_t network_id;
		/** Parameters when type == DECTNRP_CONFIG_TYPE_FILTER_SHORT_NETWORK_ID. */
		struct {
			/** Activates filter when true, deactivates filter when false. */
			bool activate;
			/** Short network-id. */
			uint8_t value;
		} filter_short_network_id;
		/** Parameters when type == DECTNRP_CONFIG_TYPE_FILTER_SHORT_RECEIVER_ADDR. */
		struct {
			/** Activates filter when true, deactivates filter when false. */
			bool activate;
			/** Short short receiver-id. 
			 * @attention This only applies to packages of PHY TYPE 2! Packages are filtered by the receiver
 			 * identity encoded in the PHY TYPE 2 header.
 			 * PHY TYPE 1 packages have not short address attached and are not be filtered!
			*/
			uint16_t value;
		} filter_short_receiver_addr;
	};
};

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
	 * @brief Get the device hardware capabilities.
	 *
	 * @param dev pointer to DECTNRP driver device
	 *
	 * @return Bit field with all supported device driver capabilities.
	 */
	enum dectnrp_hw_caps (*get_hw_capabilities)(const struct device *dev);

	/**
	 * @brief Get the device driver capabilities.
	 *
	 * @param dev pointer to DECTNRP driver device
	 * @param[out] caps pointer to capabilities provided by @p dev
	 *
	 * @retval 0 Capabilities successfully written to @p cap.
	 * @retval -EIO The driver has some error.
	 */
	int (*get_capabilities)(const struct device *dev, struct dectnrp_device_capabilities *caps);

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
	 * @attention The returned modem time is in modem ticks!
	 * 
	 * @note We use modem time because one tick of it is 1/691200 seconds (~14,46 ns
	 * per tick) - modem_time2ns gives rounding errors in that case.
	 * 
	 * @param dev pointer to DECTNRP driver device
	 * @param time[out] The modem time has been copied into it when 0 has been returned.
	 * @return 0 time successfully read into @p time
	 */
	int (*get_time)(const struct device *dev, net_time_t *time);

#ifdef CONFIG_DECTNRP_DRIVER_SCHEDULED_API

	/**
	 * @brief Schedule given operation @p op.
	 *
	 * @note This function is non-blocking and just provides @p op to the driver which
	 * in turn notifies completion back via dectnrp_driver_event_cb_t.
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
	int (*rssi1)(const struct device *dev, struct dectnrp_rssi1_mode mode, uint32_t subslots,
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
