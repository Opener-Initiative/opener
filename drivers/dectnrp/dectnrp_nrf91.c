/* Nordic nRF91x1 NR+ PHY driver
 *
 * Copyright (c) 2026 Deveritec GmbH
 * Copyright (c) 2026 Codium Electronique
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT dectnrp_driver_nrf91

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dectnrp_nrf91, CONFIG_DECTNRP_DRIVER_NRF91_LOG_LEVEL);

#include <ncs_version.h>
#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/net/net_time.h>
#include <zephyr/net/net_pkt.h>
#include <modem/nrf_modem_lib.h>
#include <nrf_modem_dect_phy.h>
#include <nrf_modem_at.h>
#include <zephyr/net/dectnrp_driver.h>

#if NCS_VERSION_NUMBER < 0x30000
#error "Your nrf ncs version " NCS_VERSION_STRING " is too old, >= 0x30000 expected!"
#endif /* NCS_VERSION_NUMBER */

#define MS_2_MODEM_TICKS(MS)  (MS * NRF_MODEM_DECT_MODEM_TIME_TICK_RATE_KHZ)
#define FRAME_MODEM_TIME_TICK MS_2_MODEM_TICKS(10)

/**
 * @brief Enumerates supported MFW versions.
 */
enum nrf91_mfw_version {
	RADIO_MFW_UNKNOWN = 0,
	RADIO_MFW_PHY_1_1_0,
	RADIO_MFW_PHY_2_0_0,
};

/**
 * @brief Enumerates common states of this driver. 
 */
enum nrf91_state {
	RADIO_STATE_UNKNOWN = 0x0,
	RADIO_STATE_INITIALIZED = 0x1,
	RADIO_STATE_STARTED = 0x2,
};

/**
 * @brief Enumerates rx states of this driver.
 * 
 * This is a temporarily solution.
 */
enum nrf91_rx_state {
	RADIO_RXSTATE_UNKNOWN = 0x0,
	RADIO_RXSTATE_PCC = 0x1,
	RADIO_RXSTATE_PDC = 0x2,
};

/**
 * @brief The context related to the one and only open nrf91 radio device.
 */
struct nrf91_context {

	/** Interface related to this nrf91 radio device. */
	struct net_if *iface;
	/** Semaphore to lock radio for atomar transactions. */
	struct k_sem modemlock;
	/** Semaphore to lock radio for operations. */
	struct k_sem operation;
	/** State of this nrf91 radio device. */
	atomic_t state;

	/** Marks the modem firmware version. */
	enum nrf91_mfw_version mfw_version;
	/** Limit of the modem temperature. */
	int16_t temperature_limit;
	/** Status of last modem transaction. */
	struct {
		enum nrf_modem_dect_phy_err last_status;
		uint64_t time;
		int16_t temperature;
		uint16_t voltage;
		uint8_t mu;
		uint8_t beta;
	} modem;

	/** Configured network-id. */
	uint32_t network_id;
	/** Registered handler to notify upper layer. */
	dectnrp_driver_event_cb_t upper_layer_event_handler;

	struct {
		uint16_t count;
	} tx;

	struct {
		atomic_t state;
		uint16_t count;
		uint16_t transaction_id;
		struct nrf_modem_dect_phy_pcc_event pcc_event;
		uint32_t crc_errors_phy_header;
		uint32_t crc_errors_data;

		struct {
			uint8_t short_network_id_active: 1;
			uint32_t short_network_id;
			uint8_t short_addr_active: 1;
			uint16_t short_addr;
		} filter;
	} rx;
};

/** Singleton device needed to back-reference the context from modem callbacks. */
static const struct device *nrf91_dev = NULL;

/**
 * @brief Convert Nordic specific RSSI2 value @p rssi2 (Q14.1 format) 
 * into format according to @ref ETSI TS 103 636-2 chapter 8.3.
 * 
 * @param rssi2 
 * @return uint8_t 
 */
static inline uint8_t dectnrp_nrf91_encode_rssi2(int16_t rssi2)
{
	rssi2 = rssi2 >> 1;

	if (rssi2 > -1) {
		return 0xff;
	}

	if (rssi2 < -140) {
		return 0x74;
	}

	return (uint8_t)(0x00ff & rssi2);
}

/**
 * @brief Convert Nordic specific SNR value @p snr (Q13.2 format) 
 * into format according to @ref ETSI TS 103 636-2 chapter 8.4. 
 * 
 * @param snr 
 * @return int8_t 
 */
static inline int8_t dectnrp_nrf91_encode_snr(int16_t snr)
{
	int16_t snr_q_14_1 = (snr + 1) >> 1;

	if (snr_q_14_1 > 0x7f) {
		return 0x7f;
	}

	if (snr_q_14_1 < (int16_t)0xffffffe0) {
		return 0xe0;
	}

	return (int8_t)(snr_q_14_1 & 0x00ff);
}

/**
 * @brief Event callback returning the state after modem initialization by nrf_modem_dect_phy_init.
 *
 * @attention This function should be used in blocking mode, it releases ctx->modemlock!
 *
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param event 'NRF_MODEM_DECT_PHY_EVT_INIT' event.
 */
static void modem_init_cb(struct nrf91_context *ctx, const uint64_t *time, 
		 const struct nrf_modem_dect_phy_init_event *event)
{
	if (event->err == NRF_MODEM_DECT_PHY_SUCCESS) {
		LOG_DBG("<%lld> PHY_EVT_INIT,%dV,%u°C,max=%u°C", *time, event->voltage, event->temp,
			event->temperature_limit);
	} else {
		LOG_ERR("<%lld> PHY_EVT_INIT failed,0x%x", *time, event->err);
	}
	ctx->temperature_limit = event->temperature_limit;
	// TODO Check temperature regularly and switch off when exceeding temperature_limit!
	// Otherwise you'll kill your transmitter!
	ctx->modem.time = *time;
	ctx->modem.temperature = event->temp;
	ctx->modem.voltage = event->voltage;
	ctx->modem.last_status = event->err;
	k_sem_give(&ctx->modemlock);
}

/**
 * @brief Event callback returning the state after modem deinitialization by nrf_modem_dect_phy_deinit.
 *
 * @attention This function should be used in blocking mode, it releases ctx->modemlock!
 *
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param event 'NRF_MODEM_DECT_PHY_EVT_DEINIT' event.
 */
static void modem_deinit_cb(struct nrf91_context *ctx, const uint64_t *time, 
		 const struct nrf_modem_dect_phy_deinit_event *event)
{
	if (event->err == NRF_MODEM_DECT_PHY_SUCCESS) {
		LOG_DBG("<%lld> PHY_EVT_DEINIT", *time);
	} else {
		LOG_ERR("PHY_EVT_DEINIT failed,0x%x", event->err);
	}
	ctx->modem.time = *time;
	ctx->modem.last_status = event->err;
	k_sem_give(&ctx->modemlock);
}

/**
 * @brief Event callback returning the state after modem configuration by nrf_modem_dect_phy_configure.
 *
 * @attention This function should be used in blocking mode, it releases ctx->modemlock!
 *
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param event 'NRF_MODEM_DECT_PHY_EVT_CONFIGURE' event.
 */
static void modem_configure_cb(struct nrf91_context *ctx, const uint64_t *time, 
		 const struct nrf_modem_dect_phy_configure_event *event)
{
	if (event->err == NRF_MODEM_DECT_PHY_SUCCESS) {
		LOG_DBG("<%lld> PHY_EVT_CONFIGURE success,%d°C,%umV", *time,
			event->temp, event->voltage);
	} else {
		LOG_ERR("PHY_EVT_CONFIGURE failed,0x%x,%d°C,%umV", event->err,
			event->temp, event->voltage);
	}
	ctx->modem.time = *time;
	ctx->modem.temperature = event->temp;
	ctx->modem.voltage = event->voltage;
	ctx->modem.last_status = event->err;
	k_sem_give(&ctx->modemlock);
}

/**
 * @brief Event callback returning the state after radio configuration by nrf_modem_dect_phy_radio_config.
 *
 * @attention This function is used in asynchronous mode!
 *
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param event 'NRF_MODEM_DECT_PHY_EVT_RADIO_CONFIG' event.
 */
static void modem_radio_config_cb(struct nrf91_context *ctx, const uint64_t *time, 
		 const struct nrf_modem_dect_phy_radio_config_event *event)
{
	if (event->err == NRF_MODEM_DECT_PHY_SUCCESS) {
		LOG_INF("<%lld> PHY_EVT_RADIO_CONFIG", *time);
	} else {
		LOG_ERR("<%lld> PHY_EVT_RADIO_CONFIG failed,0x%x", *time, event->err);
	}
	ctx->modem.time = *time;
	ctx->modem.last_status = event->err;
}

/**
 * @brief Event callback returning the state after phy activation by nrf_modem_dect_phy_activate.
 *
 * @attention This function should be used in blocking mode, it releases ctx->modemlock!
 *
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param event 'NRF_MODEM_DECT_PHY_EVT_ACTIVATE' event.
 */
static void modem_activate_cb(struct nrf91_context *ctx, const uint64_t *time, 
		 const struct nrf_modem_dect_phy_activate_event *event)
{
	if (event->err == NRF_MODEM_DECT_PHY_SUCCESS) {
		LOG_DBG("<%lld> PHY_EVT_ACTIVATE,%d°C,%umV", *time, event->temp, event->voltage);
	} else {
		LOG_ERR("<%lld> PHY_EVT_ACTIVATE failed,0x%x", *time, event->err);
	}
	ctx->modem.time = *time;
	ctx->modem.temperature = event->temp;
	ctx->modem.voltage = event->voltage;
	k_sem_give(&ctx->modemlock);
}

/**
 * @brief Event callback returning the state after phy activation by nrf_modem_dect_phy_deactivate.
 *
 * @attention This function should be used in blocking mode, it releases ctx->modemlock!
 *
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param event 'NRF_MODEM_DECT_PHY_EVT_DEACTIVATE' event.
 */
static void modem_deactivate_cb(struct nrf91_context *ctx, const uint64_t *time, 
		 const struct nrf_modem_dect_phy_deactivate_event *event)
{
	if (event->err == NRF_MODEM_DECT_PHY_SUCCESS) {
		LOG_DBG("<%lld> PHY_EVT_DEACTIVATE", *time);
	} else {
		LOG_ERR("<%lld> PHY_EVT_DEACTIVATE failed,0x%x", *time, event->err);
	}
	ctx->modem.time = *time;
	k_sem_give(&ctx->modemlock);
}

/**
 * @brief Event callback notifying modems capabilities requested by nrf_modem_dect_phy_capability_get.
 *
 * * Store supported µ and beta.
 * * Prints the capabilities of the modem.
 * * Store last_status.
 * * Unblocks ctx->modemlock.
 * 
 * @attention This function should be used in blocking mode, it releases ctx->modemlock!
 *
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param event 'NRF_MODEM_DECT_PHY_EVT_CAPABILITY' event.
 */
static void modem_capability_get_cb(struct nrf91_context *ctx, const uint64_t *time, 
	 const struct nrf_modem_dect_phy_capability_get_event *event)
{
	if (event->err == NRF_MODEM_DECT_PHY_SUCCESS) {

		/* We for now pick mu/beta from one and only variant.
			TODO But if more then one is provided we have to refactore this. */
		__ASSERT_NO_MSG(event->capability->variant_count >= 1);
		ctx->modem.mu = event->capability->variant[0].mu;
		ctx->modem.beta =event->capability->variant[0].beta;

		if (IS_ENABLED(CONFIG_DECTNRP_DRIVER_NRF91_CAP_LOGGING)) {

			LOG_WRN("<%lld> PHY_EVT_CAPABILITY,dect-version=%d,variant count=%d", *time,
				event->capability->dect_version, event->capability->variant_count);

			for (uint8_t i = 0; i < event->capability->variant_count; i++) {

				const struct nrf_modem_dect_phy_capability *capability = event->capability;
				LOG_WRN(" [%u]", i);
				LOG_WRN(" ├rx spatial streams: %d",
					capability->variant[i].rx_spatial_streams);
				LOG_WRN(" ├rx tx diversity: %d",
					capability->variant[i].rx_tx_diversity);
				LOG_WRN(" ├mcs max: %d", capability->variant[i].mcs_max);
				LOG_WRN(" ├harq soft buf size: %d",
					capability->variant[i].harq_soft_buf_size);
				LOG_WRN(" ├harq process count max: %d",
					capability->variant[i].harq_process_count_max);
				LOG_WRN(" ├harq feedback delay: %d",
					capability->variant[i].harq_feedback_delay);
				LOG_WRN(" ├mu: %d", capability->variant[i].mu);
				LOG_WRN(" └beta: %d", capability->variant[i].beta);
			}
		}
	} else {
		LOG_ERR("<%lld> PHY_EVT_CAPABILITY failed,0x%x", *time, event->err);
	}

	ctx->modem.last_status = event->err;
	k_sem_give(&ctx->modemlock);
}

/**
 * @brief Event callback notifying supported bands and there parameters requested by nrf_modem_dect_phy_band_get.
 *
 * @attention This function should be used in blocking mode, it releases ctx->modemlock!
 *
 * * Prints out band information for each available band.
 * * Store last_status.
 * * Unblocks ctx->modemlock.
 *
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param event 'NRF_MODEM_DECT_PHY_EVT_BANDS' event.
 */
static void modem_band_get_cb(struct nrf91_context *ctx, const uint64_t *time, const struct nrf_modem_dect_phy_band_get_event *event)
{
	if (event->err == NRF_MODEM_DECT_PHY_SUCCESS) {
		if (IS_ENABLED(CONFIG_DECTNRP_DRIVER_NRF91_CAP_LOGGING)) {
			LOG_WRN("<%lld> %u bands supported:0x%x.", *time, event->band_count, event->supported_bands);
			for (uint32_t i = 0; i < event->band_count; i++) {
				struct nrf_modem_dect_phy_band *band = &event->band[i];
				LOG_WRN(" band[%u]", band->band_number);
				LOG_WRN(" ├band group index: %u", band->band_group_index);
				LOG_WRN(" ├rx gain: %d", band->rx_gain);
				LOG_WRN(" ├power class: %u", band->power_class);
				LOG_WRN(" └%u<=carrier<=%u", band->min_carrier, band->max_carrier);
			}
		}
	} else {
		LOG_ERR("<%lld> PHY_EVT_BANDS failed,0x%x", *time, event->err);
	}

	ctx->modem.last_status = event->err;
	k_sem_give(&ctx->modemlock);
}

/**
 * @brief Event callback notifying modem latencies requested by nrf_modem_dect_phy_latency_get.
 *
 * @attention This function should be used in blocking mode, it releases ctx->modemlock!
 *
 * * Prints out latency information.
 * * Store last_status.
 * * Unblocks ctx->modemlock.
 *
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param event 'NRF_MODEM_DECT_PHY_EVT_LATENCY' event.
 */
static void modem_latency_get_cb(struct nrf91_context *ctx, const uint64_t *time,
			   const struct nrf_modem_dect_phy_latency_info_event *event)
{
	if (event->err == NRF_MODEM_DECT_PHY_SUCCESS) {
		struct nrf_modem_dect_phy_latency_info *latency_info = event->latency_info;
		if (IS_ENABLED(CONFIG_DECTNRP_DRIVER_NRF91_CAP_LOGGING)) {
			LOG_WRN("<%lld> PHY_EVT_LATENCY", *time);
			LOG_WRN(" RX:");
			LOG_WRN(" ├idle_to_active: %u",
				latency_info->operation.receive.idle_to_active);
			LOG_WRN(" ├active_to_idle_rssi: %u",
				latency_info->operation.receive.active_to_idle_rssi);
			LOG_WRN(" ├active_to_idle_rx: %u",
				latency_info->operation.receive.active_to_idle_rx);
			LOG_WRN(" ├active_to_idle_rx_rssi: %u",
				latency_info->operation.receive.active_to_idle_rx_rssi);
			LOG_WRN(" └stop_to_rf_off: %u",
				latency_info->operation.receive.stop_to_rf_off);
			LOG_WRN(" TX:");
			LOG_WRN(" ├idle_to_active: %u",
				latency_info->operation.transmit.idle_to_active);
			LOG_WRN(" └active_to_idle: %u",
				latency_info->operation.transmit.active_to_idle);
			LOG_WRN(" Modem:");
			LOG_WRN(" ├initialization: %u", latency_info->stack.initialization);
			LOG_WRN(" ├deinitialization: %u", latency_info->stack.deinitialization);
			LOG_WRN(" ├configuration: %u", latency_info->stack.configuration);
			LOG_WRN(" ├activation: %u", latency_info->stack.activation);
			LOG_WRN(" └deactivation: %u", latency_info->stack.deactivation);
		}
	} else {
		LOG_ERR("<%lld> PHY_EVT_LATENCY failed,0x%x", *time, event->err);
	}

	ctx->modem.last_status = event->err;
	k_sem_give(&ctx->modemlock);
}

/**
 * @brief Event callback notifying modem time requested by nrf_modem_dect_phy_time_get.
 *
 * @attention This function should be used in blocking mode, it releases ctx->modemlock!
 *
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param status response state.
 */
static void modem_time_get_cb(struct nrf91_context *ctx, const uint64_t *time, 
	 const enum nrf_modem_dect_phy_err status)
{
	if (status == NRF_MODEM_DECT_PHY_SUCCESS) {
		LOG_DBG("<%lld> PHY_EVT_TIME", *time);
		ctx->modem.time = *time;
	} else {
		LOG_ERR("<%lld> PHY_EVT_TIME failed,0x%x", *time, status);
	}

	ctx->modem.last_status = status;
	k_sem_give(&ctx->modemlock);
}

/**
 * @brief Accumulate rssi1 values measured per symbol to subslots.
 *
 * * Each subslot consists of 5 symbols (µ=1).
 * * The RSSI1 value of one subslot is computed like this: subslot_dbm = MAX(symbol_dbm[0..4])
 * * Values [0..0x73] are mapped to -1.
 *   * 0 = NRF_MODEM_DECT_PHY_RSSI_NOT_MEASURED
 *   * 1..0x73 = according to Nordic - measurement is saturated
 * * If measured rssi1 values do not fit into op->rssi1.result->subslot remaining
 *   data is ignored.
 *
 * @param op
 * @param data
 * @param len
 */
static void rssi1_accumulate(struct dectnrp_driver_op *op, int8_t *data, uint16_t len)
{
	len = MIN(sizeof(op->rssi1.result->subslot) * DECTNRP_SYMBOLS_PER_SUBSLOT, len);
	int16_t max = DECTNRP_RSSI_MIN;

	for (size_t i = 0; i < len; i++) {

		int16_t rssi1 = (int16_t)(0xff00 | data[i]);
		if (rssi1 < DECTNRP_RSSI_MIN) {
			/* If the measurement is saturated, the measured signal strength
			   is reported as a positive integer.
			   If a symbol is not measured, its value is reported
			   as @ref NRF_MODEM_DECT_PHY_RSSI_NOT_MEASURED.
			    -> Limit to DECTNRP_RSSI_MAX */
			rssi1 = DECTNRP_RSSI_MAX;
		}
		max = MAX(max, rssi1);

		if ((i + 1) % DECTNRP_SYMBOLS_PER_SUBSLOT == 0) {
			/* Write maximum rssi1 value of that sub-slot formated according to spec */
			op->rssi1.result->subslot[i / DECTNRP_SYMBOLS_PER_SUBSLOT] = (uint8_t)max;
			max = DECTNRP_RSSI_MIN;
		}
	}
}

/**
 * @brief Event callback notifying rssi1 measurement results requested by nrf_modem_dect_phy_rssi.
 * 
 * @attention This function is used in asynchronous mode!
 * 
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param op_complete_event 'NRF_MODEM_DECT_PHY_EVT_RSSI' event.
 */
static void modem_rssi_result(struct nrf91_context *ctx, const uint64_t *time, 
	 const struct nrf_modem_dect_phy_rssi_event *event)
{
	ARG_UNUSED(ctx);
	struct dectnrp_driver_op *op = (void *)event->handle;
	__ASSERT(op->rssi1.result->subslot != NULL, "Data struct for rssi1 measurements missing!");

	int64_t diff = event->meas_start_time - op->start_time;

	LOG_DBG("<%lld>[0x%x] PHY_EVT_RSSI,rssi1-start-time=%llu,diff<%llu>,"
		"(~%llu frames),mea-start-time=%llu",
		*time, event->handle, op->start_time, diff, diff / FRAME_MODEM_TIME_TICK,
		event->meas_start_time);

	rssi1_accumulate(op, event->meas, event->meas_len);

	return;
}

/**
 * @brief Event callback notifying a received PCC.
 *
 * @attention This function is used in asynchronous mode!
 * 
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param pcc_event 'NRF_MODEM_DECT_PHY_EVT_PCC' event.
 */
static void modem_pcc_received(struct nrf91_context *ctx, const uint64_t *time, 
	 const struct nrf_modem_dect_phy_pcc_event *event)
{
	int16_t rssi_2 = event->rssi_2;

	LOG_DBG("<%lld>[0x%x] PHY_EVT_PCC,stf_start_time:%llu,phy_type:%d,"
		"rssi_2:%d.%udBm,snr:0x%x",
		*time, event->handle, event->stf_start_time, event->phy_type, rssi_2 / 2,
		(rssi_2 & 0b1) * 5, event->snr);

	if (atomic_test_bit(&ctx->rx.state, RADIO_RXSTATE_PCC)) {
		LOG_WRN("PHY_EVT_PCC overrun,pcc dropped");
		// TODO Event is completly lost, signalize to upper layer.
	} else if (event->header_status == NRF_MODEM_DECT_PHY_HDR_STATUS_INVALID) {
		LOG_WRN("PHY_EVT_PCC,HDR_STATUS_INVALID,dropped");
		// TODO Event is completly lost, signalize to upper layer.
	} else if (event->header_status == NRF_MODEM_DECT_PHY_HDR_STATUS_INVALID) {
		LOG_WRN("PHY_EVT_PCC,HDR_STATUS_VALID_RX_END,dropped");
		// TODO Event is completly lost, signalize to upper layer.
	} else {
		// TODO This gets stored at ctx, maybe it would be better to store it at operation.
		memcpy(&ctx->rx.pcc_event, event, sizeof(*event));
		atomic_set_bit(&ctx->rx.state, RADIO_RXSTATE_PCC);
	}	
}

/**
 * @brief Event callback notifying a crc error while receiving PCC.
 *
 * @attention This function is used in asynchronous mode!
 * 
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param crc_failure 'NRF_MODEM_DECT_PHY_EVT_PCC_ERROR' event.
 */
static void modem_pcc_crc_err(struct nrf91_context *ctx, const uint64_t *time,
			   const struct nrf_modem_dect_phy_pcc_crc_failure_event *event)
{
	ctx->rx.crc_errors_phy_header++;
	int16_t rssi2 = event->rssi_2;

	LOG_WRN("PHY_EVT_PCC_ERROR,stf_start_time:%llu,rssi_2:%d.%udBm,"
		"snr:0x%x,crc_errors_phy_header:%u",
		event->stf_start_time, rssi2 / 2, (rssi2 & 0b1) * 5, event->snr,
		ctx->rx.crc_errors_phy_header);

	if (ctx->upper_layer_event_handler) {

		struct dectnrp_driver_op *op = (void *)event->handle;

		struct dectnrp_driver_event stack_event = {
			.code = DECTNRP_EVENT_MSG_ERROR,
			.message_error.status = DECTNRP_ERROR_PCC_CRC,
			.message_error.op = op,
			.message_error.start_time = event->stf_start_time,
			.message_error.rssi2_valid = (rssi2 != NRF_MODEM_DECT_PHY_RSSI2_NOT_MEASURED),
			.message_error.snr_valid = (event->snr != NRF_MODEM_DECT_PHY_SNR_NOT_MEASURED)
		};

		if (stack_event.message_error.rssi2_valid) {
			/* Transcode rssi2 value */
			stack_event.message_error.rssi2 = dectnrp_nrf91_encode_rssi2(rssi2);
		}
		if (stack_event.message_error.snr_valid) {
			/* Transcode snr value */
			stack_event.message_error.snr = dectnrp_nrf91_encode_snr(event->snr);
		}

		ctx->upper_layer_event_handler(nrf91_dev, &stack_event);
	}
}

/**
 * @brief Event callback notifying a received PDC.
 *
 * @attention This function is used in asynchronous mode!
 * 
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param pdc_event 'NRF_MODEM_DECT_PHY_EVT_PDC' event.
 */
static void modem_pdc_received(struct nrf91_context *ctx, const uint64_t *time, 
	 const struct nrf_modem_dect_phy_pdc_event *event)
{
	struct dectnrp_driver_op *op = (void *)event->handle;

	int16_t rssi2 = event->rssi_2;
	LOG_DBG("<%lld>[0x%x] PHY_EVT_PDC,len:%u,rssi_2:%d.%udBm,snr:0x%x", *time, event->handle,
		event->len, rssi2 / 2, (rssi2 & 0b1) * 5, event->snr);

	if (!atomic_test_bit(&ctx->rx.state, RADIO_RXSTATE_PCC)) {
		LOG_WRN("PHY_EVT_PDC,!RADIO_RXSTATE_PCC,pdc dropped");
		// TODO Event is completly lost, signalize to upper layer.
	} else if(ctx->rx.pcc_event.transaction_id != event->transaction_id) {
		LOG_WRN("PHY_EVT_PDC,transaction_id mismatch,pdc dropped");
		// TODO Event is completly lost, signalize to upper layer.
	} else {
		atomic_set_bit(&ctx->rx.state, RADIO_RXSTATE_PDC);

		if (ctx->upper_layer_event_handler) {
			struct dectnrp_driver_event stack_event = {
				.code = DECTNRP_EVENT_MSG_RECEIVED,
				.msg_received.op = op,
				.msg_received.phy_type = ctx->rx.pcc_event.phy_type,
				.msg_received.pcc = &ctx->rx.pcc_event.hdr.type_1[0],
				.msg_received.pdc = event->data,
				.msg_received.pdc_len = event->len,
				.msg_received.start_time = ctx->rx.pcc_event.stf_start_time,
				.msg_received.rssi2_valid = (rssi2 != NRF_MODEM_DECT_PHY_RSSI2_NOT_MEASURED),
				.msg_received.snr_valid = (event->snr != NRF_MODEM_DECT_PHY_SNR_NOT_MEASURED)
			};

			if (stack_event.msg_received.rssi2_valid) {
				/* Transcode rssi2 value */
				stack_event.msg_received.rssi2 = dectnrp_nrf91_encode_rssi2(rssi2);
			}
			if (stack_event.msg_received.snr_valid) {
				/* Transcode snr value */
				stack_event.msg_received.snr = dectnrp_nrf91_encode_snr(event->snr);
			}

			ctx->upper_layer_event_handler(nrf91_dev, &stack_event);
		}
	}
}

/**
 * @brief Event callback notifying a crc error while receiving PDC.
 *
 * @attention This function is used in asynchronous mode!
 * * Creates a packet with received PCC only
 * * notifies that packet up to upper layers
 *
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param crc_failure 'NRF_MODEM_DECT_PHY_EVT_PDC_ERROR' event.
 */
static void modem_pdc_crc_err(struct nrf91_context *ctx, const uint64_t *time,
			   const struct nrf_modem_dect_phy_pdc_crc_failure_event *event)
{
	struct dectnrp_driver_op *op = (void *)event->handle;

	int16_t rssi2 = event->rssi_2;
	ctx->rx.crc_errors_data++;

	LOG_WRN("PHY_EVT_PDC_ERROR,"
		"rssi_2:%d.%udBm,snr:0x%x,crc_errors_data:%u",
		rssi2 / 2, (rssi2 & 0b1) * 5, event->snr, ctx->rx.crc_errors_data);

	if (!atomic_test_bit(&ctx->rx.state, RADIO_RXSTATE_PCC)) {
		LOG_WRN("PHY_EVT_PDC_ERROR,!RADIO_RXSTATE_PCC,pdc dropped");
		// TODO: FIXME: signalize to higher Layer?
	} else {
		atomic_set_bit(&ctx->rx.state, RADIO_RXSTATE_PDC);

		if (ctx->upper_layer_event_handler) {
			/* PCC Header is good to go to upper layers */
			struct dectnrp_driver_event stack_event = {
				.code = DECTNRP_EVENT_MSG_ERROR,
				.message_error.status = DECTNRP_ERROR_PDC_CRC,
				.message_error.op = op,
				.message_error.pcc_valid = true,
				.message_error.phy_type = ctx->rx.pcc_event.phy_type,
				.message_error.pcc = &ctx->rx.pcc_event.hdr.type_1[0],
				.message_error.start_time = ctx->rx.pcc_event.stf_start_time,
				.message_error.rssi2_valid = (rssi2 != NRF_MODEM_DECT_PHY_RSSI2_NOT_MEASURED),
				.message_error.snr_valid = (event->snr != NRF_MODEM_DECT_PHY_SNR_NOT_MEASURED)
			};

			if (stack_event.message_error.rssi2_valid) {
				/* Transcode rssi2 value */
				stack_event.message_error.rssi2 = dectnrp_nrf91_encode_rssi2(rssi2);
			}
			if (stack_event.message_error.snr_valid) {
				/* Transcode snr value */
				stack_event.message_error.snr = dectnrp_nrf91_encode_snr(event->snr);
			}

			ctx->upper_layer_event_handler(nrf91_dev, &stack_event);
		}
	}
}

/**
 * @brief Event callback notifying completed operations.
 * 
 * @attention This function is used in asynchronous mode!
 * 
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param event 'NRF_MODEM_DECT_PHY_EVT_COMPLETED' event.
 */
static void modem_op_complete_cb(struct nrf91_context *ctx, const uint64_t *time,
			   const struct nrf_modem_dect_phy_op_complete_event *event)
{
	if (event->err == NRF_MODEM_DECT_PHY_SUCCESS) {
		LOG_DBG("<%lld>[0x%x] PHY_EVT_COMPLETED,%d°C,%umV", *time, event->handle,
			event->temp, event->voltage);
	} else {
		LOG_INF("<%lld>[0x%x] PHY_EVT_COMPLETED failed,0x%x,%d°C,%umV", *time,
			event->handle, event->err, event->temp, event->voltage);
	}

	struct dectnrp_driver_op *op = (void *)event->handle;

	bool rx = atomic_test_bit(&ctx->rx.state, RADIO_RXSTATE_PDC);
	if (rx) {
		atomic_clear_bit(&ctx->rx.state, RADIO_RXSTATE_PCC);
		atomic_clear_bit(&ctx->rx.state, RADIO_RXSTATE_PDC);
	}

	if (ctx->upper_layer_event_handler) {
		struct dectnrp_driver_event stack_event = {
			.code = DECTNRP_EVENT_OP_FINISHED,
			.op_finished.op = op
		};
		op->status = -event->err; // FIXME use generic error codes
		ctx->upper_layer_event_handler(nrf91_dev, &stack_event);
	}
}

/**
 * @brief Event callback notifying status of cancel operation requested by nrf_modem_dect_phy_cancel.
 *
 * @attention This function is used in asynchronous mode!
 * @attention This function does only logging up to now!
 * 
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param cancel 'NRF_MODEM_DECT_PHY_EVT_CANCELED' event.
 */
static void modem_op_cancel_cb(struct nrf91_context *ctx, const uint64_t *time, 
	 const struct nrf_modem_dect_phy_cancel_event *event)
{
	switch (event->err) {
	case NRF_MODEM_DECT_PHY_SUCCESS: {
		LOG_DBG("<%lld>[0x%x] PHY_EVT_CANCELED", *time, event->handle);
		if(event->handle != NRF_MODEM_DECT_PHY_HANDLE_CANCEL_ALL) {
			LOG_ERR("PHY_EVT_CANCELED of single operation not supported yet");
			// TODO 
			//struct dectnrp_driver_op *op = (void *)event->handle;
		} 
		break;
	}
	case NRF_MODEM_DECT_PHY_ERR_UNSUPPORTED_OP: {
		LOG_ERR("PHY_EVT_CANCELED failed, unsupported operation");
		break;
	}
	case NRF_MODEM_DECT_PHY_ERR_NOT_FOUND: {
		LOG_ERR("PHY_EVT_CANCELED failed, handle not found");
		break;
	}
	case NRF_MODEM_DECT_PHY_ERR_NOT_ALLOWED: {
		LOG_ERR("PHY_EVT_CANCELED failed, not allowed");
	}
	default: {
		break;
	}
	}

	ctx->modem.last_status = event->err;
	k_sem_give(&ctx->modemlock);
}

/**
 * @brief Event callback returning the state after configuration of links by nrf_modem_dect_phy_link_config.
 *
 * @attention This function is used in asynchronous mode!
 * @attention This function does only logging up to now!
 * 
 * @param ctx The context related to nrf91 radio device.
 * @param time Modem time of the event.
 * @param cancel 'NRF_MODEM_DECT_PHY_EVT_CANCELED' event.
 */
static void modem_link_config_cb(struct nrf91_context *ctx, const uint64_t *time, 
	 const struct nrf_modem_dect_phy_link_config_event *event)
{
	ARG_UNUSED(ctx);
	if (event->err == NRF_MODEM_DECT_PHY_SUCCESS) {
		LOG_WRN("<%lld> PHY_EVT_LINK_CONFIG", *time);
	} else {
		LOG_ERR("<%lld> PHY_EVT_LINK_CONFIG failed,0x%x", *time,
			event->err);
	}
}

/**
 * @brief Handler for events coming from the modem related to phy.
 *
 * Distributes the events to dedicated handling function using the @p event->id.
 * @attention This function for some events is used in blocking mode, it releases ctx->modemlock!
 *
 * @param event Pointer to the event containing type and event specific information.
 */
static void modem_phy_event_handler(const struct nrf_modem_dect_phy_event *event)
{
	struct nrf91_context *ctx = nrf91_dev->data;

	LOG_DBG("event id %d", event->id);

	switch (event->id) {
	case NRF_MODEM_DECT_PHY_EVT_INIT: {
		modem_init_cb(ctx, &event->time, &event->init);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_DEINIT: {
		modem_deinit_cb(ctx, &event->time, &event->deinit);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_CONFIGURE: {
		modem_configure_cb(ctx, &event->time, &event->configure);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_RADIO_CONFIG: {
		modem_radio_config_cb(ctx, &event->time, &event->radio_config);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_ACTIVATE: { 
		modem_activate_cb(ctx, &event->time, &event->activate);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_DEACTIVATE: {
		modem_deactivate_cb(ctx, &event->time, &event->deactivate);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_CAPABILITY: {
		modem_capability_get_cb(ctx, &event->time, &event->capability_get);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_BANDS: {
		modem_band_get_cb(ctx, &event->time, &event->band_get);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_LATENCY: {
		modem_latency_get_cb(ctx, &event->time, &event->latency_get);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_TIME: {
		modem_time_get_cb(ctx, &event->time, event->time_get.err);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_COMPLETED: {
		modem_op_complete_cb(ctx, &event->time, &event->op_complete);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_CANCELED: {
		modem_op_cancel_cb(ctx, &event->time, &event->cancel);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_RSSI: {
		modem_rssi_result(ctx, &event->time, &event->rssi);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_PCC: {
		modem_pcc_received(ctx, &event->time, &event->pcc);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_PCC_ERROR: {
		modem_pcc_crc_err(ctx, &event->time, &event->pcc_crc_err);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_PDC: {
		modem_pdc_received(ctx, &event->time, &event->pdc);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_PDC_ERROR: {
		modem_pdc_crc_err(ctx, &event->time, &event->pdc_crc_err);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_LINK_CONFIG: {
		modem_link_config_cb(ctx, &event->time, &event->link_config);
		break;
	}
	case NRF_MODEM_DECT_PHY_EVT_STF_CONFIG: {
		if (event->stf_cover_seq_control.err == NRF_MODEM_DECT_PHY_SUCCESS) {
			LOG_WRN("<%lld> PHY_EVT_STF_CONFIG", event->time);
		} else {
			LOG_ERR("<%lld> PHY_EVT_STF_CONFIG failed, 0x%x", event->time,
				event->stf_cover_seq_control.err);
		}
		break;
	}
	default: {
		LOG_ERR("<%lld> Unknown modem event, 0x%x", event->time, event->id);
		break;
	}
	}
}

/**
 * @brief Reads and verifies firmware version from modem.
 *
 * @param[out] mfw_version
 * @return int
 */
static int verify_modem_fw_version(enum nrf91_mfw_version *mfw_version)
{

	*mfw_version = RADIO_MFW_UNKNOWN;
	char fw_version_buf[32];

	int ret = nrf_modem_at_cmd(fw_version_buf, sizeof(fw_version_buf), "AT+CGMR");
	if (ret != 0) {
		LOG_ERR("Unable to obtain modem FW version (ERR: %d, ERR TYPE: %d)",
			nrf_modem_at_err(ret), nrf_modem_at_err_type(ret));
		ret = -EIO;
	} else {

		/* Replace "\r\n" against "\0" for printing zero terminated FW version string. */
		char *fw_version_end = strstr(fw_version_buf, "\r\n");
		if (!fw_version_end) {
			LOG_ERR("MFW version:invalid version string");
			ret = -EIO;
		} else {
			*fw_version_end = '\0';
			LOG_WRN("MFW version:%s", fw_version_buf);

			char *fw_version_1_1_0 = strstr(fw_version_buf, "mfw-nr+_nrf91_1.1.0");
			if (fw_version_1_1_0) {
				*mfw_version = RADIO_MFW_PHY_1_1_0;
			} else {
				char *fw_version_2_0_0 =
					strstr(fw_version_buf, "mfw-nr+-phy_nrf91x1_2.0.0");
				if (fw_version_2_0_0) {
					*mfw_version = RADIO_MFW_PHY_2_0_0;
				} else {
					LOG_ERR("MFW version:unsupported");
					ret = -EIO;
				}
			}
		}
	}
	return ret;
}

static void log_fw_uuid(void)
{
	char fw_uuid_buf[64];

	int ret = nrf_modem_at_cmd(fw_uuid_buf, sizeof(fw_uuid_buf), "AT%%XMODEMUUID");
	if (ret == 0) {
		/* Get string that starts with " " after "%XMODEMUUID:",
		 * then move to the next string before "\r\n"
		 * which corresponds to the FW UUID string.
		 */
		char *fw_uuid = strstr(fw_uuid_buf, " ");
		fw_uuid++;
		char *fw_uuid_end = strstr(fw_uuid_buf, "\r\n");
		size_t off = fw_uuid_end - fw_uuid_buf - 1;
		fw_uuid_buf[off + 1] = '\0';
		LOG_WRN("MFW uuid:%s", fw_uuid);
	} else {
		LOG_ERR("MFW uuid:Unable to obtain UUID,err:%d,type:%d", nrf_modem_at_err(ret),
			nrf_modem_at_err_type(ret));
	}
}

static int nrf91_start(const struct device *dev)
{
	int ret = 0;
	struct nrf91_context *ctx = dev->data;
	LOG_INF("nrf91_start(%s)", dev->name);

	if (!atomic_test_bit(&ctx->state, RADIO_STATE_INITIALIZED)) {
		LOG_ERR("nrf91_start but !RADIO_STATE_INITIALIZED");
		ret = -EIO;
		goto error;
	} else if (atomic_test_bit(&ctx->state, RADIO_STATE_STARTED)) {
		ret = -EALREADY;
		goto error;
	} else {
		ret = nrf_modem_dect_phy_activate(NRF_MODEM_DECT_PHY_RADIO_MODE_LOW_LATENCY);
		if (ret < 0) {
			NET_ERR("nrf_modem_dect_phy_activate failed,%d", ret);
			ret = -EIO;
			goto error;
		}
		ret = k_sem_take(&ctx->modemlock, K_MSEC(100));
		if (ret < 0) {
			NET_ERR("nrf_modem_dect_phy_activate(), k_sem_take failed");
			ret = -EIO;
			goto error;
		}

		ctx->tx.count = 0;
		ctx->rx.count = 0;

		/*  Operations are allowed now. */
		atomic_set_bit(&ctx->state, RADIO_STATE_STARTED);
		k_sem_give(&ctx->operation); 
		NET_INFO("DECT radio nrf91 started");
	}

	goto out;
error:
	NET_INFO("dectnrp_nrf91 NOT initialized");
out:
	return ret;
}

static int nrf91_stop(const struct device *dev)
{
	struct nrf91_context *ctx = dev->data;
	LOG_INF("nrf91_stop(%s)", dev->name);

	if (!atomic_test_bit(&ctx->state, RADIO_STATE_STARTED)) {
		return -EALREADY;
	}

	int ret = nrf_modem_dect_phy_cancel(NRF_MODEM_DECT_PHY_HANDLE_CANCEL_ALL);
	if (ret < 0) {
		LOG_ERR("Failed to send 'PHY_HANDLE_CANCEL_ALL' to modem, %d", ret);
		ret = -EIO;
		goto error;
	}
	ret = k_sem_take(&ctx->modemlock, K_MSEC(100));
	if (ret < 0) {
		NET_ERR("nrf_modem_dect_phy_cancel(), k_sem_take failed");
		ret = -EIO;
		goto error;
	}

	ret = nrf_modem_dect_phy_deinit();
	if (ret < 0) {
		LOG_ERR("Failed to deinit modem, %d", ret);
		return -EIO;
	}
	ret = k_sem_take(&ctx->modemlock, K_MSEC(100));
	if (ret < 0) {
		NET_ERR("nrf_modem_dect_phy_deinit(), k_sem_take failed");
		ret = -EIO;
		goto error;
	}

	atomic_clear_bit(&ctx->state, RADIO_STATE_STARTED);
	NET_INFO("DECT radio nrf91 stopped");
	goto out;
error:
	NET_INFO("dectnrp_nrf91 NOT stopped");
out:
	return ret;
}

static int nrf91_get_capabilities(const struct device *dev, struct dectnrp_device_capabilities *caps)
{
	LOG_INF("nrf91_get_capabilities(%s)", dev->name);
	caps->hw_caps = (DECTNRP_HW_FILTER | DECTNRP_HW_TXTIME | DECTNRP_HW_RXTIME);
	return 0;
}

static int nrf91_configure(const struct device *dev, enum dectnrp_config_type type,
			   const struct dectnrp_config *config)
{
	struct nrf91_context *ctx = dev->data;
	LOG_INF("nrf91_configure(%s)", dev->name);

	int ret = 0;

	switch (type) {
	case DECTNRP_CONFIG_TYPE_EVENT_HANDLER: {
		ctx->upper_layer_event_handler = config->event_handler;
		break;
	}
	case DECTNRP_CONFIG_TYPE_NETWORK_ID: {
		ctx->network_id = config->network_id;
		break;
	}
	case DECTNRP_CONFIG_TYPE_FILTER_SHORT_NETWORK_ID: {
		ctx->rx.filter.short_network_id_active = config->filter_short_network_id.activate;
		ctx->rx.filter.short_network_id = config->filter_short_network_id.value;
		break;
	}
	case DECTNRP_CONFIG_TYPE_FILTER_SHORT_RECEIVER_ADDR: {
		ctx->rx.filter.short_addr_active = config->filter_short_receiver_addr.activate;
		ctx->rx.filter.short_addr = config->filter_short_receiver_addr.value;
		break;
	}
	default:
		ret = -ENOTSUP;
		break;
	}
	return ret;
}

static int nrf91_get_time(const struct device *dev, net_time_t *time)
{
	struct nrf91_context *ctx = dev->data;
	LOG_DBG("nrf91_get_time(%s)", dev->name);

	int ret = 0;

	if (!atomic_test_bit(&ctx->state, RADIO_STATE_INITIALIZED)) {
		LOG_ERR("nrf91_get_time but !RADIO_STATE_INITIALIZED");
		ret = -EIO;
		goto error;
	}

	ret = k_sem_take(&ctx->operation, K_MSEC(10));
	if (ret < 0) {
		NET_ERR("nrf91_get_time() k_sem_take(operation) failed");
		ret = -EIO;
		goto error;
	}

	ret = nrf_modem_dect_phy_time_get();
	if (ret < 0) {
		NET_ERR("nrf_modem_dect_phy_time_get failed, %d", ret);
		ret = -EIO;
		goto out;
	}

	/* Block until time_get_cb has been called and semaphore was given back */
	ret = k_sem_take(&ctx->modemlock, K_MSEC(10));
	if (ret < 0) {
		NET_ERR("nrf91_get_time() k_sem_take(modem) failed");
		ret = -EIO;
		goto out;
	}

	*time = ctx->modem.time;
	LOG_INF("<%" PRIu64 "> nrf91_get_time()", ctx->modem.time);

out:
	k_sem_give(&ctx->operation);
error:
	return ret;
}

#if CONFIG_DECTNRP_DRIVER_SCHEDULED_API

static int nrf91_schedule(const struct device *dev, struct dectnrp_driver_op *op)
{
	struct nrf91_context *ctx = dev->data;
	LOG_DBG("nrf91_schedule(%s)", dev->name);

	int ret = 0;

	if (!atomic_test_bit(&ctx->state, RADIO_STATE_STARTED)) {
		LOG_ERR("nrf91-device not initialized yet, drop rx operation");
		ret = -EIO;
		goto out;
	}

	switch (op->type) {
	case DECTNRP_DRIVER_OP_TX: {

		uint8_t ctrl_size_type = op->tx.phy_header_type;
		uint8_t ctrl_size_bytes = ctrl_size_type == 0 ? DECTNRP_PHY_HEADER_TYPE1_SIZE
							      : DECTNRP_PHY_HEADER_TYPE2_SIZE;
		const struct dectnrp_transport_parameters *parameters =
			&op->tx.transport_parameters;

		if (parameters->mu != ctx->modem.mu) {
			NET_ERR("radio does not support mu=%d, expect mu=%d, drop data",
				parameters->mu, ctx->modem.mu);
			ret = -EINVAL;
			goto out;
		}
		if (parameters->beta != ctx->modem.beta) {
			NET_ERR("radio does not support beta=%d, expect beta=%d, drop data",
				parameters->beta, ctx->modem.beta);
			ret = -EINVAL;
			goto out;
		}

		struct net_buf *frag = op->tx.pkt->buffer;

		struct nrf_modem_dect_phy_tx_params tx_param = {
			.handle = (uintptr_t)op,
			.network_id = ctx->network_id,
			.phy_type = op->tx.phy_header_type,
			.carrier = op->channel,
			.phy_header = (union nrf_modem_dect_phy_hdr *)&frag->data[0],
			.data = &frag->data[ctrl_size_bytes],
			.data_size = frag->len - ctrl_size_bytes,
		};
		if (op->tx.mode.is_scheduled) {
			tx_param.start_time = op->start_time;
		} else {
			/* Immediate operation. */
			tx_param.start_time = 0;
		}

		if (op->tx.mode.is_lbt) {

			if (op->tx.lbt.period < NRF_MODEM_DECT_LBT_PERIOD_MIN) {
				NET_ERR("[%p]nrf91_schedule lbt.period %d to small, min=%d", op,
					op->tx.lbt.period, NRF_MODEM_DECT_LBT_PERIOD_MIN);
				ret = -EINVAL;
				goto out;
			} else if (op->tx.lbt.period > NRF_MODEM_DECT_LBT_PERIOD_MAX) {
				NET_ERR("[%p]nrf91_schedule lbt.period %d to big, min=%d", op,
					op->tx.lbt.period, NRF_MODEM_DECT_LBT_PERIOD_MAX);
				ret = -EINVAL;
				goto out;
			}
			tx_param.lbt_period = op->tx.lbt.period;
			tx_param.lbt_rssi_threshold_max = op->tx.lbt.rssi_threshold;
		} else {
			/* Without LBT */
			tx_param.lbt_period = 0;
			tx_param.lbt_rssi_threshold_max = 0;
		}

		LOG_DBG("[%p]<%lld> PHY TX", op, tx_param.start_time);
		LOG_HEXDUMP_DBG(&frag->data[0], frag->len, "PHY TX:");

		ret = k_sem_take(&ctx->operation, K_MSEC(10));
		if (ret < 0) {
			NET_ERR("[%p]nrf91_schedule k_sem_take failed,%d", op, ret);
			ret = -EIO;
		} else {
			ret = nrf_modem_dect_phy_tx(&tx_param);
			if (ret < 0) {
				NET_ERR("[%p]nrf_modem_dect_phy_tx failed,%d", op, ret);
				ret = -EIO;
			}
			k_sem_give(&ctx->operation);
		}

		break;
	}
	case DECTNRP_DRIVER_OP_RX: {

		struct nrf_modem_dect_phy_rx_params rx_params = {
			.handle = (uintptr_t)op,
			.network_id = ctx->network_id,
			.mode = NRF_MODEM_DECT_PHY_RX_MODE_SINGLE_SHOT,
			.link_id = NRF_MODEM_DECT_PHY_LINK_UNSPECIFIED,
			.rssi_level = 0, /* fast AGC algorithm */
			.carrier = op->channel,
			.duration = op->rx.duration};

		if (op->rx.mode.is_scheduled) {
			rx_params.start_time = op->start_time;
		} else {
			/** Immediate operation. */
			rx_params.start_time = 0; 
		}

		rx_params.filter.is_short_network_id_used = ctx->rx.filter.short_network_id_active;
		if (ctx->rx.filter.short_network_id_active) {
			rx_params.filter.short_network_id = ctx->rx.filter.short_network_id;
		}
		if (ctx->rx.filter.short_addr_active) {
			rx_params.filter.receiver_identity = ctx->rx.filter.short_addr;
		} else {
			// listen for everything (broadcast mode used)
			rx_params.filter.receiver_identity = 0;
		}
		LOG_DBG("[%p]<%lld> PHY RX", op, rx_params.start_time);

		ret = k_sem_take(&ctx->operation, K_MSEC(10));
		if (ret < 0) {
			NET_ERR("[%p]nrf91_schedule k_sem_take failed,%d", op, ret);
			ret = -EIO;
		} else {
			ret = nrf_modem_dect_phy_rx(&rx_params);
			if (ret < 0) {
				NET_ERR("[%p]nrf_modem_dect_phy_rx failed,%d", op, ret);
				ret = -EIO;
			}
			k_sem_give(&ctx->operation);
		}

		break;
	}
	case DECTNRP_DRIVER_OP_RSSI1: {

		struct nrf_modem_dect_phy_rssi_params rssi1_params = {
			.handle = (uintptr_t)op,
			.carrier = op->channel,
			.duration = op->rssi1.subslots,
			.reporting_interval = NRF_MODEM_DECT_PHY_RSSI_INTERVAL_24_SLOTS};

		if (op->rssi1.mode.is_scheduled) {
			rssi1_params.start_time = op->start_time;
		} else {
			/** Immediate operation. */
			rssi1_params.start_time = 0;
		}

		ret = k_sem_take(&ctx->operation, K_MSEC(100));
		if (ret < 0) {
			NET_ERR("[%p]nrf91_schedule k_sem_take failed,%d", op, ret);
			ret = -EIO;
		} else {
			ret = nrf_modem_dect_phy_rssi(&rssi1_params);
			if (ret < 0) {
				NET_ERR("[%p]nrf_modem_dect_phy_rssi failed,%d", op, ret);
				ret = -EIO;
			}
			k_sem_give(&ctx->operation);
		}

		break;
	}
	}

out:
	return ret;
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

static int nrf91_init(const struct device *dev)
{
	int ret = 0;
	struct nrf91_context *ctx = dev->data;
	LOG_DBG("nrf91_init(%s)", dev->name);

	if (nrf91_dev != NULL) {
		/* This driver is only a singleton initialized once for now. */
		LOG_ERR("nrf91_init(%s) failed, init twice not supported for now", dev->name);
		ret = -EEXIST;
	} else {

		nrf91_dev = dev;
		ctx->upper_layer_event_handler = NULL;
		ctx->state = ATOMIC_INIT(RADIO_STATE_UNKNOWN);
		ctx->rx.state = ATOMIC_INIT(RADIO_RXSTATE_UNKNOWN);
		k_sem_init(&ctx->modemlock, 0, 1);
		k_sem_init(&ctx->operation, 0, 1);

		ret = nrf_modem_lib_init();
		if (ret < 0) {
			NET_ERR("nrf_modem_lib_init failed,0x%x", ret);
			ret = -EIO;
			goto error;
		}

		ret = verify_modem_fw_version(&ctx->mfw_version);
		if (ret < 0) {
			goto error;
		}
		log_fw_uuid();
		char *modem_lib_ver = nrf_modem_build_version();
		LOG_WRN("MFW lib-version:%s", modem_lib_ver);

		ret = nrf_modem_dect_phy_event_handler_set(modem_phy_event_handler);
		if (ret < 0) {
			NET_ERR("nrf_modem_dect_phy_event_handler_set failed,0x%x", ret);
			ret = -EIO;
			goto error;
		}

		ret = nrf_modem_dect_phy_init();
		if (ret < 0) {
			NET_ERR("nrf_modem_dect_phy_init failed,0x%x", ret);
			ret = -EIO;
			goto error;
		}
		ret = k_sem_take(&ctx->modemlock, K_MSEC(100));
		if (ret < 0) {
			NET_ERR("nrf_modem_dect_phy_init, k_sem_take failed");
			ret = -EIO;
			goto error;
		} else if (ctx->modem.last_status != NRF_MODEM_DECT_PHY_SUCCESS) {
			NET_ERR("nrf_modem_dect_phy_init failed,0x%x", ctx->modem.last_status );
			ret = -EIO;
			goto error;
		}

		static const struct nrf_modem_dect_phy_config_params init_params = {
			.band_group_index = CONFIG_DECTNRP_DRIVER_NRF91_BAND_GROUP,
			.harq_rx_process_count = 1,        /* TODO make it more flexible */
			.harq_rx_expiry_time_us = 5000000, /* TODO make it more flexible */
		};
		ret = nrf_modem_dect_phy_configure(&init_params);
		if (ret < 0) {
			NET_ERR("nrf_modem_dect_phy_init failed,0x%x", ret);
			ret = -EIO;
			goto error;
		}
		ret = k_sem_take(&ctx->modemlock, K_MSEC(500));
		if (ret < 0) {
			NET_ERR("nrf_modem_dect_phy_configure(), k_sem_take failed");
			ret = -EIO;
			goto error;
		} else if (ctx->modem.last_status != NRF_MODEM_DECT_PHY_SUCCESS) {
			NET_ERR("nrf_modem_dect_phy_configure failed,0x%x", ctx->modem.last_status);
			ret = -EIO;
			goto error;
		}

		ret = nrf_modem_dect_phy_capability_get();
		if (ret < 0) {
			NET_ERR("nrf_modem_dect_phy_capability_get failed,0x%x", ret);
			ret = -EIO;
			goto error;
		}
		ret = k_sem_take(&ctx->modemlock, K_MSEC(100));
		if (ret < 0) {
			NET_ERR("nrf_modem_dect_phy_capability_get, k_sem_take failed");
			ret = -EIO;
			goto error;
		} else if (ctx->modem.last_status != NRF_MODEM_DECT_PHY_SUCCESS) {
			NET_ERR("nrf_modem_dect_phy_capability_get failed,0x%x", ctx->modem.last_status);
			ret = -EIO;
			goto error;
		}

		ret = nrf_modem_dect_phy_band_get();
		if (ret < 0) {
			NET_ERR("nrf_modem_dect_phy_band_get failed,0x%x", ret);
			ret = -EIO;
			goto error;
		}
		ret = k_sem_take(&ctx->modemlock, K_MSEC(100));
		if (ret < 0) {
			NET_ERR("nrf_modem_dect_phy_band_get, k_sem_take failed");
			ret = -EIO;
			goto error;
		}

		ret = nrf_modem_dect_phy_latency_get();
		if (ret < 0) {
			NET_ERR("nrf_modem_dect_phy_latency_get failed,0x%x", ret);
			ret = -EIO;
			goto error;
		}
		ret = k_sem_take(&ctx->modemlock, K_MSEC(100));
		if (ret < 0) {
			NET_ERR("nrf_modem_dect_phy_latency_get, k_sem_take failed");
			ret = -EIO;
			goto error;
		}
	}

	LOG_INF("nrf91 dectnrp radio initialized");
	atomic_set_bit(&ctx->state, RADIO_STATE_INITIALIZED);
	goto out;
error:
	NET_INFO("dectnrp_nrf91 NOT initialized");
out:
	return ret;
}

static void nrf91_net_if_init(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	struct nrf91_context *ctx = dev->data;
	LOG_DBG("nrf91_net_if_init");

	if (ctx->iface != NULL) {
		/* This driver is only a singleton initialized once for now. */
		LOG_ERR("nrf91_net_if_init(%s) failed, init twice not supported for now", dev->name);
	} else {
		ctx->iface = iface;
		dectnrp_l2_init(ctx->iface);
		LOG_INF("Iface initialized");
	}
	return;
}

static struct nrf91_context context = {0};

static const struct dectnrp_driver_api nrf91_api = {
	.iface_api = { .init = nrf91_net_if_init},
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

NET_DEVICE_INIT(dectnrp_nrf91, "dectnrp_nrf91", nrf91_init, NULL, &context, NULL,
		CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &nrf91_api, DECTNRP_L2,
		NET_L2_GET_CTX_TYPE(DECTNRP_L2), DECTNRP_MTU);
