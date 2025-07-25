// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"
#include "inc/pd_dpm_core.h"
#include "inc/pd_dpm_pdo_select.h"
#include "inc/pd_core.h"
#include "pd_dpm_prv.h"

struct pd_device_policy_manager {
	uint8_t temp;
};

static const struct svdm_svid_ops svdm_svid_ops[] = {
	{
		.name = "DisplayPort",
		.svid = USB_SID_DISPLAYPORT,
		.cable_svids = {
			.cnt = DP_ALT_MODE_CABLE_SVIDS_CNT,
			.svids = { USB_SID_TBT, USB_SID_DISPLAYPORT },
		},

		.dfp_inform_id = dp_dfp_u_notify_discover_id,
		.dfp_inform_svids = dp_dfp_u_notify_discover_svids,
		.dfp_inform_modes = dp_dfp_u_notify_discover_modes,

		.dfp_inform_enter_mode = dp_dfp_u_notify_enter_mode,
		.dfp_inform_exit_mode = dp_dfp_u_notify_exit_mode,

		.dfp_inform_attention = dp_dfp_u_notify_attention,

		.dfp_inform_cable_id = dp_dfp_u_notify_discover_cable_id,
		.dfp_inform_cable_svids = dp_dfp_u_notify_discover_cable_svids,
		.dfp_inform_cable_modes = dp_dfp_u_notify_discover_cable_modes,

		.ufp_request_enter_mode = dp_ufp_u_request_enter_mode,
		.ufp_request_exit_mode = dp_ufp_u_request_exit_mode,

		.notify_pe_startup = dp_dfp_u_notify_pe_startup,
		.notify_pe_ready = dp_dfp_u_notify_pe_ready,

		.reset_state = dp_reset_state,
		.parse_svid_data = dp_parse_svid_data,
	},
};

/*
 * DPM Init
 */

static void pd_dpm_update_pdos_flags(struct pd_port *pd_port, uint32_t pdo,
				     bool src)
{
	uint16_t dpm_flags = pd_port->pe_data.dpm_flags
		& ~DPM_FLAGS_RESET_PARTNER_MASK;

	/* Only update PDO flags if pdo's type is fixed */
	if ((pdo & PDO_TYPE_MASK) == PDO_TYPE_FIXED) {
		if (pdo & PDO_FIXED_DUAL_ROLE_POWER)
			dpm_flags |= DPM_FLAGS_PARTNER_DR_POWER;

		if (src && pdo & PDO_FIXED_USB_SUSPEND)
			dpm_flags |= DPM_FLAGS_PARTNER_USB_SUSPEND;

		if (!src && pdo & PDO_FIXED_HIGH_CAP)
			dpm_flags |= DPM_FLAGS_PARTNER_HIGH_CAP;

		if (pdo & PDO_FIXED_UNCONSTRAINED_POWER)
			dpm_flags |= DPM_FLAGS_PARTNER_EXTPOWER;

		if (pdo & PDO_FIXED_USB_COMM)
			dpm_flags |= DPM_FLAGS_PARTNER_USB_COMM;

		if (pdo & PDO_FIXED_DUAL_ROLE_DATA)
			dpm_flags |= DPM_FLAGS_PARTNER_DR_DATA;
	}

	pd_port->pe_data.dpm_flags = dpm_flags;
}


int pd_dpm_send_sink_caps(struct pd_port *pd_port)
{
	struct pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;

#if CONFIG_USB_PD_REV30_PPS_SINK
	if (pd_check_rev30(pd_port))
		snk_cap->nr = pd_port->local_snk_cap_nr_pd30;
	else
		snk_cap->nr = pd_port->local_snk_cap_nr_pd20;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	return pd_send_sop_data_msg(pd_port, PD_DATA_SINK_CAP,
		snk_cap->nr, snk_cap->pdos);
}

int pd_dpm_send_source_caps(struct pd_port *pd_port)
{
	uint8_t i;
	uint32_t cable_curr = 3000;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;
	struct pd_port_power_caps *src_cap0 = &pd_port->local_src_cap_default;
	struct pd_port_power_caps *src_cap1 = &pd_port->local_src_cap;

	if (pd_port->pe_data.cable_discovered_state) {
		cable_curr = pd_get_cable_current_limit(pd_port);
		DPM_DBG("cable_limit: %dmA\n", cable_curr);
	}

	src_cap1->nr = src_cap0->nr;
	for (i = 0; i < src_cap0->nr; i++) {
		src_cap1->pdos[i] =
			pd_reset_pdo_power(tcpc, src_cap0->pdos[i], cable_curr);
		DPM_DBG("SrcCap%d: 0x%08x\n", i+1, src_cap1->pdos[i]);
	}

	return pd_send_sop_data_msg(pd_port, PD_DATA_SOURCE_CAP,
		src_cap1->nr, src_cap1->pdos);
}

void pd_dpm_send_source_caps_0a(bool val)
{
	static struct tcpc_device *tcpc;
	struct pd_port *pd_port;
	uint32_t curr;

	tcpc = tcpc_dev_get_by_name("type_c_port0");
	if(!tcpc){
		printk("pd_dpm_send_source_caps_0a fail\n");
		return;
	}
	printk("pd_dpm_send_source_caps_0a success\n");
	pd_port = &tcpc->pd_port;
	if(pd_port->pe_state_curr == PE_SRC_READY){
		if(val)
			curr = 0x00019000;
		else
			curr = 0x00019032;
		pd_send_sop_data_msg(&tcpc->pd_port, PD_DATA_SOURCE_CAP,1,&curr);
	}
	else{
		printk("pd is not ready\n");
	}
	return;
}
EXPORT_SYMBOL(pd_dpm_send_source_caps_0a);

void pd_dpm_inform_cable_id(struct pd_port *pd_port, bool ack, bool src_startup)
{
	struct pe_data *pe_data = &pd_port->pe_data;
	uint32_t *payload = pd_get_msg_vdm_data_payload(pd_port);
	uint8_t cnt = pd_get_msg_vdm_data_count(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;
	int i = 0, offset = 0;
	char buf[100] = "\0";
	size_t buf_size = sizeof(buf);

	if (ack && payload) {
		offset += snprintf(buf + offset, buf_size - offset, "InformCableID");
		for (i = 0; i < cnt; i++)
			offset += snprintf(buf + offset, buf_size - offset, ", 0x%08x", payload[i]);

		offset += snprintf(buf + offset, buf_size - offset, "\n");
		DPM_DBG("%s", buf);

		memcpy(pe_data->cable_vdos, payload, sizeof(uint32_t) * cnt);
		pe_data->cable_discovered_state = CABLE_DISCOVERED_ID;
	}

	svdm_dfp_inform_cable_id(pd_port, ack, payload, cnt);

	if (src_startup)
		pd_enable_timer(pd_port, PD_TIMER_SOURCE_START);
	else
		VDM_STATE_DPM_INFORMED(pd_port);
}

void pd_dpm_inform_cable_svids(struct pd_port *pd_port, bool ack)
{
	struct pe_data *pe_data = &pd_port->pe_data;
	uint32_t *payload = pd_get_msg_vdm_data_payload(pd_port);
	uint8_t cnt = pd_get_msg_vdm_data_count(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;
	int i = 0, offset = 0;
	char buf[100] = "\0";
	size_t buf_size = sizeof(buf);

	if (ack && payload) {
		offset += snprintf(buf + offset, buf_size - offset, "InformCableSVIDs");
		for (i = 0; i < cnt; i++)
			offset += snprintf(buf + offset, buf_size - offset, ", 0x%08x", payload[i]);

		offset += snprintf(buf + offset, buf_size - offset, "\n");
		DPM_DBG("%s", buf);

		if (cnt < VDO_MAX_NR ||
		    !PD_VDO_SVID_SVID0(payload[cnt-1]) ||
		    !PD_VDO_SVID_SVID1(payload[cnt-1]))
			pe_data->cable_discovered_state =
				CABLE_DISCOVERED_SVIDS;
	}

	svdm_dfp_inform_cable_svids(pd_port, ack, payload, cnt);

	VDM_STATE_DPM_INFORMED(pd_port);
}

void pd_dpm_inform_cable_modes(struct pd_port *pd_port, bool ack)
{
	struct pe_data *pe_data = &pd_port->pe_data;
	uint32_t *payload = pd_get_msg_vdm_data_payload(pd_port);
	uint8_t cnt = pd_get_msg_vdm_data_count(pd_port);
	uint16_t svid = dpm_vdm_get_svid(pd_port);
	uint16_t expected_svid = pd_port->cable_mode_svid;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;
	int i = 0, offset = 0;
	char buf[100] = "\0";
	size_t buf_size = sizeof(buf);

	if (svid != expected_svid)
		DPM_INFO("Not expected SVID (0x%04x, 0x%04x)\n",
			 svid, expected_svid);

	if (ack && payload) {
		offset += snprintf(buf + offset, buf_size - offset, "InformCableModes");
		for (i = 0; i < cnt; i++)
			offset += snprintf(buf + offset, buf_size - offset, ", 0x%08x", payload[i]);

		offset += snprintf(buf + offset, buf_size - offset, "\n");
		DPM_DBG("%s", buf);

		if (svid == expected_svid)
			pe_data->cable_discovered_state =
				CABLE_DISCOVERED_MODES;
	}

	svdm_dfp_inform_cable_modes(pd_port, svid, ack, payload, cnt);
	VDM_STATE_DPM_INFORMED(pd_port);
}

static bool dpm_response_request(struct pd_port *pd_port, bool accept)
{
	if (accept)
		return pd_put_dpm_ack_event(pd_port);

	return pd_put_dpm_nak_event(pd_port, PD_DPM_NAK_REJECT);
}

/* ---- SNK ---- */

static void dpm_build_sink_pdo_info(struct dpm_pdo_info_t *sink,
		uint8_t type, int request_v, int request_i)
{
	memset(sink, 0, sizeof(*sink));

	sink->type = type;

#if CONFIG_USB_PD_REV30_PPS_SINK
	if (type == DPM_PDO_TYPE_APDO) {
		request_v = (request_v / 20) * 20;
		request_i = (request_i / 50) * 50;
	} else
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */
		request_i = (request_i / 10) * 10;

	sink->vmax = sink->vmin = request_v;
	sink->uw = request_v * request_i;
	sink->ma = request_i;
}

static inline bool dpm_build_request_info_with_new_src_cap(
		struct pd_port *pd_port, struct dpm_rdo_info_t *req_info,
		struct pd_port_power_caps *src_cap, uint8_t policy)
{
	struct dpm_pdo_info_t sink = pd_port->last_sink_pdo_info;

	return dpm_find_match_req_info(pd_port, req_info,
			&sink, src_cap->nr, src_cap->pdos,
			-1, policy);
}

#if CONFIG_USB_PD_REV30_PPS_SINK
void pd_dpm_start_pps_request(struct pd_port *pd_port, bool en)
{
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_INFO("pps (%s)\n", en ? "start" : "end");
	if (en) {
		pm_stay_awake(&tcpc->dev);
		pd_restart_timer(pd_port, PD_TIMER_PPS_REQUEST);
	} else {
		pd_disable_timer(pd_port, PD_TIMER_PPS_REQUEST);
		pm_relax(&tcpc->dev);
	}
}

static bool dpm_build_request_info_apdo(
		struct pd_port *pd_port, struct dpm_rdo_info_t *req_info,
		struct pd_port_power_caps *src_cap, uint8_t policy)
{
	struct dpm_pdo_info_t sink;

	dpm_build_sink_pdo_info(&sink, DPM_PDO_TYPE_APDO,
			pd_port->request_v_apdo, pd_port->request_i_apdo);

	return dpm_find_match_req_info(pd_port, req_info,
			&sink, src_cap->nr, src_cap->pdos,
			-1, policy);
}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */


static bool dpm_build_request_info_pdo(
		struct pd_port *pd_port, struct dpm_rdo_info_t *req_info,
		struct pd_port_power_caps *src_cap, uint8_t policy)
{
	bool find_cap = false;
	int i, max_uw = -1;
	struct dpm_pdo_info_t sink;
	struct pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	for (i = 0; i < snk_cap->nr; i++) {
		DPM_DBG("EvaSinkCap%d\n", i+1);
		dpm_extract_pdo_info(snk_cap->pdos[i], &sink);

		find_cap = dpm_find_match_req_info(pd_port, req_info,
				&sink, src_cap->nr, src_cap->pdos,
				max_uw, policy);

		if (find_cap) {
			if (req_info->type == DPM_PDO_TYPE_BAT)
				max_uw = req_info->oper_uw;
			else
				max_uw = req_info->vmax * req_info->oper_ma;

			DPM_DBG("Find SrcCap%d(%s):%d mw\n",
					req_info->pos, req_info->mismatch ?
					"Mismatch" : "Match", max_uw/1000);
		}
	}

	return max_uw >= 0;
}

static bool dpm_build_request_info(
		struct pd_port *pd_port, struct dpm_rdo_info_t *req_info)
{
	int i;
	uint8_t policy = pd_port->dpm_charging_policy;
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;
	struct pd_event *pd_event = pd_get_curr_pd_event(pd_port);

	memset(req_info, 0, sizeof(struct dpm_rdo_info_t));

	DPM_INFO("Policy=0x%X\n", policy);

	for (i = 0; i < src_cap->nr; i++)
		DPM_INFO("SrcCap%d: 0x%08x\n", i+1, src_cap->pdos[i]);

	if (pd_event_data_msg_match(pd_event, PD_DATA_SOURCE_CAP) &&
		pd_port->pe_data.explicit_contract) {
		pd_update_connect_state(pd_port, PD_CONNECT_NEW_SRC_CAP);
		if (dpm_build_request_info_with_new_src_cap(
				pd_port, req_info, src_cap, policy))
			return true;
	}

#if CONFIG_USB_PD_REV30_PPS_SINK
	if ((policy & DPM_CHARGING_POLICY_MASK) >= DPM_CHARGING_POLICY_PPS) {
		if (dpm_build_request_info_apdo(
				pd_port, req_info, src_cap, policy))
			return true;
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	return dpm_build_request_info_pdo(
			pd_port, req_info, src_cap, policy);
}

static bool dpm_build_default_request_info(
		struct pd_port *pd_port, struct dpm_rdo_info_t *req_info)
{
	struct dpm_pdo_info_t source;
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;

	dpm_extract_pdo_info(src_cap->pdos[0], &source);

	req_info->pos = 1;
	req_info->type = source.type;
	req_info->mismatch = false;
	req_info->vmin = source.vmin;
	req_info->vmax = source.vmax;
	req_info->max_ma = source.ma;
	req_info->oper_ma = source.ma;

	return true;
}

static inline void dpm_update_request_i_new(
		struct pd_port *pd_port, struct dpm_rdo_info_t *req_info)
{
	if (req_info->mismatch)
		pd_port->request_i_new = pd_port->request_i_op;
	else
		pd_port->request_i_new = pd_port->request_i_max;
}

static inline void dpm_update_request_bat(struct pd_port *pd_port,
	struct dpm_rdo_info_t *req_info, uint32_t flags)
{
	uint32_t mw_max, mw_op;

	pd_port->request_i_max = req_info->max_uw / req_info->vmin;
	pd_port->request_i_op = req_info->oper_uw / req_info->vmin;

	dpm_update_request_i_new(pd_port, req_info);

	mw_max = req_info->max_uw / 1000;
	mw_op = req_info->oper_uw / 1000;

	pd_port->last_rdo = RDO_BATT(
			req_info->pos, mw_op, mw_max, flags);
}

static inline void dpm_update_request_not_bat(struct pd_port *pd_port,
	struct dpm_rdo_info_t *req_info, uint32_t flags)
{
	pd_port->request_i_max = req_info->max_ma;
	pd_port->request_i_op = req_info->oper_ma;

	dpm_update_request_i_new(pd_port, req_info);

#if CONFIG_USB_PD_REV30_PPS_SINK
	if (req_info->type == DPM_PDO_TYPE_APDO) {
		pd_port->request_apdo = true;
		pd_port->last_rdo = RDO_APDO(
				req_info->pos, req_info->vmin,
				req_info->oper_ma, flags);
		return;
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	pd_port->last_rdo = RDO_FIXED(
			req_info->pos, req_info->oper_ma,
			req_info->max_ma, flags);
}

static inline void dpm_update_request(
	struct pd_port *pd_port, struct dpm_rdo_info_t *req_info)
{
	uint32_t flags = 0;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;
	const bool rejected = pd_port->pe_data.request_rejected;

#if CONFIG_USB_PD_REV30_PPS_SINK
	pd_port->request_apdo = false;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_GIVE_BACK)
		flags |= RDO_GIVE_BACK;

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_NO_SUSPEND)
		flags |= RDO_NO_SUSPEND;

	if (pd_port->dpm_caps & DPM_CAP_LOCAL_USB_COMM)
		flags |= RDO_COMM_CAP;

	if (req_info->mismatch) {
		DPM_INFO("cap miss match case, rejected=%d\n", rejected);
		if (rejected) {
			req_info->mismatch = false;
			req_info->max_ma = req_info->oper_ma;
		} else {
			flags |= RDO_CAP_MISMATCH;
		}
	}

	pd_port->request_v_new = req_info->vmin;

	if (req_info->type == DPM_PDO_TYPE_BAT)
		dpm_update_request_bat(pd_port, req_info, flags);
	else
		dpm_update_request_not_bat(pd_port, req_info, flags);

#if CONFIG_USB_PD_DIRECT_CHARGE
	pd_notify_pe_direct_charge(pd_port,
			req_info->vmin < TCPC_VBUS_SINK_5V);
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */
}

int pd_dpm_update_tcp_request(struct pd_port *pd_port,
		struct tcp_dpm_pd_request *pd_req)
{
	bool find_cap = false;
	uint8_t type = DPM_PDO_TYPE_FIXED;
	struct dpm_pdo_info_t sink;
	struct dpm_rdo_info_t req_info;
	uint8_t policy = pd_port->dpm_charging_policy;
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	memset(&req_info, 0, sizeof(struct dpm_rdo_info_t));

	DPM_INFO("Policy=0x%X\n", policy);

#if CONFIG_USB_PD_REV30_PPS_SINK
	if ((policy & DPM_CHARGING_POLICY_MASK) >= DPM_CHARGING_POLICY_PPS)
		type = DPM_PDO_TYPE_APDO;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	dpm_build_sink_pdo_info(&sink, type, pd_req->mv, pd_req->ma);

#if CONFIG_USB_PD_REV30_PPS_SINK
	if (pd_port->request_apdo &&
		(sink.vmin == pd_port->request_v) &&
		(sink.ma == pd_port->request_i))
		return TCP_DPM_RET_DENIED_REPEAT_REQUEST;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	find_cap = dpm_find_match_req_info(pd_port, &req_info,
			&sink, src_cap->nr, src_cap->pdos,
			-1, policy);

	if (!find_cap) {
		DPM_INFO("Can't find match_cap\n");
		return TCP_DPM_RET_DENIED_INVALID_REQUEST;
	}

#if CONFIG_USB_PD_REV30_PPS_SINK
	if ((policy & DPM_CHARGING_POLICY_MASK) >= DPM_CHARGING_POLICY_PPS) {
		pd_port->request_v_apdo = sink.vmin;
		pd_port->request_i_apdo = sink.ma;
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	dpm_update_request(pd_port, &req_info);
	return TCP_DPM_RET_SUCCESS;
}

int pd_dpm_update_tcp_request_ex(struct pd_port *pd_port,
	struct tcp_dpm_pd_request_ex *pd_req)
{
	struct dpm_pdo_info_t source;
	struct dpm_rdo_info_t req_info;
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (pd_req->pos > src_cap->nr)
		return TCP_DPM_RET_DENIED_INVALID_REQUEST;

	dpm_extract_pdo_info(src_cap->pdos[pd_req->pos-1], &source);

	req_info.pos = pd_req->pos;
	req_info.type = source.type;
	req_info.vmin = pd_req->vmin;
	req_info.vmax = source.vmax;

	if (req_info.type == DPM_PDO_TYPE_BAT) {
		req_info.mismatch = source.uw < pd_req->max_uw;
		req_info.max_uw = pd_req->max_uw;
		req_info.oper_uw = pd_req->oper_uw;
	} else {
		req_info.mismatch = source.ma < pd_req->max_ma;
		req_info.max_ma = pd_req->max_ma;
		req_info.oper_ma = pd_req->oper_ma;
	}

	dpm_update_request(pd_port, &req_info);
	return TCP_DPM_RET_SUCCESS;
}

static uint8_t pd_dpm_build_rdo(struct pd_port *pd_port)
{
	bool find_cap = false;
	struct dpm_rdo_info_t req_info;
	struct pd_port_power_caps *snk_cap = &pd_port->local_snk_cap;
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if ((src_cap->nr <= 0) || (snk_cap->nr <= 0)) {
		DPM_INFO("SrcNR or SnkNR = 0\n");
		return 0;
	}

	find_cap = dpm_build_request_info(pd_port, &req_info);

	/* If we can't find any cap to use, choose default setting */
	if (!find_cap) {
		DPM_INFO("Can't find any SrcCap\n");
		dpm_build_default_request_info(pd_port, &req_info);
	} else
		DPM_INFO("Select SrcCap%d\n", req_info.pos);

	dpm_update_request(pd_port, &req_info);

	return req_info.pos;
}

int pd_dpm_update_tcp_request_again(struct pd_port *pd_port)
{
	if (pd_dpm_build_rdo(pd_port))
		return TCP_DPM_RET_SUCCESS;
	else
		return TCP_DPM_RET_DENIED_INVALID_REQUEST;
}

void pd_dpm_snk_evaluate_caps(struct pd_port *pd_port)
{
	uint8_t pos = 0;

	PD_BUG_ON(pd_get_msg_data_payload(pd_port) == NULL);

	pd_dpm_dr_inform_source_cap(pd_port);

	pos = pd_dpm_build_rdo(pd_port);
	if (pos)
		pd_put_dpm_notify_event(pd_port, pos);
}

void pd_dpm_snk_standby_power(struct pd_port *pd_port)
{
#if CONFIG_USB_PD_SNK_STANDBY_POWER
	/*
	 * pSnkStdby :
	 *   Maximum power consumption while in Sink Standby. (2.5W)
	 * I1 = (pSnkStdby/VBUS)
	 * I2 = (pSnkStdby/VBUS) + cSnkBulkPd(DVBUS/Dt)
	 * STANDBY_UP = I1 < I2, STANDBY_DOWN = I1 > I2
	 *
	 * tSnkNewPower (t1):
	 *   Maximum transition time between power levels. (15ms)
	 */

	uint8_t type;
	int ma = -1;
	int standby_curr = 2500000 / max(pd_port->request_v,
					 pd_port->request_v_new);

#if CONFIG_USB_PD_VCONN_SAFE5V_ONLY
	struct tcpc_device *tcpc = pd_port->tcpc;
	struct pe_data *pe_data = &pd_port->pe_data;
	bool vconn_highv_prot = pd_port->request_v_new > 5000;

	if (!pe_data->vconn_highv_prot && vconn_highv_prot &&
		tcpc->tcpc_flags & TCPC_FLAGS_VCONN_SAFE5V_ONLY) {
		PE_INFO("VC_HIGHV_PROT: %d\n", vconn_highv_prot);
		pe_data->vconn_highv_prot_role = pd_port->vconn_role;
		pd_set_vconn(pd_port, PD_ROLE_VCONN_OFF);
		pe_data->vconn_highv_prot = vconn_highv_prot;
	}
#endif	/* CONFIG_USB_PD_VCONN_SAFE5V_ONLY */

#if CONFIG_USB_PD_REV30_PPS_SINK
	/*
	 * A Sink is not required to transition to Sink Standby
	 *	when operating with a Programmable Power Supply
	 *	(Check it later, against new spec)
	 */
	if (pd_port->request_apdo)
		return;
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

	if (pd_port->request_v_new > pd_port->request_v) {
		/* Case2 Increasing the Voltage */
		/* Case3 Increasing the Voltage and Current */
		/* Case4 Increasing the Voltage and Decreasing the Curren */
		ma = standby_curr;
		type = TCP_VBUS_CTRL_STANDBY_UP;
	} else if (pd_port->request_v_new < pd_port->request_v) {
		/* Case5 Decreasing the Voltage and Increasing the Current */
		/* Case7 Decreasing the Voltage */
		/* Case8 Decreasing the Voltage and the Current*/
		ma = standby_curr;
		type = TCP_VBUS_CTRL_STANDBY_DOWN;
	} else if (pd_port->request_i_new < pd_port->request_i) {
		/* Case6 Decreasing the Current, t1 i = new */
		ma = pd_port->request_i_new;
		type = TCP_VBUS_CTRL_STANDBY;
	}

	if (ma >= 0) {
		tcpci_sink_vbus(
			pd_port->tcpc, type, pd_port->request_v_new, ma);
	}
#else
#if CONFIG_USB_PD_SNK_GOTOMIN
	tcpci_sink_vbus(pd_port->tcpc, TCP_VBUS_CTRL_REQUEST,
		pd_port->request_v, pd_port->request_i_new);
#endif	/* CONFIG_USB_PD_SNK_GOTOMIN */
#endif	/* CONFIG_USB_PD_SNK_STANDBY_POWER */
}

void pd_dpm_snk_transition_power(struct pd_port *pd_port)
{
	tcpci_sink_vbus(pd_port->tcpc, TCP_VBUS_CTRL_REQUEST,
		pd_port->request_v_new, pd_port->request_i_new);

	pd_port->request_v = pd_port->request_v_new;
	pd_port->request_i = pd_port->request_i_new;

#if CONFIG_USB_PD_REV30_PPS_SINK
	pd_dpm_start_pps_request(pd_port, pd_port->request_apdo);
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */
}

void pd_dpm_snk_hard_reset(struct pd_port *pd_port)
{
	/*
	 * tSnkHardResetPrepare :
	 * Time allotted for the Sink power electronics
	 * to prepare for a Hard Reset
	 */

	int mv = 0, ma = 0;
	bool ignore_hreset = false;

	if (!pd_port->pe_data.pd_prev_connected) {
#if CONFIG_USB_PD_SNK_IGNORE_HRESET_IF_TYPEC_ONLY
		ignore_hreset = true;
#else
		ma = -1;
		mv = TCPC_VBUS_SINK_5V;
#endif	/* CONFIG_USB_PD_SNK_IGNORE_HRESET_IF_TYPEC_ONLY */
	}

	if (!ignore_hreset) {
		tcpci_sink_vbus(
			pd_port->tcpc, TCP_VBUS_CTRL_HRESET, mv, ma);
	}

	pd_put_pe_event(pd_port, PD_PE_POWER_ROLE_AT_DEFAULT);
}

/* ---- SRC ---- */

static inline bool dpm_evaluate_request(
	struct pd_port *pd_port, uint32_t rdo, uint8_t rdo_pos)
{
	uint32_t pdo;
	uint32_t sink_v;
	uint32_t op_curr, max_curr;
	struct dpm_pdo_info_t src_info;
	struct pd_port_power_caps *src_cap = &pd_port->local_src_cap;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	pd_port->pe_data.dpm_flags &= (~DPM_FLAGS_PARTNER_MISMATCH);

	if ((rdo_pos == 0) || (rdo_pos > src_cap->nr)) {
		DPM_INFO("RequestPos Wrong (%d)\n", rdo_pos);
		return false;
	}

	pdo = src_cap->pdos[rdo_pos-1];

	dpm_extract_pdo_info(pdo, &src_info);
	pd_extract_rdo_power(rdo, pdo, &op_curr, &max_curr);

	if (src_info.ma < op_curr) {
		DPM_INFO("src_i (%d) < op_i (%d)\n", src_info.ma, op_curr);
		return false;
	}

	if (rdo & RDO_CAP_MISMATCH) {
		/* TODO: handle it later */
		DPM_INFO("CAP_MISMATCH\n");
		pd_port->pe_data.dpm_flags |= DPM_FLAGS_PARTNER_MISMATCH;
	} else if (src_info.ma < max_curr) {
		DPM_INFO("src_i (%d) < max_i (%d)\n", src_info.ma, max_curr);
		return false;
	}

	sink_v = src_info.vmin;

#if CONFIG_USB_PD_REV30_PPS_SOURCE
	if ((pdo & PDO_TYPE_MASK) == PDO_TYPE_APDO) {
		sink_v = RDO_APDO_EXTRACT_OP_MV(rdo);

		if ((sink_v < src_info.vmin) || (sink_v > src_info.vmax)) {
			DPM_INFO("sink_v (%d) not in src_v (%d~%d)\n",
				sink_v, src_info.vmin, src_info.vmax);
			return false;
		}
	}
#endif	/* CONFIG_USB_PD_REV30_PPS_SOURCE */

	/* Accept request */

	pd_port->request_i_max = max_curr;
	pd_port->request_i_op = op_curr;

	if (rdo & RDO_CAP_MISMATCH)
		pd_port->request_i_new = op_curr;
	else
		pd_port->request_i_new = max_curr;

	pd_port->request_v_new = sink_v;
	return true;
}

void pd_dpm_src_evaluate_request(struct pd_port *pd_port)
{
	uint32_t rdo;
	uint8_t rdo_pos;
	struct pe_data *pe_data;
	uint32_t *payload = pd_get_msg_data_payload(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	PD_BUG_ON(payload == NULL);

	rdo = payload[0];
	rdo_pos = RDO_POS(rdo);

	DPM_INFO("RequestCap%d\n", rdo_pos);

	pe_data = &pd_port->pe_data;

	if (dpm_evaluate_request(pd_port, rdo, rdo_pos))  {
		pe_data->selected_cap = rdo_pos;
		pd_put_dpm_notify_event(pd_port, rdo_pos);
	} else {
		/*
		 * "Contract Invalid" means that the previously
		 * negotiated Voltage and Current values
		 * are no longer included in the Sources new Capabilities.
		 * If the Sink fails to make a valid Request in this case
		 * then Power Delivery operation is no longer possible
		 * and Power Delivery mode is exited with a Hard Reset.
		 */

		pe_data->invalid_contract = false;
		pe_data->selected_cap = 0;
		pd_put_dpm_nak_event(pd_port, PD_DPM_NAK_REJECT);
	}
}

void pd_dpm_src_transition_power(struct pd_port *pd_port)
{
	pd_enable_vbus_stable_detection(pd_port);

#if CONFIG_USB_PD_SRC_HIGHCAP_POWER
	if (pd_port->request_v > pd_port->request_v_new) {
		mutex_lock(&pd_port->tcpc->access_lock);
		tcpci_enable_force_discharge(
			pd_port->tcpc, true, pd_port->request_v_new);
		mutex_unlock(&pd_port->tcpc->access_lock);
	}
#endif	/* CONFIG_USB_PD_SRC_HIGHCAP_POWER */

	tcpci_source_vbus(pd_port->tcpc, TCP_VBUS_CTRL_REQUEST,
		pd_port->request_v_new, pd_port->request_i_new);

	if (pd_port->request_v == pd_port->request_v_new)
		pd_put_vbus_stable_event(pd_port->tcpc);
#if CONFIG_USB_PD_VBUS_STABLE_TOUT
	else
		pd_enable_timer(pd_port, PD_TIMER_VBUS_STABLE);
#endif	/* CONFIG_USB_PD_VBUS_STABLE_TOUT */

	pd_port->request_v = pd_port->request_v_new;
	pd_port->request_i = pd_port->request_i_new;
}

void pd_dpm_src_hard_reset(struct pd_port *pd_port)
{
	tcpci_source_vbus(pd_port->tcpc,
		TCP_VBUS_CTRL_HRESET, TCPC_VBUS_SOURCE_0V, 0);
	pd_enable_vbus_safe0v_detection(pd_port);
}

/* ---- UFP : update_svid_data ---- */

static inline bool dpm_ufp_update_svid_data_enter_mode(
	struct pd_port *pd_port, uint16_t svid, uint8_t ops)
{
	struct svdm_svid_data *svid_data;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_DBG("EnterMode (svid0x%04x, ops:%d)\n", svid, ops);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);

	if (svid_data == NULL)
		return false;

	/* Only accept 1 mode active at the same time */
	if (svid_data->active_mode)
		return false;

	if ((ops == 0) || (ops > svid_data->local_mode.mode_cnt))
		return false;

	svid_data->active_mode = ops;
	pd_port->pe_data.modal_operation = true;

	svdm_ufp_request_enter_mode(pd_port, svid, ops);

	tcpci_enter_mode(tcpc, svid, ops, svid_data->local_mode.mode_vdo[ops]);
	return true;
}

static bool dpm_update_svid_data_exit_mode(
	struct pd_port *pd_port, uint16_t svid, uint8_t ops)
{
	uint8_t i;
	bool modal_operation = false, found = false;
	struct svdm_svid_data *svid_data;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_DBG("ExitMode (svid0x%04x, mode:%d)\n", svid, ops);

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];
		if (!svid_data->active_mode)
			continue;
		if (svid_data->svid != svid ||
		    (ops != 7 && svid_data->active_mode != ops)) {
			modal_operation = true;
			continue;
		}
		svid_data->active_mode = 0;
		found = true;
	}
	pd_port->pe_data.modal_operation = modal_operation;

	return found;
}

static inline bool dpm_ufp_update_svid_data_exit_mode(
	struct pd_port *pd_port, uint16_t svid, uint8_t ops)
{
	bool found = false;

	found = dpm_update_svid_data_exit_mode(pd_port, svid, ops);
	if (found) {
		svdm_ufp_request_exit_mode(pd_port, svid, ops);
		tcpci_exit_mode(pd_port->tcpc, svid);
	}
	return found;
}


/* ---- UFP : Response VDM Request ---- */

static int dpm_vdm_ufp_response_id(struct pd_port *pd_port)
{
	uint8_t *cnt = &pd_port->pe_data.pd_response_id30_cnt;
	bool rev30 = pd_check_rev30(pd_port) && *cnt < PD_RESPONSE_ID30_COUNT;

	if (rev30) {
		pd_port->id_vdos[0] = pd_port->id_header;
		*cnt += 1;
	} else
		pd_port->id_vdos[0] = VDO_IDH_PD20(pd_port->id_header);

	return pd_reply_svdm_request(pd_port, CMDT_RSP_ACK,
		rev30 ? pd_port->id_vdo_nr : 3, pd_port->id_vdos);
}

static int dpm_ufp_response_svids(struct pd_port *pd_port)
{
	struct svdm_svid_data *svid_data;
	uint16_t svid_list[2];
	uint32_t svids[VDO_MAX_NR];
	uint8_t i = 0, j = 0, cnt = pd_port->svid_data_cnt;

	PD_BUG_ON(pd_port->svid_data_cnt >= VDO_MAX_SVID_NR);

	while (i < cnt) {
		svid_data = &pd_port->svid_data[i++];
		svid_list[0] = svid_data->svid;

		if (i < cnt) {
			svid_data = &pd_port->svid_data[i++];
			svid_list[1] = svid_data->svid;
		} else
			svid_list[1] = 0;

		svids[j++] = VDO_SVID(svid_list[0], svid_list[1]);
	}

	if ((cnt % 2) == 0)
		svids[j++] = VDO_SVID(0, 0);

	return pd_reply_svdm_request(pd_port, CMDT_RSP_ACK, j, svids);
}

static int dpm_vdm_ufp_response_modes(struct pd_port *pd_port)
{
	struct svdm_svid_data *svid_data;
	uint16_t svid = dpm_vdm_get_svid(pd_port);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);

	PD_BUG_ON(svid_data == NULL);

	return pd_reply_svdm_request(
		pd_port, CMDT_RSP_ACK,
		svid_data->local_mode.mode_cnt,
		svid_data->local_mode.mode_vdo);
}

/* ---- UFP : Handle VDM Request ---- */

void pd_dpm_ufp_request_id_info(struct pd_port *pd_port)
{
	bool ack = dpm_vdm_get_svid(pd_port) == USB_SID_PD;

	if (!ack) {
		dpm_vdm_reply_svdm_nak(pd_port);
		return;
	}

	dpm_vdm_ufp_response_id(pd_port);
}

void pd_dpm_ufp_request_svid_info(struct pd_port *pd_port)
{
	bool ack = false;

	if (pd_is_support_modal_operation(pd_port))
		ack = (dpm_vdm_get_svid(pd_port) == USB_SID_PD);

	if (!ack) {
		dpm_vdm_reply_svdm_nak(pd_port);
		return;
	}

	dpm_ufp_response_svids(pd_port);
}

void pd_dpm_ufp_request_mode_info(struct pd_port *pd_port)
{
	uint16_t svid = dpm_vdm_get_svid(pd_port);

	if (dpm_get_svdm_svid_data(pd_port, svid)) {
		dpm_vdm_ufp_response_modes(pd_port);
		return;
	}

	dpm_vdm_reply_svdm_nak(pd_port);
}

void pd_dpm_ufp_request_enter_mode(struct pd_port *pd_port)
{
	bool ack = dpm_ufp_update_svid_data_enter_mode(pd_port,
		dpm_vdm_get_svid(pd_port), dpm_vdm_get_ops(pd_port));

	dpm_vdm_reply_svdm_request(pd_port, ack);
}

void pd_dpm_ufp_request_exit_mode(struct pd_port *pd_port)
{
	bool ack = dpm_ufp_update_svid_data_exit_mode(pd_port,
		dpm_vdm_get_svid(pd_port), dpm_vdm_get_ops(pd_port));

	dpm_vdm_reply_svdm_request(pd_port, ack);
}

/* ---- DFP : update_svid_data ---- */

static inline void dpm_dfp_update_partner_id(
			struct pd_port *pd_port, uint32_t *payload)
{
#if CONFIG_USB_PD_KEEP_PARTNER_ID
	struct pe_data *pe_data = &pd_port->pe_data;
	uint8_t cnt = pd_get_msg_vdm_data_count(pd_port);
	uint32_t size = sizeof(uint32_t) * (cnt);

	memcpy(pe_data->partner_vdos, payload, size);
	memset((char *)pe_data->partner_vdos + size, 0,
		sizeof(uint32_t) * VDO_MAX_NR - size);
	pe_data->partner_id_present = true;
#endif	/* CONFIG_USB_PD_KEEP_PARTNER_ID */
}
static inline void dpm_dfp_update_svid_data_exist(
			struct pd_port *pd_port, uint16_t svid)
{
	uint8_t k;
	struct svdm_svid_data *svid_data;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;
#if CONFIG_USB_PD_KEEP_SVIDS
	struct svdm_svid_list *list = &pd_port->pe_data.remote_svid_list;

	if (list->cnt < VDO_MAX_SVID_NR)
		list->svids[list->cnt++] = svid;
	else
		DPM_INFO("ERR:SVIDCNT\n");
#endif	/* CONFIG_USB_PD_KEEP_SVIDS */

	for (k = 0; k < pd_port->svid_data_cnt; k++) {

		svid_data = &pd_port->svid_data[k];

		if (svid_data->svid == svid)
			svid_data->exist = true;
	}
}

static inline void dpm_dfp_update_svid_data_modes(struct pd_port *pd_port,
	uint16_t svid, uint32_t *mode_list, uint8_t count)
{
	uint8_t i;
	struct svdm_svid_data *svid_data;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_DBG("InformMode (0x%04x:%d):\n", svid, count);
	for (i = 0; i < count; i++)
		DPM_DBG("Mode[%d]: 0x%08x\n", i, mode_list[i]);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data == NULL)
		return;

	svid_data->remote_mode.mode_cnt = count;
	memcpy(svid_data->remote_mode.mode_vdo, mode_list,
	       sizeof(uint32_t) * count);
}

static inline void dpm_dfp_update_svid_data_enter_mode(
	struct pd_port *pd_port, uint16_t svid, uint8_t ops)
{
	struct svdm_svid_data *svid_data;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_DBG("EnterMode (svid0x%04x, mode:%d)\n", svid, ops);

	svid_data = dpm_get_svdm_svid_data(pd_port, svid);
	if (svid_data == NULL)
		return;

	svid_data->active_mode = ops;
	pd_port->pe_data.modal_operation = true;

	tcpci_enter_mode(tcpc, svid, ops, svid_data->remote_mode.mode_vdo[ops]);
}

static inline void dpm_dfp_update_svid_data_exit_mode(
	struct pd_port *pd_port, uint16_t svid, uint8_t ops)
{
	if (dpm_update_svid_data_exit_mode(pd_port, svid, ops))
		tcpci_exit_mode(pd_port->tcpc, svid);
}


/* ---- DFP : Inform VDM Result ---- */

void pd_dpm_dfp_inform_id(struct pd_port *pd_port, bool ack)
{
	uint32_t *payload = pd_get_msg_vdm_data_payload(pd_port);
	uint8_t cnt = pd_get_msg_vdm_data_count(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;
	int i = 0, offset = 0;
	char buf[100] = "\0";
	size_t buf_size = sizeof(buf);

	VDM_STATE_DPM_INFORMED(pd_port);

	if (ack && payload) {
		offset += snprintf(buf + offset, buf_size - offset, "InformID");
		for (i = 0; i < cnt; i++)
			offset += snprintf(buf + offset, buf_size - offset, ", 0x%08x", payload[i]);

		offset += snprintf(buf + offset, buf_size - offset, "\n");
		DPM_DBG("%s", buf);

		dpm_dfp_update_partner_id(pd_port, payload);
	}

	if (!pd_port->pe_data.vdm_discard_retry_flag) {
		/*
		 * For PD compliance test,
		 * If device doesn't reply discoverID
		 * or doesn't support modal operation,
		 * then don't send discoverSVIDs
		 */
		if (!ack || !payload)
			dpm_reaction_clear(pd_port,
					   DPM_REACTION_DISCOVER_SVIDS);
		else if (!(payload[0] & PD_IDH_MODAL_SUPPORT))
			dpm_reaction_clear(pd_port,
					   DPM_REACTION_DISCOVER_SVIDS);
		else
			dpm_reaction_set(pd_port, DPM_REACTION_DISCOVER_SVIDS);

		svdm_dfp_inform_id(pd_port, ack);
		dpm_reaction_clear(pd_port, DPM_REACTION_DISCOVER_ID);
	}
}

static inline int dpm_dfp_consume_svids(
	struct pd_port *pd_port, uint32_t *svid_list, uint8_t count)
{
	bool discover_again = true;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;
	uint8_t i, j;
	uint16_t svid[2];

	DPM_DBG("InformSVID (%d):\n", count);

	if (count < 6)
		discover_again = false;

	for (i = 0; i < count; i++) {
		svid[0] = PD_VDO_SVID_SVID0(svid_list[i]);
		svid[1] = PD_VDO_SVID_SVID1(svid_list[i]);

		DPM_DBG("svid[%d]: 0x%04x 0x%04x\n", i, svid[0], svid[1]);

		for (j = 0; j < 2; j++) {
			if (svid[j] == 0) {
				discover_again = false;
				break;
			}

			dpm_dfp_update_svid_data_exist(pd_port, svid[j]);
		}
	}

	if (discover_again) {
		DPM_DBG("DiscoverSVID Again\n");
		pd_put_tcp_vdm_event(pd_port, TCP_DPM_EVT_DISCOVER_SVIDS);
		return 1;
	}

	return 0;
}

void pd_dpm_dfp_inform_svids(struct pd_port *pd_port, bool ack)
{
	uint32_t *payload = pd_get_msg_vdm_data_payload(pd_port);
	uint8_t cnt = pd_get_msg_vdm_data_count(pd_port);

	VDM_STATE_DPM_INFORMED(pd_port);

	if (ack && payload)
		if (dpm_dfp_consume_svids(pd_port, payload, cnt))
			return;

	if (!pd_port->pe_data.vdm_discard_retry_flag) {
		svdm_dfp_inform_svids(pd_port, ack);
		dpm_reaction_clear(pd_port, DPM_REACTION_DISCOVER_SVIDS);
	}
}

void pd_dpm_dfp_inform_modes(struct pd_port *pd_port, bool ack)
{
	uint32_t *payload = pd_get_msg_vdm_data_payload(pd_port);
	uint8_t cnt = pd_get_msg_vdm_data_count(pd_port);
	uint16_t svid = dpm_vdm_get_svid(pd_port);
	uint16_t expected_svid = pd_port->mode_svid;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (svid != expected_svid)
		DPM_INFO("Not expected SVID (0x%04x, 0x%04x)\n",
			 svid, expected_svid);

	if (ack && payload)
		dpm_dfp_update_svid_data_modes(pd_port, svid, payload, cnt);

	svdm_dfp_inform_modes(pd_port, svid, ack);
	VDM_STATE_DPM_INFORMED(pd_port);
}

void pd_dpm_dfp_inform_enter_mode(struct pd_port *pd_port, bool ack)
{
	uint16_t svid = dpm_vdm_get_svid(pd_port);
	uint16_t expected_svid = pd_port->mode_svid;
	uint8_t ops = dpm_vdm_get_ops(pd_port);
	uint8_t expected_ops = pd_port->mode_obj_pos;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (svid != expected_svid)
		DPM_INFO("Not expected SVID (0x%04x, 0x%04x)\n",
			 svid, expected_svid);
	else if (ops != expected_ops)
		DPM_INFO("Not expected ops (%d, %d)\n", ops, expected_ops);

	if (ack)
		dpm_dfp_update_svid_data_enter_mode(pd_port, svid, ops);

	svdm_dfp_inform_enter_mode(pd_port, svid, ops, ack);
	VDM_STATE_DPM_INFORMED(pd_port);
}

void pd_dpm_dfp_inform_exit_mode(struct pd_port *pd_port)
{
	bool ack = PD_VDO_CMDT(pd_get_msg_vdm_hdr(pd_port)) == CMDT_RSP_ACK;
	uint16_t svid = dpm_vdm_get_svid(pd_port);
	uint16_t expected_svid = pd_port->mode_svid;
	uint8_t ops = dpm_vdm_get_ops(pd_port);
	uint8_t expected_ops = pd_port->mode_obj_pos;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (svid != expected_svid)
		DPM_INFO("Not expected SVID (0x%04x, 0x%04x)\n",
			 svid, expected_svid);
	else if (ops != expected_ops)
		DPM_INFO("Not expected ops (%d, %d)\n", ops, expected_ops);

	if (ack) {
		dpm_dfp_update_svid_data_exit_mode(pd_port, svid, ops);
		svdm_dfp_inform_exit_mode(pd_port, svid, ops);
	}

	VDM_STATE_DPM_INFORMED(pd_port);
}

void pd_dpm_dfp_inform_attention(struct pd_port *pd_port)
{
#if DPM_DBG_ENABLE
	uint8_t ops = dpm_vdm_get_ops(pd_port);
#endif
	uint16_t svid = dpm_vdm_get_svid(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_DBG("Attention (svid0x%04x, mode:%d)\n", svid, ops);

	svdm_dfp_inform_attention(pd_port, svid);
	VDM_STATE_DPM_INFORMED(pd_port);
}

/* ---- Unstructured VDM ---- */

void pd_dpm_ufp_recv_cvdm(struct pd_port *pd_port)
{
	struct pd_event *pd_event = pd_get_curr_pd_event(pd_port);
	bool cable = pd_event->pd_msg->frame_type != TCPC_TX_SOP;
	uint16_t svid = dpm_vdm_get_svid(pd_port);
	struct svdm_svid_data *svid_data = cable ?
					   dpm_get_svdm_svid_data_via_cable_svids(pd_port, svid) :
					   dpm_get_svdm_svid_data(pd_port, svid);

	pd_port->cvdm_cable = cable;
	pd_port->cvdm_cnt = pd_get_msg_data_count(pd_port);
	memcpy(pd_port->cvdm_data,
	       pd_get_msg_data_payload(pd_port),
	       pd_get_msg_data_size(pd_port));
	pd_port->cvdm_svid = svid;

	if (svid_data) {
		if (svid_data->ops->ufp_notify_cvdm)
			svid_data->ops->ufp_notify_cvdm(pd_port, svid_data);
		else
			VDM_STATE_DPM_INFORMED(pd_port);

		tcpci_notify_cvdm(pd_port->tcpc, true);
	} else {
		pd_put_dpm_event(pd_port, cable ? PD_DPM_CABLE_NOT_SUPPORT :
						  PD_DPM_NOT_SUPPORT);
		VDM_STATE_DPM_INFORMED(pd_port);
	}
}

void pd_dpm_dfp_send_cvdm(struct pd_port *pd_port)
{
	pd_send_custom_vdm(pd_port, pd_port->cvdm_cable ? TCPC_TX_SOP_PRIME : TCPC_TX_SOP);

	if (pd_port->cvdm_wait_resp)
		VDM_STATE_RESPONSE_CMD(pd_port, PD_TIMER_CVDM_RESPONSE);
}

void pd_dpm_dfp_inform_cvdm(struct pd_port *pd_port, bool ack)
{
	uint16_t svid = dpm_vdm_get_svid(pd_port);
	uint16_t expected_svid = pd_port->cvdm_svid;
	struct svdm_svid_data *svid_data = pd_port->cvdm_cable ?
			dpm_get_svdm_svid_data_via_cable_svids(pd_port, svid) :
			dpm_get_svdm_svid_data(pd_port, svid);
	struct tcpc_device *tcpc = pd_port->tcpc;

	if (svid != expected_svid)
		DPM_INFO("Not expected SVID (0x%04x, 0x%04x)\n",
			 svid, expected_svid);

	if (ack) {
		pd_port->cvdm_cnt = pd_get_msg_data_count(pd_port);
		memcpy(pd_port->cvdm_data,
		       pd_get_msg_data_payload(pd_port),
		       pd_get_msg_data_size(pd_port));
	}

	if (svid_data) {
		if (svid_data->ops->dfp_notify_cvdm)
			svid_data->ops->dfp_notify_cvdm(pd_port, svid_data, ack);
	}

	tcpci_notify_cvdm(tcpc, ack);
	pd_notify_tcp_vdm_event_2nd_result(pd_port,
		ack ? TCP_DPM_RET_VDM_ACK : TCP_DPM_RET_VDM_NAK);
	VDM_STATE_DPM_INFORMED(pd_port);
}

void pd_dpm_ufp_send_svdm_nak(struct pd_port *pd_port)
{
	dpm_vdm_reply_svdm_nak(pd_port);
}

/*
 * DRP : Inform Source/Sink Cap
 */

void pd_dpm_dr_inform_sink_cap(struct pd_port *pd_port)
{
	const uint32_t reaction_clear = DPM_REACTION_GET_SINK_CAP
		| DPM_REACTION_ATTEMPT_GET_FLAG;
	struct pd_event *pd_event = pd_get_curr_pd_event(pd_port);
	struct pd_port_power_caps *snk_cap = &pd_port->pe_data.remote_snk_cap;

	if (!pd_event_data_msg_match(pd_event, PD_DATA_SINK_CAP))
		return;

	snk_cap->nr = pd_get_msg_data_count(pd_port);
	memcpy(snk_cap->pdos,
		pd_get_msg_data_payload(pd_port),
		pd_get_msg_data_size(pd_port));

	pd_dpm_update_pdos_flags(pd_port, snk_cap->pdos[0], false);

	dpm_reaction_clear(pd_port, reaction_clear);
}

void pd_dpm_dr_inform_source_cap(struct pd_port *pd_port)
{
	uint32_t reaction_clear = DPM_REACTION_GET_SOURCE_CAP
		| DPM_REACTION_ATTEMPT_GET_FLAG;
	struct pd_event *pd_event = pd_get_curr_pd_event(pd_port);
	struct pd_port_power_caps *src_cap = &pd_port->pe_data.remote_src_cap;

	if (!pd_event_data_msg_match(pd_event, PD_DATA_SOURCE_CAP))
		return;

	src_cap->nr = pd_get_msg_data_count(pd_port);
	memcpy(src_cap->pdos,
		pd_get_msg_data_payload(pd_port),
		pd_get_msg_data_size(pd_port));

	pd_dpm_update_pdos_flags(pd_port, src_cap->pdos[0], true);

	if (!(pd_port->pe_data.dpm_flags & DPM_FLAGS_PARTNER_DR_POWER))
		reaction_clear |= DPM_REACTION_GET_SINK_CAP;

	dpm_reaction_clear(pd_port, reaction_clear);
}

/*
 * DRP : Data Role Swap
 */

#if CONFIG_USB_PD_DR_SWAP
bool __weak pd_dpm_drs_is_usb_ready(struct pd_port *pd_port, uint8_t role)
{
	return true;
}

void pd_dpm_drs_evaluate_swap(struct pd_port *pd_port, uint8_t role)
{
	if (pd_dpm_drs_is_usb_ready(pd_port, role))
		pd_put_dpm_ack_event(pd_port);
	else
		pd_put_dpm_nak_event(pd_port, PD_DPM_NAK_WAIT);
}

void pd_dpm_drs_change_role(struct pd_port *pd_port, uint8_t role)
{
	uint32_t set = 0, clear = 0;

	pd_port->pe_data.pe_ready = false;

#if CONFIG_USB_PD_REV30
	pd_port->pe_data.pd_traffic_idle = false;
#endif	/* CONFIG_USB_PD_REV30 */

	pd_set_data_role(pd_port, role);

	if (role == PD_ROLE_DFP)
		set |= DPM_REACTION_CAP_RESET_CABLE;

#if CONFIG_USB_PD_DFP_FLOW_DELAY_DRSWAP
	set |= DPM_REACTION_DFP_FLOW_DELAY;
#else
	clear |= DPM_REACTION_DFP_FLOW_DELAY;
#endif	/* CONFIG_USB_PD_DFP_FLOW_DELAY_DRSWAP */

	svdm_reset_state(pd_port);

	if (pd_port->dpm_caps & DPM_CAP_ATTEMPT_ENTER_DP_MODE) {
		if (role == PD_ROLE_DFP) {
			svdm_notify_pe_startup(pd_port);
			set |= DPM_REACTION_DISCOVER_ID;
		} else
			clear |= DPM_REACTION_DISCOVER_ID |
				 DPM_REACTION_DISCOVER_SVIDS;
	}

	if (pd_port->dpm_caps & DPM_CAP_ATTEMPT_DISCOVER_ID_DFP &&
	    role == PD_ROLE_DFP)
		set |= DPM_REACTION_DISCOVER_ID;

	dpm_reaction_set_clear(pd_port, set, clear);

	PE_STATE_DPM_INFORMED(pd_port);
}

#endif	/* CONFIG_USB_PD_DR_SWAP */

/*
 * DRP : Power Role Swap
 */

#if CONFIG_USB_PD_PR_SWAP

/*
 * Rules:
 * External Sources -> EXS
 * Provider/Consumers -> PC
 * Consumers/Provider -> CP
 * 1.  PC (with EXS) shall always deny PR_SWAP from CP (without EXS)
 * 2.  PC (without EXS) shall always acppet PR_SWAP from CP (with EXS)
 * unless the requester isn't able to provide PDOs.
 */

int dpm_check_good_power(struct pd_port *pd_port)
{
	bool local_ex, partner_ex;

	local_ex =
		(pd_port->dpm_caps & DPM_CAP_LOCAL_EXT_POWER) != 0;

	partner_ex =
		(pd_port->pe_data.dpm_flags & DPM_FLAGS_PARTNER_EXTPOWER) != 0;

	if (local_ex != partner_ex) {
		if (partner_ex)
			return GOOD_PW_PARTNER;
		return GOOD_PW_LOCAL;
	}

	if (local_ex)
		return GOOD_PW_BOTH;

	return GOOD_PW_NONE;
}

void pd_dpm_prs_evaluate_swap(struct pd_port *pd_port, uint8_t role)
{
	int good_power;
	bool sink, accept = true;

	bool check_src = (pd_port->dpm_caps & DPM_CAP_PR_SWAP_CHECK_GP_SRC) ?
		true : false;
	bool check_snk = (pd_port->dpm_caps & DPM_CAP_PR_SWAP_CHECK_GP_SNK) ?
		true : false;

#if CONFIG_USB_PD_SRC_REJECT_PR_SWAP_IF_GOOD_PW
	bool check_ext =
		(pd_port->dpm_caps & DPM_CAP_CHECK_EXT_POWER) ? true : false;

	if (check_ext)
		check_src = true;
#endif	/* CONFIG_USB_PD_SRC_REJECT_PR_SWAP_IF_GOOD_PW */

	if (check_src || check_snk) {
		sink = pd_port->power_role == PD_ROLE_SINK;
		good_power = dpm_check_good_power(pd_port);

		switch (good_power) {
		case GOOD_PW_PARTNER:
			if (sink && check_snk)
				accept = false;
			break;

		case GOOD_PW_LOCAL:
			if ((!sink) && (check_src))
				accept = false;
			break;

		default:
			accept = true;
			break;
		}
	}

	dpm_response_request(pd_port, accept);
}

void pd_dpm_prs_turn_off_power_sink(struct pd_port *pd_port)
{
	/* iSnkSwapStdby : 2.5mA */
	tcpci_sink_vbus(pd_port->tcpc,
		TCP_VBUS_CTRL_PR_SWAP, TCPC_VBUS_SINK_0V, 0);
}

void pd_dpm_prs_enable_power_source(struct pd_port *pd_port, bool en)
{
	int vbus_level = en ? TCPC_VBUS_SOURCE_5V : TCPC_VBUS_SOURCE_0V;

	tcpci_source_vbus(pd_port->tcpc,
		TCP_VBUS_CTRL_PR_SWAP, vbus_level, -1);

	if (en)
		pd_enable_vbus_valid_detection(pd_port, true);
	else
		pd_enable_vbus_safe0v_detection(pd_port);
}

void pd_dpm_prs_change_role(struct pd_port *pd_port, uint8_t role)
{
	pd_port->pe_data.pe_ready = false;

#if CONFIG_USB_PD_REV30
	pd_port->pe_data.pd_traffic_idle = false;
#endif	/* CONFIG_USB_PD_REV30 */

	pd_set_power_role(pd_port, role);
	dpm_reaction_clear(pd_port, DPM_REACTION_REQUEST_PR_SWAP);
	pd_put_dpm_ack_event(pd_port);
}

#endif	/* CONFIG_USB_PD_PR_SWAP */

/*
 * DRP : Vconn Swap
 */

#if CONFIG_USB_PD_VCONN_SWAP

void pd_dpm_vcs_evaluate_swap(struct pd_port *pd_port)
{
	bool accept = true;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (!tcpm_inquire_pd_vconn_role(tcpc)) {
		if (tcpc->tcpc_vconn_supply == TCPC_VCONN_SUPPLY_NEVER)
			accept = false;
#if CONFIG_USB_PD_VCONN_SAFE5V_ONLY
		if (pd_port->pe_data.vconn_highv_prot) {
			DPM_DBG("VC_OVER5V\n");
			accept = false;
		}
#endif	/* CONFIG_USB_PD_VCONN_SAFE5V_ONLY */
	}

	dpm_response_request(pd_port, accept);
}

void pd_dpm_vcs_enable_vconn(struct pd_port *pd_port, uint8_t role)
{
	pd_set_vconn(pd_port, role);

	/* If we can't enable vconn immediately,
	 * then after vconn_on,
	 * Vconn Controller should pd_put_dpm_ack_event()
	 */

#if CONFIG_USB_PD_VCONN_READY_TOUT
	if (role != PD_ROLE_VCONN_OFF) {
		pd_enable_timer(pd_port, PD_TIMER_VCONN_READY);
		return;
	}
#endif	/* CONFIG_USB_PD_VCONN_READY_TOUT */

	PE_STATE_DPM_ACK_IMMEDIATELY(pd_port);
}

#endif	/* CONFIG_USB_PD_VCONN_SWAP */

/*
 * PE : PD3.0
 */

#if CONFIG_USB_PD_REV30

#if CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE
void pd_dpm_inform_source_cap_ext(struct pd_port *pd_port)
{
#if DPM_INFO2_ENABLE
	struct pd_source_cap_ext *scedb;
#endif /* DPM_INFO2_ENABLE */
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_SOURCE_CAP_EXT)) {
#if DPM_INFO2_ENABLE
		scedb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("vid=0x%04x, pid=0x%04x\n", scedb->vid, scedb->pid);
		DPM_INFO2("fw_ver=0x%02x, hw_ver=0x%02x\n",
			scedb->fw_ver, scedb->hw_ver);
#endif /* DPM_INFO2_ENABLE */
		dpm_reaction_clear(pd_port,
			DPM_REACTION_GET_SOURCE_CAP_EXT);
	}
}
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_REMOTE */

#if CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL
int pd_dpm_send_source_cap_ext(struct pd_port *pd_port)
{
	return pd_send_sop_ext_msg(pd_port, PD_EXT_SOURCE_CAP_EXT,
		PD_SCEDB_SIZE, &pd_port->src_cap_ext);
}
#endif	/* CONFIG_USB_PD_REV30_SRC_CAP_EXT_LOCAL */

void pd_dpm_inform_sink_cap_ext(struct pd_port *pd_port)
{
#if DPM_INFO2_ENABLE
	struct pd_sink_cap_ext *skedb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_SINK_CAP_EXT)) {
		skedb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("vid=0x%04x, pid=0x%04x\n", skedb->vid, skedb->pid);
		DPM_INFO2("fw_ver=0x%02x, hw_ver=0x%02x\n",
			skedb->fw_ver, skedb->hw_ver);
		DPM_INFO2("skedb_ver=0x%02x\n", skedb->skedb_ver);
		DPM_INFO2("min_pdp=%d, max_pdp=%d\n",
			skedb->min_pdp, skedb->max_pdp);
	}
#endif /* DPM_INFO2_ENABLE */
}

int pd_dpm_send_sink_cap_ext(struct pd_port *pd_port)
{
	return pd_send_sop_ext_msg(pd_port, PD_EXT_SINK_CAP_EXT,
		PD_SKEDB_SIZE, &pd_port->snk_cap_ext);
}

#if CONFIG_USB_PD_REV30_BAT_CAP_LOCAL
static const struct pd_battery_capabilities c_invalid_bcdb = {
	0xffff, 0, PD_BCDB_BAT_CAP_NOT_PRESENT,
	PD_BCDB_BAT_CAP_NOT_PRESENT, PD_BCDB_BAT_TYPE_INVALID
};

int pd_dpm_send_battery_cap(struct pd_port *pd_port)
{
	struct pd_battery_info *bat_info;
	const struct pd_battery_capabilities *bcdb;
	struct pd_get_battery_capabilities *gbcdb =
		pd_get_msg_data_payload(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_INFO2("bat_ref=%d\n", gbcdb->bat_cap_ref);

	bat_info = pd_get_battery_info(pd_port, gbcdb->bat_cap_ref);

	if (bat_info != NULL) {
		tcpci_notify_request_bat_info(
			tcpc, gbcdb->bat_cap_ref);
		bcdb = &bat_info->bat_cap;
	} else
		bcdb = &c_invalid_bcdb;

	return pd_send_sop_ext_msg(pd_port, PD_EXT_BAT_CAP,
		PD_BCDB_SIZE, bcdb);
}
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_LOCAL */

#if CONFIG_USB_PD_REV30_BAT_CAP_REMOTE
void pd_dpm_inform_battery_cap(struct pd_port *pd_port)
{
#if DPM_INFO2_ENABLE
	struct pd_battery_capabilities *bcdb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_BAT_CAP)) {
		bcdb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("vid=0x%04x, pid=0x%04x\n",
			bcdb->vid, bcdb->pid);
	}
#endif /* DPM_INFO2_ENABLE */
}
#endif	/* CONFIG_USB_PD_REV30_BAT_CAP_REMOTE */

#if CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL

static const uint32_t c_invalid_bsdo =
	BSDO(0xffff, BSDO_BAT_INFO_INVALID_REF);

int pd_dpm_send_battery_status(struct pd_port *pd_port)
{
	const uint32_t *bsdo;
	struct pd_battery_info *bat_info;
	struct pd_get_battery_status *gbsdb =
		pd_get_msg_data_payload(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_INFO2("bat_ref=%d\n", gbsdb->bat_status_ref);

	bat_info = pd_get_battery_info(pd_port, gbsdb->bat_status_ref);

	if (bat_info != NULL) {
		tcpci_notify_request_bat_info(
			tcpc, gbsdb->bat_status_ref);
		bsdo = &bat_info->bat_status;
	} else
		bsdo = &c_invalid_bsdo;

	return pd_send_sop_data_msg(pd_port,
		PD_DATA_BAT_STATUS, PD_BSDO_SIZE, bsdo);
}
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_LOCAL */


#if CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE
void pd_dpm_inform_battery_status(struct pd_port *pd_port)
{
#if DPM_INFO2_ENABLE
	uint32_t *payload;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_data_msg_event(pd_port, PD_DATA_BAT_STATUS)) {
		payload = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("0x%08x\n", payload[0]);
	}
#endif /* DPM_INFO2_ENABLE */
}
#endif	/* CONFIG_USB_PD_REV30_BAT_STATUS_REMOTE */

#if CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL
static const struct pd_manufacturer_info c_invalid_mfrs = {
	.vid = 0xFFFF, .pid = 0, .mfrs_string = "Not Supported",
};

int pd_dpm_send_mfrs_info(struct pd_port *pd_port)
{
	uint8_t len = 0;
	struct pd_battery_info *bat_info;
	const struct pd_manufacturer_info *midb = NULL;
	struct pd_get_manufacturer_info *gmidb =
		pd_get_msg_data_payload(pd_port);

	if (gmidb->info_target == PD_GMIDB_TARGET_PORT)
		midb = &pd_port->mfrs_info;

	if (gmidb->info_target == PD_GMIDB_TARGET_BATTRY) {
		bat_info = pd_get_battery_info(pd_port, gmidb->info_ref);
		if (bat_info)
			midb = &bat_info->mfrs_info;
	}

	if (midb == NULL)
		midb = &c_invalid_mfrs;

	len = strnlen((char *)midb->mfrs_string, sizeof(midb->mfrs_string));
	if (len < sizeof(midb->mfrs_string))
		len++;
	return pd_send_sop_ext_msg(pd_port, PD_EXT_MFR_INFO,
		PD_MIDB_MIN_SIZE + len, midb);
}
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_LOCAL */

#if CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE
void pd_dpm_inform_mfrs_info(struct pd_port *pd_port)
{
#if DPM_INFO2_ENABLE
	struct pd_manufacturer_info *midb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_MFR_INFO)) {
		midb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("vid=0x%x, pid=0x%x\n", midb->vid, midb->pid);
	}
#endif /* DPM_INFO2_ENABLE */
}
#endif	/* CONFIG_USB_PD_REV30_MFRS_INFO_REMOTE */


#if CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE
void pd_dpm_inform_country_codes(struct pd_port *pd_port)
{
#if DPM_INFO2_ENABLE
	struct pd_country_codes *ccdb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_COUNTRY_CODES)) {
		ccdb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("len=%d, country_code[0]=0x%04x\n",
			ccdb->length, ccdb->country_code[0]);
	}
#endif /* DPM_INFO2_ENABLE */
}
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_REMOTE */


#if CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL
int pd_dpm_send_country_codes(struct pd_port *pd_port)
{
	uint8_t i;
	struct pd_country_codes ccdb;
	struct pd_country_authority *ca = pd_port->country_info;

	ccdb.length = pd_port->country_nr;

	for (i = 0; i < ccdb.length; i++)
		ccdb.country_code[i] = ca[i].code;

	return pd_send_sop_ext_msg(pd_port, PD_EXT_COUNTRY_CODES,
		2 + ccdb.length*2, &ccdb);
}
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_CODE_LOCAL */

#if CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE
void pd_dpm_inform_country_info(struct pd_port *pd_port)
{
#if DPM_INFO2_ENABLE
	struct pd_country_info *cidb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_COUNTRY_INFO)) {
		cidb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("cc=0x%04x, ci=%d\n",
			cidb->country_code, cidb->country_special_data[0]);
	}
#endif /* DPM_INFO2_ENABLE */
}
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_REMOTE */

#if CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL
int pd_dpm_send_country_info(struct pd_port *pd_port)
{
	uint8_t i, cidb_size;
	struct pd_country_info cidb;
	struct pd_country_authority *ca = pd_port->country_info;
	uint32_t *pccdo = pd_get_msg_data_payload(pd_port);
	uint16_t cc = CCDO_COUNTRY_CODE(*pccdo);

	cidb_size = PD_CIDB_MIN_SIZE;
	cidb.country_code = cc;
	cidb.reserved = 0;
	cidb.country_special_data[0] = 0;

	for (i = 0; i < pd_port->country_nr; i++) {
		if (ca[i].code == cc) {
			cidb_size += ca[i].len;
			memcpy(cidb.country_special_data,
					ca[i].data, ca[i].len);
			break;
		}
	}

	return pd_send_sop_ext_msg(pd_port, PD_EXT_COUNTRY_INFO,
		 cidb_size, &cidb);
}
#endif	/* CONFIG_USB_PD_REV30_COUNTRY_INFO_LOCAL */

#if CONFIG_USB_PD_REV30_ALERT_REMOTE
void pd_dpm_inform_alert(struct pd_port *pd_port)
{
	uint32_t *data = pd_get_msg_data_payload(pd_port);
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	DPM_INFO("inform_alert:0x%08x\n", data[0]);

	pd_port->pe_data.pd_traffic_idle = false;
	pd_port->pe_data.remote_alert = data[0];
	tcpci_notify_alert(pd_port->tcpc, data[0]);
}
#endif	/* CONFIG_USB_PD_REV30_ALERT_REMOTE */

#if CONFIG_USB_PD_REV30_ALERT_LOCAL
int pd_dpm_send_alert(struct pd_port *pd_port)
{
	uint32_t ado = pd_port->pe_data.local_alert;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	pd_port->pe_data.local_alert = 0;
	DPM_INFO("send_alert:0x%08x\n", ado);

	return pd_send_sop_data_msg(pd_port, PD_DATA_ALERT,
		PD_ADO_SIZE, &ado);
}
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */

#if CONFIG_USB_PD_REV30_STATUS_REMOTE
void pd_dpm_inform_status(struct pd_port *pd_port)
{
	struct pd_status *sdb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_STATUS)) {
		sdb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("Temp=%d, IN=0x%x, BAT_IN=0x%x, EVT=0x%x, PTF=0x%x\n",
			sdb->internal_temp, sdb->present_input,
			sdb->present_battery_input, sdb->event_flags,
			PD_STATUS_TEMP_PTF(sdb->temp_status));

		tcpci_notify_status(tcpc, sdb);
	}
}
#endif /* CONFIG_USB_PD_REV30_STATUS_REMOTE */

#if CONFIG_USB_PD_REV30_STATUS_LOCAL
int pd_dpm_send_status(struct pd_port *pd_port)
{
	struct pd_status sdb;
	struct pe_data *pe_data = &pd_port->pe_data;

	memset(&sdb, 0, PD_SDB_SIZE);

	sdb.present_input = pd_port->pd_status_present_in;

#if CONFIG_USB_PD_REV30_BAT_INFO
	if (sdb.present_input &
		PD_STATUS_INPUT_INT_POWER_BAT) {
		sdb.present_battery_input = pd_port->pd_status_bat_in;
	}
#endif	/* CONFIG_USB_PD_REV30_BAT_INFO */

	sdb.event_flags = pe_data->pd_status_event;
	pe_data->pd_status_event &= ~PD_STASUS_EVENT_READ_CLEAR;

#if CONFIG_USB_PD_REV30_STATUS_LOCAL_TEMP
	sdb.internal_temp = pd_port->pd_status_temp;
	sdb.temp_status = pd_port->pd_status_temp_status;
#else
	sdb.internal_temp = 0;
	sdb.temp_status = 0;
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL_TEMP */

	if (sdb.event_flags & PD_STATUS_EVENT_OTP)
		sdb.temp_status = PD_STATUS_TEMP_SET_PTF(PD_PTF_OVER_TEMP);

	if (pd_port->power_role != PD_ROLE_SINK)
		sdb.event_flags &= ~PD_STATUS_EVENT_OVP;

	return pd_send_sop_ext_msg(pd_port, PD_EXT_STATUS,
			PD_SDB_SIZE, &sdb);
}
#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */


#if CONFIG_USB_PD_REV30_PPS_SINK
void pd_dpm_inform_pps_status(struct pd_port *pd_port)
{
#if DPM_INFO2_ENABLE
	struct pd_pps_status_raw *ppssdb;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_ext_msg_event(pd_port, PD_EXT_PPS_STATUS)) {
		ppssdb = pd_get_msg_data_payload(pd_port);
		DPM_INFO2("mv=%d, ma=%d\n",
			PD_PPS_GET_OUTPUT_MV(ppssdb->output_vol_raw),
			PD_PPS_GET_OUTPUT_MA(ppssdb->output_curr_raw));
	}
#endif /* DPM_INFO2_ENABLE */
}
#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

void pd_dpm_inform_not_support(struct pd_port *pd_port)
{
	/* TODO */
}

void pd_dpm_inform_revision(struct pd_port *pd_port)
{
#if DPM_INFO2_ENABLE
	uint32_t *payload;
	uint32_t rmdo;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (dpm_check_data_msg_event(pd_port, PD_DATA_REVISION)) {
		payload = pd_get_msg_data_payload(pd_port);
		rmdo = payload[0];
		DPM_INFO2("Revision=%d.%d, Version=%d.%d\n",
			  RMDO_REV_MAJ(rmdo), RMDO_REV_MIN(rmdo),
			  RMDO_VER_MAJ(rmdo), RMDO_VER_MIN(rmdo));
	}
#endif /* DPM_INFO2_ENABLE */
}

static const uint32_t c_rmdo = RMDO(3, 1, 1, 6);

int pd_dpm_send_revision(struct pd_port *pd_port)
{
	return pd_send_sop_data_msg(pd_port,
		PD_DATA_REVISION, PD_RMDO_SIZE, &c_rmdo);
}
#endif	/* CONFIG_USB_PD_REV30 */

/*
 * PE : Dynamic Control Vconn
 */

void pd_dpm_dynamic_enable_vconn(struct pd_port *pd_port)
{
	struct tcpc_device *tcpc = pd_port->tcpc;

	if (tcpc->tcpc_vconn_supply <= TCPC_VCONN_SUPPLY_ALWAYS)
		return;

	if (pd_port->vconn_role == PD_ROLE_VCONN_DYNAMIC_OFF) {
		DPM_INFO2("DynamicVCEn\n");
		pd_set_vconn(pd_port, PD_ROLE_VCONN_DYNAMIC_ON);
	}
}

void pd_dpm_dynamic_disable_vconn(struct pd_port *pd_port)
{
	bool keep_vconn;
	struct tcpc_device *tcpc = pd_port->tcpc;

	if (!tcpm_inquire_pd_vconn_role(tcpc))
		return;

	switch (tcpc->tcpc_vconn_supply) {
	case TCPC_VCONN_SUPPLY_ALWAYS:
		keep_vconn = true;
		break;
	case TCPC_VCONN_SUPPLY_EMARK_ONLY:
		keep_vconn = !!pd_port->pe_data.cable_discovered_state;
		break;
	default:
		keep_vconn = false;
		break;
	}

	if (keep_vconn)
		return;

	if (tcpc->tcp_event_count)
		return;

	if (pd_port->vconn_role != PD_ROLE_VCONN_DYNAMIC_OFF) {
		DPM_INFO2("DynamicVCDis\n");
		pd_set_vconn(pd_port, PD_ROLE_VCONN_DYNAMIC_OFF);
	}
}

/*
 * PE : Notify DPM
 */

int pd_dpm_notify_pe_startup(struct pd_port *pd_port)
{
	uint32_t reactions = DPM_REACTION_CAP_ALWAYS;

#if CONFIG_USB_PD_DFP_FLOW_DELAY_STARTUP
	reactions |= DPM_REACTION_DFP_FLOW_DELAY;
#endif	/* CONFIG_USB_PD_DFP_FLOW_DELAY_STARTUP */

#if CONFIG_USB_PD_UFP_FLOW_DELAY
	reactions |= DPM_REACTION_UFP_FLOW_DELAY;
#endif	/* CONFIG_USB_PD_UFP_FLOW_DELAY */

#if CONFIG_USB_PD_SRC_TRY_PR_SWAP_IF_BAD_PW
	reactions |= DPM_REACTION_ATTEMPT_GET_FLAG |
		DPM_REACTION_REQUEST_PR_SWAP;
#else
	if (DPM_CAP_EXTRACT_PR_CHECK(pd_port->dpm_caps)) {
		reactions |= DPM_REACTION_REQUEST_PR_SWAP;
		if (DPM_CAP_EXTRACT_PR_CHECK(pd_port->dpm_caps) ==
			DPM_CAP_PR_CHECK_PREFER_SNK)
			reactions |= DPM_REACTION_ATTEMPT_GET_FLAG;
	}

	if (pd_port->dpm_caps & DPM_CAP_CHECK_EXT_POWER)
		reactions |= DPM_REACTION_ATTEMPT_GET_FLAG;
#endif	/* CONFIG_USB_PD_SRC_TRY_PR_SWAP_IF_BAD_PW */

	if (DPM_CAP_EXTRACT_DR_CHECK(pd_port->dpm_caps)) {
		reactions |= DPM_REACTION_REQUEST_DR_SWAP;
		if (DPM_CAP_EXTRACT_DR_CHECK(pd_port->dpm_caps) ==
			DPM_CAP_DR_CHECK_PREFER_UFP)
			reactions |= DPM_REACTION_ATTEMPT_GET_FLAG;
	}

	if (pd_port->dpm_caps & DPM_CAP_ATTEMPT_DISCOVER_CABLE)
		reactions |= DPM_REACTION_DISCOVER_CABLE_FLOW;

	if (pd_port->dpm_caps & DPM_CAP_ATTEMPT_ENTER_DP_MODE &&
	    pd_port->data_role == PD_ROLE_DFP)
		reactions |= DPM_REACTION_DISCOVER_ID;

	if (pd_port->dpm_caps & DPM_CAP_ATTEMPT_DISCOVER_ID)
		reactions |= DPM_REACTION_DISCOVER_ID;
	if (pd_port->dpm_caps & DPM_CAP_ATTEMPT_DISCOVER_ID_DFP &&
	    pd_port->data_role == PD_ROLE_DFP)
		reactions |= DPM_REACTION_DISCOVER_ID;
	if (pd_port->dpm_caps & DPM_CAP_ATTEMPT_DISCOVER_SVIDS)
		reactions |= DPM_REACTION_DISCOVER_SVIDS;

	svdm_reset_state(pd_port);
	svdm_notify_pe_startup(pd_port);
	dpm_reaction_set(pd_port, reactions);
	return 0;

}

int pd_dpm_notify_pe_hardreset(struct pd_port *pd_port)
{
	svdm_reset_state(pd_port);

	if (pd_port->dpm_caps & DPM_CAP_ATTEMPT_ENTER_DP_MODE) {
		if (pd_port->data_role == PD_ROLE_DFP) {
			svdm_notify_pe_startup(pd_port);
			dpm_reaction_set(pd_port, DPM_REACTION_DISCOVER_ID);
		} else
			dpm_reaction_clear(pd_port, DPM_REACTION_DISCOVER_ID |
					   DPM_REACTION_DISCOVER_SVIDS);
	}

	return 0;
}

/*
 * SVDM
 */

static inline bool dpm_register_svdm_ops(struct pd_port *pd_port,
	struct svdm_svid_data *svid_data, const struct svdm_svid_ops *ops)
{
	bool ret = true;
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (ops->parse_svid_data)
		ret = ops->parse_svid_data(pd_port, svid_data);

	if (ret) {
		svid_data->ops = ops;
		svid_data->svid = ops->svid;
		svid_data->cable_svids = ops->cable_svids;
		DPM_DBG("register_svdm: 0x%x\n", ops->svid);
	}

	return ret;
}

struct svdm_svid_data *dpm_get_svdm_svid_data(
		struct pd_port *pd_port, uint16_t svid)
{
	uint8_t i;
	struct svdm_svid_data *svid_data;

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];
		if (svid_data->svid == svid)
			return svid_data;
	}

	return NULL;
}

struct svdm_svid_data *dpm_get_svdm_svid_data_via_cable_svids(
		struct pd_port *pd_port, uint16_t svid)
{
	uint8_t i, j;
	struct svdm_svid_data *svid_data;
	struct svdm_svid_list *cable_svids;

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];
		cable_svids = &svid_data->cable_svids;
		for (j = 0; j < cable_svids->cnt; j++)
			if (cable_svids->svids[j] == svid)
				return svid_data;
	}

	return NULL;
}

bool svdm_reset_state(struct pd_port *pd_port)
{
	int i;
	struct svdm_svid_data *svid_data;

	pd_port->dpm_charging_policy = pd_port->dpm_charging_policy_default;

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];
		if (svid_data->ops && svid_data->ops->reset_state)
			svid_data->ops->reset_state(pd_port, svid_data);
	}

	return true;
}

bool svdm_notify_pe_startup(struct pd_port *pd_port)
{
	int i;
	struct svdm_svid_data *svid_data;

	for (i = 0; i < pd_port->svid_data_cnt; i++) {
		svid_data = &pd_port->svid_data[i];
		if (svid_data->ops && svid_data->ops->notify_pe_startup)
			svid_data->ops->notify_pe_startup(pd_port, svid_data);
	}

	return true;
}

/*
 * dpm_core_init
 */

int pd_dpm_core_init(struct pd_port *pd_port)
{
	int i, j;
	bool ret;
	uint8_t svid_ops_nr = ARRAY_SIZE(svdm_svid_ops);
	struct tcpc_device *tcpc = pd_port->tcpc;

	pd_port->svid_data = devm_kzalloc(&tcpc->dev,
		sizeof(struct svdm_svid_data) * svid_ops_nr, GFP_KERNEL);

	if (!pd_port->svid_data)
		return -ENOMEM;

	for (i = 0, j = 0; i < svid_ops_nr && j < svid_ops_nr; i++) {
		ret = dpm_register_svdm_ops(pd_port,
			&pd_port->svid_data[j], &svdm_svid_ops[i]);

		if (ret)
			j++;
	}

	pd_port->svid_data_cnt = j;

	return 0;
}
