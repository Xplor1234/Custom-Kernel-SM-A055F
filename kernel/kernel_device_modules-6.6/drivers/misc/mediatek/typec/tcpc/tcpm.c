// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "inc/tcpm.h"
#include "inc/tcpci.h"
#include "inc/tcpci_typec.h"

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#include "inc/pd_core.h"
#include "inc/pd_dpm_core.h"
#include "pd_dpm_prv.h"
#include "inc/pd_policy_engine.h"
#include "inc/pd_dpm_pdo_select.h"
#endif	/* CONFIG_USB_POWER_DELIVERY */

#if IS_ENABLED(CONFIG_AW35615_PD)
#include "aw35615/platform_helpers.h"

static struct aw_tcpm_ops_ptr *aw_tcpm_ops = NULL;
static bool  aw35615_probe_flag = false;
void tcpm_set_aw_pps_ops(struct aw_tcpm_ops_ptr *pps_ops)
{
	aw_tcpm_ops = pps_ops;
}
EXPORT_SYMBOL(tcpm_set_aw_pps_ops);

void set_aw35615_probe_flag(bool is_aw35615_probe_ok)
{
	aw35615_probe_flag = is_aw35615_probe_ok;
	printk("aw35615 probe_flag = %d\n", aw35615_probe_flag);
}
EXPORT_SYMBOL(set_aw35615_probe_flag);
#endif

/* Check status */
static int tcpm_check_typec_attached(struct tcpc_device *tcpc)
{
	if (tcpc->typec_attach_old == TYPEC_UNATTACHED ||
		tcpc->typec_attach_new == TYPEC_UNATTACHED)
		return TCPM_ERROR_UNATTACHED;

	return TCPM_SUCCESS;
}

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
int tcpm_check_pd_attached(struct tcpc_device *tcpc)
{
	int ret = TCPM_SUCCESS;
	struct pd_port *pd_port = &tcpc->pd_port;
	struct pe_data *pe_data = &pd_port->pe_data;

	tcpci_lock_typec(tcpc);

	if (!tcpc->pd_inited_flag) {
		ret = TCPM_ERROR_PE_NOT_READY;
		goto unlock_typec_out;
	}

	ret = tcpm_check_typec_attached(tcpc);

unlock_typec_out:
	tcpci_unlock_typec(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	mutex_lock(&pd_port->pd_lock);

	if (!pe_data->pd_prev_connected) {
		ret = TCPM_ERROR_NO_PD_CONNECTED;
		goto unlock_pd_out;
	}

	if (!pe_data->pe_ready)
		ret = TCPM_ERROR_PE_NOT_READY;

unlock_pd_out:
	mutex_unlock(&pd_port->pd_lock);
	return ret;
}
EXPORT_SYMBOL(tcpm_check_pd_attached);
#endif	/* CONFIG_USB_POWER_DELIVERY */


/* Inquire TCPC status */

int tcpm_shutdown(struct tcpc_device *tcpc)
{
	tcpci_lock_typec(tcpc);
#if CONFIG_TCPC_SHUTDOWN_VBUS_DISABLE
	if (tcpc->typec_power_ctrl)
		tcpci_disable_vbus_control(tcpc);
#endif	/* CONFIG_TCPC_SHUTDOWN_VBUS_DISABLE */

	if (tcpc->ops->deinit)
		tcpc->ops->deinit(tcpc);
	tcpc->pd_inited_flag = 0;
	tcpci_unlock_typec(tcpc);

	return 0;
}
EXPORT_SYMBOL(tcpm_shutdown);

int tcpm_suspend(struct tcpc_device *tcpc)
{
	int ret = 0;

	/*
	 * the following 3 if statements are lockless solutions
	 * for preventing some race conditions
	 */
	if (atomic_read(&tcpc->suspend_pending) > 0)
		return -EBUSY;

	if (atomic_read(&tcpc->pending_event) > 0 ||
	    tcpc_get_timer_tick(tcpc))
		return -EBUSY;

	if (atomic_read(&tcpc->suspend_pending) > 0)
		return -EBUSY;

#if CONFIG_TYPEC_SNK_ONLY_WHEN_SUSPEND
	tcpci_lock_typec(tcpc);
	ret = tcpc_typec_suspend(tcpc);
	tcpci_unlock_typec(tcpc);
#endif	/* CONFIG_TYPEC_SNK_ONLY_WHEN_SUSPEND */

	return ret;
}
EXPORT_SYMBOL(tcpm_suspend);

void tcpm_resume(struct tcpc_device *tcpc)
{
#if CONFIG_TYPEC_SNK_ONLY_WHEN_SUSPEND
	tcpci_lock_typec(tcpc);
	tcpc_typec_resume(tcpc);
	tcpci_unlock_typec(tcpc);
#endif	/* CONFIG_TYPEC_SNK_ONLY_WHEN_SUSPEND */
}
EXPORT_SYMBOL(tcpm_resume);

int tcpm_inquire_remote_cc(struct tcpc_device *tcpc,
	uint8_t *cc1, uint8_t *cc2, bool from_ic)
{
	int rv = 0;

	tcpci_lock_typec(tcpc);
	if (from_ic) {
		rv = tcpci_get_cc(tcpc);
		if (rv < 0)
			goto out;
	}

	*cc1 = tcpc->typec_remote_cc[0];
	*cc2 = tcpc->typec_remote_cc[1];
out:
	tcpci_unlock_typec(tcpc);
	return rv;
}
EXPORT_SYMBOL(tcpm_inquire_remote_cc);

int tcpm_inquire_typec_remote_rp_curr(struct tcpc_device *tcpc)
{
	if (tcpm_check_typec_attached(tcpc))
		return 0;

	switch (tcpc->typec_remote_rp_level) {
	case TYPEC_CC_VOLT_SNK_1_5:
		return 1500;
	case TYPEC_CC_VOLT_SNK_3_0:
		return 3000;
	case TYPEC_CC_VOLT_SNK_DFT:
	default:
		return 500;
	}
}
EXPORT_SYMBOL(tcpm_inquire_typec_remote_rp_curr);

int tcpm_inquire_vbus_level(struct tcpc_device *tcpc, bool from_ic)
{
	int rv = 0;

	tcpci_lock_typec(tcpc);
	if (from_ic) {
		rv = tcpci_get_power_status(tcpc);
		if (rv < 0)
			return rv;
	}
	rv = tcpc->vbus_level;
	tcpci_unlock_typec(tcpc);

	return rv;
}
EXPORT_SYMBOL(tcpm_inquire_vbus_level);

int tcpm_inquire_cc_high(struct tcpc_device *tcpc)
{
	int rv = TCPM_ERROR_UNKNOWN;

	tcpci_lock_typec(tcpc);
	if (!tcpc->cc_hidet_en)
		goto out;
	if (tcpc->ops->get_cc_hi)
		rv = tcpc->ops->get_cc_hi(tcpc);
out:
	tcpci_unlock_typec(tcpc);

	return rv;
}
EXPORT_SYMBOL(tcpm_inquire_cc_high);

bool tcpm_inquire_cc_polarity(struct tcpc_device *tcpc)
{
	return tcpc->typec_polarity;
}
EXPORT_SYMBOL(tcpm_inquire_cc_polarity);

uint8_t tcpm_inquire_typec_attach_state(struct tcpc_device *tcpc)
{
	return tcpc->typec_attach_new;
}
EXPORT_SYMBOL(tcpm_inquire_typec_attach_state);

uint8_t tcpm_inquire_typec_role(struct tcpc_device *tcpc)
{
	return tcpc->typec_role;
}
EXPORT_SYMBOL(tcpm_inquire_typec_role);

uint8_t tcpm_inquire_typec_role_def(struct tcpc_device *tcpc)
{
	return tcpc->desc.role_def;
}
EXPORT_SYMBOL(tcpm_inquire_typec_role_def);

bool tcpm_inquire_floating_ground(struct tcpc_device *tcpc)
{
	return !!(tcpc->tcpc_flags & TCPC_FLAGS_FLOATING_GROUND);
}
EXPORT_SYMBOL(tcpm_inquire_floating_ground);

uint8_t tcpm_inquire_typec_local_rp(struct tcpc_device *tcpc)
{
	return tcpc->typec_local_rp_level;
}
EXPORT_SYMBOL(tcpm_inquire_typec_local_rp);

void tcpm_inquire_sink_vbus(struct tcpc_device *tcpc,
			    int *mv, int *ma, uint8_t *type)
{
	*mv = tcpc->sink_vbus_mv;
	*ma = tcpc->sink_vbus_ma;
	*type = tcpc->sink_vbus_type;
}
EXPORT_SYMBOL(tcpm_inquire_sink_vbus);

int tcpm_typec_set_usb_sink_curr(struct tcpc_device *tcpc, int curr)
{
	bool force_sink_vbus = true;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	struct pd_port *pd_port = &tcpc->pd_port;

	if (pd_port->pe_data.pd_prev_connected)
		force_sink_vbus = false;
#endif	/* CONFIG_USB_POWER_DELIVERY */

	tcpci_lock_typec(tcpc);
	tcpc->typec_usb_sink_curr = curr;

	if (tcpc->typec_remote_rp_level != TYPEC_CC_VOLT_SNK_DFT)
		force_sink_vbus = false;

	if (force_sink_vbus) {
		tcpci_sink_vbus(tcpc,
			TCP_VBUS_CTRL_TYPEC, TCPC_VBUS_SINK_5V, -1);
	}
	tcpci_unlock_typec(tcpc);

	return 0;
}
EXPORT_SYMBOL(tcpm_typec_set_usb_sink_curr);

int tcpm_typec_set_rp_level(struct tcpc_device *tcpc, uint8_t level)
{
	int ret = 0;

	tcpci_lock_typec(tcpc);
	ret = tcpc_typec_set_rp_level(tcpc, level);
	tcpci_unlock_typec(tcpc);

	return ret;
}
EXPORT_SYMBOL(tcpm_typec_set_rp_level);

int tcpm_typec_role_swap(struct tcpc_device *tcpc)
{
#if CONFIG_TYPEC_CAP_ROLE_SWAP
	int ret = tcpm_check_typec_attached(tcpc);

	if (ret != TCPM_SUCCESS)
		return ret;

	tcpci_lock_typec(tcpc);
	ret = tcpc_typec_swap_role(tcpc);
	tcpci_unlock_typec(tcpc);

	return ret;
#else
	return TCPM_ERROR_NO_SUPPORT;
#endif /* CONFIG_TYPEC_CAP_ROLE_SWAP */
}
EXPORT_SYMBOL(tcpm_typec_role_swap);

int tcpm_typec_change_role(
	struct tcpc_device *tcpc, uint8_t typec_role)
{
	int ret = 0;

	tcpci_lock_typec(tcpc);
	ret = tcpc_typec_change_role(tcpc, typec_role, false);
	tcpci_unlock_typec(tcpc);

	return ret;
}
EXPORT_SYMBOL(tcpm_typec_change_role);

/* @postpone: whether to postpone Type-C role change until unattached */
int tcpm_typec_change_role_postpone(
	struct tcpc_device *tcpc, uint8_t typec_role, bool postpone)
{
	int ret = 0;

	tcpci_lock_typec(tcpc);
	ret = tcpc_typec_change_role(tcpc, typec_role, postpone);
	tcpci_unlock_typec(tcpc);

	return ret;
}
EXPORT_SYMBOL(tcpm_typec_change_role_postpone);

int tcpm_typec_error_recovery(struct tcpc_device *tcpc)
{
	int ret = 0;

	tcpci_lock_typec(tcpc);
	ret = tcpc_typec_error_recovery(tcpc);
	tcpci_unlock_typec(tcpc);

	return ret;
}
EXPORT_SYMBOL(tcpm_typec_error_recovery);

int tcpm_typec_disable_function(struct tcpc_device *tcpc, bool disable)
{
	int ret = 0;

	tcpci_lock_typec(tcpc);
	ret = (disable ? tcpc_typec_disable : tcpc_typec_enable)(tcpc);
	tcpci_unlock_typec(tcpc);

	return ret;
}
EXPORT_SYMBOL(tcpm_typec_disable_function);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)

bool tcpm_inquire_pd_connected(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;
#if IS_ENABLED(CONFIG_AW35615_PD)
	if (aw35615_probe_flag) {
		int ret;

		ret = aw_tcpm_ops->pd_connected(tcpc);
		if (ret >= 0) {
			if (ret)
				return true;
			else
				return false;
		}
	}
#endif

	return pd_port->pe_data.pd_connected;
}
EXPORT_SYMBOL(tcpm_inquire_pd_connected);

bool tcpm_inquire_pd_prev_connected(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return pd_port->pe_data.pd_prev_connected;
}
EXPORT_SYMBOL(tcpm_inquire_pd_prev_connected);

uint8_t tcpm_inquire_pd_data_role(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return pd_port->data_role;
}
EXPORT_SYMBOL(tcpm_inquire_pd_data_role);

uint8_t tcpm_inquire_pd_power_role(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return pd_port->power_role;
}
EXPORT_SYMBOL(tcpm_inquire_pd_power_role);

uint8_t tcpm_inquire_pd_vconn_role(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;
	uint8_t vconn_role = pd_port->vconn_role;
#if CONFIG_USB_PD_VCONN_SAFE5V_ONLY
	struct pe_data *pe_data = &pd_port->pe_data;

	if (pe_data->vconn_highv_prot)
		vconn_role = pe_data->vconn_highv_prot_role;
#endif	/* CONFIG_USB_PD_VCONN_SAFE5V_ONLY */

	return vconn_role;
}
EXPORT_SYMBOL(tcpm_inquire_pd_vconn_role);

uint8_t tcpm_inquire_pd_pe_ready(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

#if IS_ENABLED(CONFIG_AW35615_PD)
	if (aw35615_probe_flag) {
		int ret;

		if (aw_tcpm_ops != NULL) {
			ret = aw_tcpm_ops->pd_pe_ready(tcpc);
			if (ret >= 0) {
				if (ret)
					return true;
				else
					return false;
			}
		}
	}
#endif

	return pd_port->pe_data.pe_ready;
}
EXPORT_SYMBOL(tcpm_inquire_pd_pe_ready);

uint32_t tcpm_inquire_cable_current(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;
	uint32_t ret = 0;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->pe_data.cable_discovered_state)
		ret = pd_get_cable_current_limit(pd_port);
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}
EXPORT_SYMBOL(tcpm_inquire_cable_current);

uint32_t tcpm_inquire_dpm_flags(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return pd_port->pe_data.dpm_flags;
}
EXPORT_SYMBOL(tcpm_inquire_dpm_flags);

uint32_t tcpm_inquire_dpm_caps(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return pd_port->dpm_caps;
}
EXPORT_SYMBOL(tcpm_inquire_dpm_caps);

void tcpm_set_dpm_caps(struct tcpc_device *tcpc, uint32_t caps)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	mutex_lock(&pd_port->pd_lock);
	pd_port->dpm_caps = caps;
	mutex_unlock(&pd_port->pd_lock);
}
EXPORT_SYMBOL(tcpm_set_dpm_caps);

/* Inquire TCPC to get PD Information */

int tcpm_inquire_pd_contract(struct tcpc_device *tcpc, int *mv, int *ma)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if ((mv == NULL) || (ma == NULL))
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->pe_data.explicit_contract) {
		*mv = pd_port->request_v;
		*ma = pd_port->request_i;
	} else
		ret = TCPM_ERROR_NO_EXPLICIT_CONTRACT;
	mutex_unlock(&pd_port->pd_lock);

	return ret;

}
EXPORT_SYMBOL(tcpm_inquire_pd_contract);

int tcpm_inquire_cable_inform(struct tcpc_device *tcpc, uint32_t *vdos)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (vdos == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->pe_data.cable_discovered_state) {
		memcpy(vdos, pd_port->pe_data.cable_vdos,
			sizeof(uint32_t) * VDO_MAX_NR);
	} else
		ret = TCPM_ERROR_NO_POWER_CABLE;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}
EXPORT_SYMBOL(tcpm_inquire_cable_inform);

int tcpm_inquire_pd_partner_inform(struct tcpc_device *tcpc, uint32_t *vdos)
{
#if CONFIG_USB_PD_KEEP_PARTNER_ID
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (vdos == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->pe_data.partner_id_present) {
		memcpy(vdos, pd_port->pe_data.partner_vdos,
			sizeof(uint32_t) * VDO_MAX_NR);
	} else
		ret = TCPM_ERROR_NO_PARTNER_INFORM;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
#else
	return TCPM_ERROR_NO_SUPPORT;
#endif	/* CONFIG_USB_PD_KEEP_PARTNER_ID */
}
EXPORT_SYMBOL(tcpm_inquire_pd_partner_inform);

int tcpm_inquire_pd_partner_svids(
	struct tcpc_device *tcpc, struct tcpm_svid_list *list)
{
#if CONFIG_USB_PD_KEEP_SVIDS
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;
	struct svdm_svid_list *svdm_list = &pd_port->pe_data.remote_svid_list;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (list == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (svdm_list->cnt) {
		list->cnt = svdm_list->cnt;
		memcpy(list->svids, svdm_list->svids,
			sizeof(uint16_t) * list->cnt);
	} else
		ret = TCPM_ERROR_NO_PARTNER_INFORM;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
#else
	return TCPM_ERROR_NO_SUPPORT;
#endif	/* CONFIG_USB_PD_KEEP_SVIDS */
}
EXPORT_SYMBOL(tcpm_inquire_pd_partner_svids);

int tcpm_inquire_pd_partner_modes(
	struct tcpc_device *tcpc, uint16_t svid, struct tcpm_mode_list *list)
{
	int ret = TCPM_SUCCESS;
	struct svdm_svid_data *svid_data = NULL;
	struct pd_port *pd_port = &tcpc->pd_port;

	mutex_lock(&pd_port->pd_lock);

	svid_data =
		dpm_get_svdm_svid_data(pd_port, USB_SID_DISPLAYPORT);

	if (svid_data == NULL) {
		mutex_unlock(&pd_port->pd_lock);
		return TCPM_ERROR_PARAMETER;
	}

	if (svid_data->remote_mode.mode_cnt) {
		list->cnt = svid_data->remote_mode.mode_cnt;
		memcpy(list->modes, svid_data->remote_mode.mode_vdo,
			sizeof(uint32_t) * list->cnt);
	} else
		ret = TCPM_ERROR_NO_PARTNER_INFORM;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}
EXPORT_SYMBOL(tcpm_inquire_pd_partner_modes);

int tcpm_inquire_pd_source_cap(
	struct tcpc_device *tcpc, struct tcpm_power_cap *cap)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (cap == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->pe_data.remote_src_cap.nr) {
		cap->cnt = pd_port->pe_data.remote_src_cap.nr;
		memcpy(cap->pdos, pd_port->pe_data.remote_src_cap.pdos,
			sizeof(uint32_t) * cap->cnt);
	} else
		ret = TCPM_ERROR_NO_SOURCE_CAP;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}
EXPORT_SYMBOL(tcpm_inquire_pd_source_cap);

int tcpm_inquire_pd_sink_cap(
	struct tcpc_device *tcpc, struct tcpm_power_cap *cap)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (cap == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	if (pd_port->pe_data.remote_snk_cap.nr) {
		cap->cnt = pd_port->pe_data.remote_snk_cap.nr;
		memcpy(cap->pdos, pd_port->pe_data.remote_snk_cap.pdos,
			sizeof(uint32_t) * cap->cnt);
	} else
		ret = TCPM_ERROR_NO_SINK_CAP;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}
EXPORT_SYMBOL(tcpm_inquire_pd_sink_cap);

bool tcpm_extract_power_cap_val(uint32_t pdo, struct tcpm_power_cap_val *cap)
{
	struct dpm_pdo_info_t info;

	dpm_extract_pdo_info(pdo, &info);

	cap->type = info.type;
	cap->min_mv = info.vmin;
	cap->max_mv = info.vmax;

	if (info.type == DPM_PDO_TYPE_BAT)
		cap->uw = info.uw;
	else
		cap->ma = info.ma;

#if CONFIG_USB_PD_REV30
	if (info.type == DPM_PDO_TYPE_APDO) {
		cap->apdo_type = info.apdo_type;
		cap->pwr_limit = info.pwr_limit;
	}
#endif	/* CONFIG_USB_PD_REV30 */

	return cap->type != TCPM_POWER_CAP_VAL_TYPE_UNKNOWN;
}
EXPORT_SYMBOL(tcpm_extract_power_cap_val);

int tcpm_get_remote_power_cap(struct tcpc_device *tcpc,
		struct tcpm_remote_power_cap *remote_cap)
{
	struct pd_port *pd_port = &tcpc->pd_port;
	struct tcpm_power_cap_val cap;
	int i;

#if IS_ENABLED(CONFIG_AW35615_PD)
	if (aw35615_probe_flag) {
		int ret;

		if (aw_tcpm_ops != NULL) {
			ret = aw_tcpm_ops->get_power_cap(tcpc, remote_cap);
			if (ret >= 0) {
				if (ret == 0)
					return TCPM_SUCCESS;
				else
					return TCPM_ERROR_UNKNOWN;
			}
		}
	}
#endif

	mutex_lock(&pd_port->pd_lock);
	remote_cap->selected_cap_idx = pd_port->pe_data.selected_cap;
	remote_cap->nr = pd_port->pe_data.remote_src_cap.nr;
	for (i = 0; i < remote_cap->nr; i++) {
		tcpm_extract_power_cap_val(
			pd_port->pe_data.remote_src_cap.pdos[i], &cap);
		remote_cap->max_mv[i] = cap.max_mv;
		remote_cap->min_mv[i] = cap.min_mv;
		if (cap.type == TCPM_POWER_CAP_VAL_TYPE_BATTERY)
			remote_cap->ma[i] = cap.uw / cap.min_mv;
		else
			remote_cap->ma[i] = cap.ma;
		remote_cap->type[i] = cap.type;
	}
	mutex_unlock(&pd_port->pd_lock);

	return TCPM_SUCCESS;
}
EXPORT_SYMBOL(tcpm_get_remote_power_cap);

static inline int __tcpm_inquire_select_source_cap(
	struct pd_port *pd_port, struct tcpm_power_cap_val *cap_val)
{
	uint8_t sel;
	struct pe_data *pe_data = &pd_port->pe_data;

	if (pd_port->power_role != PD_ROLE_SINK)
		return TCPM_ERROR_POWER_ROLE;

	sel = RDO_POS(pd_port->last_rdo) - 1;
	if (sel >= pe_data->remote_src_cap.nr)
		return TCPM_ERROR_NO_SOURCE_CAP;

	if (!tcpm_extract_power_cap_val(
		pe_data->remote_src_cap.pdos[sel], cap_val))
		return TCPM_ERROR_NOT_FOUND;

	return TCPM_SUCCESS;
}

int tcpm_inquire_select_source_cap(
		struct tcpc_device *tcpc, struct tcpm_power_cap_val *cap_val)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (cap_val == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	ret = __tcpm_inquire_select_source_cap(pd_port, cap_val);
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}
EXPORT_SYMBOL(tcpm_inquire_select_source_cap);

int tcpm_inquire_pd_local_source_cap(
	struct tcpc_device *tcpc, struct tcpm_power_cap *cap)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	if (cap == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	cap->cnt = pd_port->local_src_cap_default.nr;
	memcpy(cap->pdos, pd_port->local_src_cap_default.pdos,
	       sizeof(uint32_t) * cap->cnt);
	mutex_unlock(&pd_port->pd_lock);

	return TCPM_SUCCESS;
}
EXPORT_SYMBOL(tcpm_inquire_pd_local_source_cap);

int tcpm_set_pd_local_source_cap(
	struct tcpc_device *tcpc, struct tcpm_power_cap *cap)
{
	struct pd_port *pd_port = &tcpc->pd_port;
	struct tcpm_power_cap_val cap_val;

	if (cap == NULL)
		return TCPM_ERROR_PARAMETER;
	else if (!tcpm_extract_power_cap_val(cap->pdos[0], &cap_val))
		return TCPM_ERROR_PARAMETER;
	else if (cap->cnt == 0 || cap->cnt > 7 ||
		 cap_val.type != TCPM_POWER_CAP_VAL_TYPE_FIXED ||
		 cap_val.max_mv != 5000 || cap_val.min_mv != 5000)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	pd_port->local_src_cap_default.nr = cap->cnt;
	memcpy(pd_port->local_src_cap_default.pdos, cap->pdos,
	       sizeof(uint32_t) * cap->cnt);
	mutex_unlock(&pd_port->pd_lock);

	return TCPM_SUCCESS;
}
EXPORT_SYMBOL(tcpm_set_pd_local_source_cap);

bool tcpm_inquire_usb_comm(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return !!(pd_port->pe_data.dpm_flags & DPM_FLAGS_PARTNER_USB_COMM);
}
EXPORT_SYMBOL(tcpm_inquire_usb_comm);

bool tcpm_is_src_usb_suspend_support(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return !!(pd_port->pe_data.dpm_flags & DPM_FLAGS_PARTNER_USB_SUSPEND);
}
EXPORT_SYMBOL(tcpm_is_src_usb_suspend_support);
/* Request TCPC to send PD Request */

#if CONFIG_USB_PD_BLOCK_TCPM

#define TCPM_BK_PD_CMD_TOUT	500

/* tPSTransition 550 ms */
#define TCPM_BK_REQUEST_TOUT		1500

/* tPSSourceOff 920 ms,	tPSSourceOn 480 ms*/
#define TCPM_BK_PR_SWAP_TOUT		2500
#define TCPM_BK_HARD_RESET_TOUT	3500

static int tcpm_put_tcp_dpm_event(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event);

static int tcpm_put_tcp_dpm_event_bk(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event,
	uint32_t tout_ms, uint8_t *data, uint8_t size);
#endif	/* CONFIG_USB_PD_BLOCK_TCPM */

static inline int tcpm_put_tcp_dpm_event_cb(struct tcpc_device *tcpc,
	struct tcp_dpm_event *event,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	if (cb_data) {
		event->user_data = cb_data->user_data;
		event->event_cb = cb_data->event_cb;
	}
	return tcpm_put_tcp_dpm_event(tcpc, event);
}

static int tcpm_put_tcp_dpm_event_cbk1(struct tcpc_device *tcpc,
	struct tcp_dpm_event *event,
	const struct tcp_dpm_event_cb_data *cb_data, uint32_t tout_ms)
{
#if CONFIG_USB_PD_BLOCK_TCPM
	if (cb_data == NULL) {
		return tcpm_put_tcp_dpm_event_bk(
			tcpc, event, tout_ms, NULL, 0);
	}
#endif	/* CONFIG_USB_PD_BLOCK_TCPM */

	return tcpm_put_tcp_dpm_event_cb(tcpc, event, cb_data);
}

int tcpm_dpm_pd_power_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_PR_SWAP_AS_SNK + role,
	};

#if IS_ENABLED(CONFIG_AW35615_PD)
	if (aw35615_probe_flag) {
		int ret;

		if (aw_tcpm_ops != NULL) {
			ret = aw_tcpm_ops->request_pr_swap(tcpc, role);
			if (ret >= 0) {
				if (ret == 0)
					return TCPM_SUCCESS;
				else
					return TCPM_ERROR_UNKNOWN;
			}
		}
	}
#endif

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PR_SWAP_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_power_swap);

int tcpm_dpm_pd_data_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DR_SWAP_AS_UFP + role,
	};

#if IS_ENABLED(CONFIG_AW35615_PD)
	if (aw35615_probe_flag) {
		int ret;

		if (aw_tcpm_ops != NULL) {
			ret = aw_tcpm_ops->request_dr_swap(tcpc, role);
			if (ret >= 0) {
				if (ret == 0)
					return TCPM_SUCCESS;
				else
					return TCPM_ERROR_UNKNOWN;
			}
		}
	}
#endif

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_data_swap);

int tcpm_dpm_pd_vconn_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_VCONN_SWAP_OFF + role,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_vconn_swap);

int tcpm_dpm_pd_goto_min(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GOTOMIN,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_goto_min);

int tcpm_dpm_pd_soft_reset(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_SOFTRESET,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_soft_reset);

int tcpm_dpm_pd_get_source_cap(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_SOURCE_CAP,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_source_cap);

int tcpm_dpm_pd_get_sink_cap(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_SINK_CAP,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_sink_cap);

int tcpm_dpm_pd_source_cap(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_SOURCE_CAP,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_source_cap);

int tcpm_dpm_pd_request(struct tcpc_device *tcpc,
	int mv, int ma, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_REQUEST,
		.tcp_dpm_data.pd_req.mv = mv,
		.tcp_dpm_data.pd_req.ma = ma,
	};
#if IS_ENABLED(CONFIG_AW35615_PD)
	if (aw35615_probe_flag) {
		int ret;

		if (aw_tcpm_ops != NULL) {
			if (tcpc->pd_port.dpm_charging_policy == DPM_CHARGING_POLICY_PPS) {
				ret = aw_tcpm_ops->request_apdo(tcpc, (uint16_t)mv, (uint16_t)ma);
				if (ret >= 0) {
					if (ret == 0)
						return TCPM_SUCCESS;
					else
						return TCPM_ERROR_UNKNOWN;
				}
			} else {
				ret = aw_tcpm_ops->request_pdo(tcpc, (uint16_t)mv, (uint16_t)ma);
				if (ret >= 0) {
					if (ret == 0)
						return TCPM_SUCCESS;
					else
						return TCPM_ERROR_UNKNOWN;
				}
			}
		}
	}
#endif

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_request);

int tcpm_dpm_pd_request_ex(struct tcpc_device *tcpc,
	uint8_t pos, int vmin, uint32_t max, uint32_t oper,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_REQUEST_EX,
		.tcp_dpm_data.pd_req_ex.pos = pos,
		.tcp_dpm_data.pd_req_ex.vmin = vmin,
		.tcp_dpm_data.pd_req_ex.max = max,
		.tcp_dpm_data.pd_req_ex.oper = oper,
	};

	if (oper > max)
		return TCPM_ERROR_PARAMETER;

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_request_ex);

int tcpm_dpm_pd_bist_cm2(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_BIST_CM2,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_bist_cm2);

#if CONFIG_USB_PD_REV30

static int tcpm_put_tcp_dpm_event_cbk2(struct tcpc_device *tcpc,
	struct tcp_dpm_event *event,
	const struct tcp_dpm_event_cb_data *cb_data,
	uint32_t tout_ms, uint8_t *data, uint8_t size)
{
#if CONFIG_USB_PD_BLOCK_TCPM
	if (cb_data == NULL) {
		return tcpm_put_tcp_dpm_event_bk(
			tcpc, event, tout_ms, data, size);
	}
#endif	/* CONFIG_USB_PD_BLOCK_TCPM */

	return tcpm_put_tcp_dpm_event_cb(tcpc, event, cb_data);
}

int tcpm_dpm_pd_get_source_cap_ext(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_source_cap_ext *src_cap_ext)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_SOURCE_CAP_EXT,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) src_cap_ext, PD_SCEDB_SIZE);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_source_cap_ext);

int tcpm_dpm_pd_get_sink_cap_ext(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_sink_cap_ext *snk_cap_ext)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_SINK_CAP_EXT,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) snk_cap_ext, PD_SKEDB_SIZE);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_sink_cap_ext);

int tcpm_dpm_pd_fast_swap(struct tcpc_device *tcpc,
	uint8_t role, const struct tcp_dpm_event_cb_data *cb_data)
{
	return TCPM_ERROR_NO_SUPPORT;
}
EXPORT_SYMBOL(tcpm_dpm_pd_fast_swap);

int tcpm_dpm_pd_get_status(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data, struct pd_status *status)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_STATUS,
	};
#if IS_ENABLED(CONFIG_AW35615_PD)
	if (aw35615_probe_flag) {
		int ret;

		if (aw_tcpm_ops != NULL) {
			ret = aw_tcpm_ops->pd_get_status(tcpc, status);
			if (ret >= 0) {
				if (ret == 0)
					return TCPM_SUCCESS;
				else
					return TCPM_ERROR_UNKNOWN;
			}
		}
	}
#endif

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) status, PD_SDB_SIZE);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_status);

int tcpm_dpm_pd_get_pps_status_raw(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_pps_status_raw *pps_status)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_PPS_STATUS,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) pps_status, PD_PPSSDB_SIZE);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_pps_status_raw);

int tcpm_dpm_pd_get_pps_status(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_pps_status *pps_status)
{
	int ret;
	struct pd_pps_status_raw pps_status_raw;

#if IS_ENABLED(CONFIG_AW35615_PD)
	if (aw35615_probe_flag) {
		if (aw_tcpm_ops != NULL) {
			ret = aw_tcpm_ops->get_pps_status(tcpc, pps_status);
			if (ret >= 0) {
				if (ret == 0)
					return TCPM_SUCCESS;
				else
					return TCPM_ERROR_UNKNOWN;
			}
		}
	}
#endif

	ret = tcpm_dpm_pd_get_pps_status_raw(
		tcpc, cb_data, &pps_status_raw);

	if (ret != 0)
		return ret;

	if (pps_status_raw.output_vol_raw == 0xffff)
		pps_status->output_mv = -1;
	else
		pps_status->output_mv =
			PD_PPS_GET_OUTPUT_MV(pps_status_raw.output_vol_raw);

	if (pps_status_raw.output_curr_raw == 0xff)
		pps_status->output_ma = -1;
	else
		pps_status->output_ma =
			PD_PPS_GET_OUTPUT_MA(pps_status_raw.output_curr_raw);

	pps_status->real_time_flags = pps_status_raw.real_time_flags;
	return ret;
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_pps_status);

int tcpm_dpm_pd_get_country_code(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_country_codes *ccdb)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_COUNTRY_CODE,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) ccdb, PD_CCDB_MAX_SIZE);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_country_code);

int tcpm_dpm_pd_get_country_info(struct tcpc_device *tcpc, uint32_t ccdo,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_country_info *cidb)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_COUNTRY_INFO,
		.tcp_dpm_data.index = ccdo,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) cidb, PD_CIDB_MAX_SIZE);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_country_info);

int tcpm_dpm_pd_get_bat_cap(struct tcpc_device *tcpc,
	struct pd_get_battery_capabilities *gbcdb,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_battery_capabilities *bcdb)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_BAT_CAP,
		.tcp_dpm_data.gbcdb = *gbcdb,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) bcdb, PD_BCDB_SIZE);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_bat_cap);

int tcpm_dpm_pd_get_bat_status(struct tcpc_device *tcpc,
	struct pd_get_battery_status *gbsdb,
	const struct tcp_dpm_event_cb_data *cb_data,
	uint32_t *bsdo)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_BAT_STATUS,
		.tcp_dpm_data.gbsdb = *gbsdb,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) bsdo, sizeof(uint32_t) * PD_BSDO_SIZE);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_bat_status);

int tcpm_dpm_pd_get_mfrs_info(struct tcpc_device *tcpc,
	struct pd_get_manufacturer_info *gmidb,
	const struct tcp_dpm_event_cb_data *cb_data,
	struct pd_manufacturer_info *midb)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_MFRS_INFO,
		.tcp_dpm_data.gmidb = *gmidb,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) midb, PD_MIDB_MAX_SIZE);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_mfrs_info);

int tcpm_dpm_pd_get_revision(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data,
	uint32_t *rmdo)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_GET_REVISION,
	};

	return tcpm_put_tcp_dpm_event_cbk2(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT,
		(uint8_t *) rmdo, sizeof(uint32_t) * PD_RMDO_SIZE);
}
EXPORT_SYMBOL(tcpm_dpm_pd_get_revision);

int tcpm_dpm_pd_alert(struct tcpc_device *tcpc,
	uint32_t ado, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_ALERT,
		.tcp_dpm_data.index = ado,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_alert);

#endif	/* CONFIG_USB_PD_REV30 */

int tcpm_dpm_pd_hard_reset(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_HARD_RESET,
	};

	/* check not block case !!! */
	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_HARD_RESET_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_hard_reset);

int tcpm_dpm_pd_error_recovery(struct tcpc_device *tcpc)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_ERROR_RECOVERY,
	};

	if (tcpm_put_tcp_dpm_event(tcpc, &tcp_event) != TCPM_SUCCESS)
		tcpm_typec_error_recovery(tcpc);

	return TCPM_SUCCESS;
}
EXPORT_SYMBOL(tcpm_dpm_pd_error_recovery);

int tcpm_dpm_pd_cable_soft_reset(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_CABLE_SOFTRESET,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_pd_cable_soft_reset);

int tcpm_dpm_vdm_discover_cable_id(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DISCOVER_CABLE_ID,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_vdm_discover_cable_id);

int tcpm_dpm_vdm_discover_id(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DISCOVER_ID,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_vdm_discover_id);

int tcpm_dpm_vdm_discover_svids(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DISCOVER_SVIDS,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_vdm_discover_svids);

int tcpm_dpm_vdm_discover_modes(struct tcpc_device *tcpc,
	uint16_t svid, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DISCOVER_MODES,
		.tcp_dpm_data.svdm_data.svid = svid,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_vdm_discover_modes);

int tcpm_dpm_vdm_enter_mode(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_ENTER_MODE,
		.tcp_dpm_data.svdm_data.svid = svid,
		.tcp_dpm_data.svdm_data.ops = ops,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_vdm_enter_mode);

int tcpm_dpm_vdm_exit_mode(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_EXIT_MODE,
		.tcp_dpm_data.svdm_data.svid = svid,
		.tcp_dpm_data.svdm_data.ops = ops,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_vdm_exit_mode);

int tcpm_dpm_vdm_attention(struct tcpc_device *tcpc,
	uint16_t svid, uint8_t ops,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_ATTENTION,
		.tcp_dpm_data.svdm_data.svid = svid,
		.tcp_dpm_data.svdm_data.ops = ops,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_vdm_attention);

int tcpm_inquire_dp_ufp_u_state(struct tcpc_device *tcpc, uint8_t *state)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (state == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	*state = pd_get_dp_data(pd_port)->ufp_u_state;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}
EXPORT_SYMBOL(tcpm_inquire_dp_ufp_u_state);

int tcpm_dpm_dp_attention(struct tcpc_device *tcpc,
	uint32_t dp_status, uint32_t mask,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DP_ATTENTION,
		.tcp_dpm_data.dp_data.val = dp_status,
		.tcp_dpm_data.dp_data.mask = mask,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_dp_attention);

int tcpm_inquire_dp_dfp_u_state(struct tcpc_device *tcpc, uint8_t *state)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS)
		return ret;

	if (state == NULL)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	*state = pd_get_dp_data(pd_port)->dfp_u_state;
	mutex_unlock(&pd_port->pd_lock);

	return ret;
}
EXPORT_SYMBOL(tcpm_inquire_dp_dfp_u_state);

int tcpm_dpm_dp_status_update(struct tcpc_device *tcpc,
	uint32_t dp_status, uint32_t mask,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DP_STATUS_UPDATE,
		.tcp_dpm_data.dp_data.val = dp_status,
		.tcp_dpm_data.dp_data.mask = mask,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_dp_status_update);

int tcpm_dpm_dp_config(struct tcpc_device *tcpc,
	uint32_t dp_config, uint32_t mask,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DP_CONFIG,
		.tcp_dpm_data.dp_data.val = dp_config,
		.tcp_dpm_data.dp_data.mask = mask,
	};

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_PD_CMD_TOUT);
}
EXPORT_SYMBOL(tcpm_dpm_dp_config);

int tcpm_dpm_send_custom_vdm(
	struct tcpc_device *tcpc,
	struct tcp_dpm_custom_vdm_data *cvdm_data,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_CVDM,
	};

	if (cvdm_data->cnt > PD_DATA_OBJ_SIZE)
		return TCPM_ERROR_PARAMETER;

	memcpy(&tcp_event.tcp_dpm_data.cvdm_data, cvdm_data,
	       sizeof(*cvdm_data));

	ret = tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);

#if CONFIG_USB_PD_TCPM_CB_2ND
	if (cb_data == NULL && ret == TCP_DPM_RET_SUCCESS &&
	    cvdm_data->wait_resp) {
		mutex_lock(&pd_port->pd_lock);
		cvdm_data->cnt = pd_port->cvdm_cnt;
		memcpy(cvdm_data->data, pd_port->cvdm_data,
		       sizeof(cvdm_data->data[0]) * cvdm_data->cnt);
		mutex_unlock(&pd_port->pd_lock);
	}
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	return ret;
}
EXPORT_SYMBOL(tcpm_dpm_send_custom_vdm);

#if CONFIG_USB_PD_TCPM_CB_2ND
static void tcpm_replace_curr_tcp_event(
	struct pd_port *pd_port, struct tcp_dpm_event *event)
{
	int reason = TCP_DPM_RET_DENIED_UNKNOWN;

	switch (event->event_id) {
	case TCP_DPM_EVT_HARD_RESET:
		reason = TCP_DPM_RET_DROP_SENT_HRESET;
		break;
	case TCP_DPM_EVT_ERROR_RECOVERY:
		reason = TCP_DPM_RET_DROP_ERROR_REOCVERY;
		break;
	}

	mutex_lock(&pd_port->pd_lock);
	pd_notify_tcp_event_2nd_result(pd_port, reason);
	pd_port->tcp_event_drop_reset_once = true;
	pd_port->tcp_event_id_2nd = event->event_id;
	memcpy(&pd_port->tcp_event, event, sizeof(struct tcp_dpm_event));
	mutex_unlock(&pd_port->pd_lock);
}
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

static int tcpm_put_tcp_dpm_event(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event)
{
	int ret;
	bool imme = event->event_id >= TCP_DPM_EVT_IMMEDIATELY;
	struct pd_port *pd_port = &tcpc->pd_port;

	ret = tcpm_check_pd_attached(tcpc);
	if (ret != TCPM_SUCCESS &&
		(imme && ret == TCPM_ERROR_UNATTACHED))
		return ret;

	if (imme) {
		ret = pd_put_tcp_pd_event(pd_port, event->event_id,
					  PD_TCP_FROM_TCPM);

#if CONFIG_USB_PD_TCPM_CB_2ND
		if (ret)
			tcpm_replace_curr_tcp_event(pd_port, event);
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */
	} else {
		mutex_lock(&pd_port->pd_lock);
		ret = pd_put_deferred_tcp_event(tcpc, event);
		mutex_unlock(&pd_port->pd_lock);
	}

	if (!ret)
		return TCPM_ERROR_PUT_EVENT;

	return TCPM_SUCCESS;
}

int tcpm_notify_vbus_stable(struct tcpc_device *tcpc)
{
#if CONFIG_USB_PD_VBUS_STABLE_TOUT
	tcpc_disable_timer(tcpc, PD_TIMER_VBUS_STABLE);
#endif	/* CONFIG_USB_PD_VBUS_STABLE_TOUT */

	pd_put_vbus_stable_event(tcpc);
	return TCPM_SUCCESS;
}
EXPORT_SYMBOL(tcpm_notify_vbus_stable);

uint8_t tcpm_inquire_pd_charging_policy(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return pd_port->dpm_charging_policy;
}
EXPORT_SYMBOL(tcpm_inquire_pd_charging_policy);

uint8_t tcpm_inquire_pd_charging_policy_default(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	return pd_port->dpm_charging_policy_default;
}
EXPORT_SYMBOL(tcpm_inquire_pd_charging_policy_default);

int tcpm_reset_pd_charging_policy(struct tcpc_device *tcpc,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	return tcpm_set_pd_charging_policy(
		tcpc, tcpc->pd_port.dpm_charging_policy_default, cb_data);
}
EXPORT_SYMBOL(tcpm_reset_pd_charging_policy);

int tcpm_set_pd_charging_policy_default(
	struct tcpc_device *tcpc, uint8_t policy)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	/* PPS should not be default charging policy ... */
	if ((policy & DPM_CHARGING_POLICY_MASK) >= DPM_CHARGING_POLICY_PPS)
		return TCPM_ERROR_PARAMETER;

	mutex_lock(&pd_port->pd_lock);
	pd_port->dpm_charging_policy_default = policy;
	mutex_unlock(&pd_port->pd_lock);

	return TCPM_SUCCESS;
}
EXPORT_SYMBOL(tcpm_set_pd_charging_policy_default);

int tcpm_set_pd_charging_policy(struct tcpc_device *tcpc,
	uint8_t policy, const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_REQUEST_AGAIN,
	};

	struct pd_port *pd_port = &tcpc->pd_port;
#if IS_ENABLED(CONFIG_AW35615_PD)
	int ret;
#endif
	/* PPS should call another function ... */
	if ((policy & DPM_CHARGING_POLICY_MASK) >= DPM_CHARGING_POLICY_PPS)
		return TCPM_ERROR_PARAMETER;

#if IS_ENABLED(CONFIG_AW35615_PD)
	if (aw35615_probe_flag) {
		pd_port->dpm_charging_policy = policy;

		if (aw_tcpm_ops != NULL) {
			ret = aw_tcpm_ops->request_pdo(tcpc, 5000, 2000);
			if (ret >= 0) {
				if (ret == 0)
					return TCPM_SUCCESS;
				else
					return TCPM_ERROR_UNKNOWN;
			}
		}
	}
#endif
	mutex_lock(&pd_port->pd_lock);
	if (pd_port->dpm_charging_policy == policy) {
		mutex_unlock(&pd_port->pd_lock);
		return TCPM_SUCCESS;
	}

	pd_port->dpm_charging_policy = policy;
	mutex_unlock(&pd_port->pd_lock);

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}
EXPORT_SYMBOL(tcpm_set_pd_charging_policy);

#if CONFIG_USB_PD_DIRECT_CHARGE
int tcpm_set_direct_charge_en(struct tcpc_device *tcpc, bool en)
{
	mutex_lock(&tcpc->access_lock);
	tcpc->pd_during_direct_charge = en;
	mutex_unlock(&tcpc->access_lock);

	return 0;
}
EXPORT_SYMBOL(tcpm_set_direct_charge_en);

bool tcpm_inquire_during_direct_charge(struct tcpc_device *tcpc)
{
	return tcpc->pd_during_direct_charge;
}
EXPORT_SYMBOL(tcpm_inquire_during_direct_charge);
#endif	/* CONFIG_USB_PD_DIRECT_CHARGE */

int tcpm_set_exit_attached_snk_via_cc(struct tcpc_device *tcpc, bool en)
{
	tcpci_lock_typec(tcpc);
	tcpc->pd_exit_attached_snk_via_cc = en;
	tcpci_unlock_typec(tcpc);

	return 0;
}
EXPORT_SYMBOL(tcpm_set_exit_attached_snk_via_cc);

bool tcpm_inquire_exit_attached_snk_via_cc(struct tcpc_device *tcpc)
{
	return tcpc->pd_exit_attached_snk_via_cc;
}
EXPORT_SYMBOL(tcpm_inquire_exit_attached_snk_via_cc);

static int tcpm_put_tcp_dummy_event(struct tcpc_device *tcpc)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_DUMMY,
	};

	return tcpm_put_tcp_dpm_event(tcpc, &tcp_event);
}

int tcpm_dpm_set_vconn_supply_mode(struct tcpc_device *tcpc, uint8_t mode)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	mutex_lock(&pd_port->pd_lock);
	tcpc->tcpc_vconn_supply = mode;
	dpm_reaction_set(pd_port,
		DPM_REACTION_DYNAMIC_VCONN |
		DPM_REACTION_VCONN_STABLE_DELAY);
	mutex_unlock(&pd_port->pd_lock);

	return tcpm_put_tcp_dummy_event(tcpc);
}
EXPORT_SYMBOL(tcpm_dpm_set_vconn_supply_mode);

#if CONFIG_USB_PD_REV30_PPS_SINK

int tcpm_set_apdo_charging_policy(struct tcpc_device *tcpc,
	uint8_t policy, int mv, int ma,
	const struct tcp_dpm_event_cb_data *cb_data)
{
	struct tcp_dpm_event tcp_event = {
		.event_id = TCP_DPM_EVT_REQUEST_AGAIN,
	};

	struct pd_port *pd_port = &tcpc->pd_port;
#if IS_ENABLED(CONFIG_AW35615_PD)
	int ret;
#endif

	/* Not PPS should call another function ... */
	if ((policy & DPM_CHARGING_POLICY_MASK) < DPM_CHARGING_POLICY_PPS)
		return TCPM_ERROR_PARAMETER;

#if IS_ENABLED(CONFIG_AW35615_PD)
	if (aw35615_probe_flag) {
		if (aw_tcpm_ops != NULL) {
			ret = aw_tcpm_ops->request_apdo(tcpc, (uint16_t)mv, (uint16_t)ma);
			if (ret >= 0) {
				pd_port->dpm_charging_policy = policy;
				if (ret == 0)
					return TCPM_SUCCESS;
				else
					return TCPM_ERROR_UNKNOWN;
			}
		}
	}
#endif

	mutex_lock(&pd_port->pd_lock);
	if (!pd_is_source_support_apdo(pd_port)) {
		mutex_unlock(&pd_port->pd_lock);
		return TCPM_ERROR_INVALID_POLICY;
	}

	if (pd_port->dpm_charging_policy == policy) {
		mutex_unlock(&pd_port->pd_lock);
		return tcpm_dpm_pd_request(tcpc, mv, ma, NULL);
	}

	pd_port->dpm_charging_policy = policy;
	pd_port->request_v_apdo = mv;
	pd_port->request_i_apdo = ma;
	mutex_unlock(&pd_port->pd_lock);

	return tcpm_put_tcp_dpm_event_cbk1(
		tcpc, &tcp_event, cb_data, TCPM_BK_REQUEST_TOUT);
}
EXPORT_SYMBOL(tcpm_set_apdo_charging_policy);

int tcpm_inquire_pd_source_apdo(struct tcpc_device *tcpc,
	uint8_t apdo_type, uint8_t *cap_i, struct tcpm_power_cap_val *cap_val)
{
	int ret;
	uint8_t i;
	struct tcpm_power_cap cap;

#if IS_ENABLED(CONFIG_AW35615_PD)
	if (aw35615_probe_flag) {
		if (aw_tcpm_ops != NULL) {
			ret = aw_tcpm_ops->get_source_apdo(tcpc, apdo_type, cap_i, cap_val);
			if (ret >= 0) {
				if (ret == 0)
					return TCPM_SUCCESS;
				else
					return TCPM_ERROR_NOT_FOUND;
			}
		}
	}
#endif

	ret = tcpm_inquire_pd_source_cap(tcpc, &cap);
	if (ret != TCPM_SUCCESS)
		return ret;

	for (i = *cap_i; i < cap.cnt; i++) {
		if (!tcpm_extract_power_cap_val(cap.pdos[i], cap_val))
			continue;

		if (cap_val->type != TCPM_POWER_CAP_VAL_TYPE_AUGMENT)
			continue;

		if ((cap_val->apdo_type & TCPM_APDO_TYPE_MASK) != apdo_type)
			continue;

		*cap_i = i+1;
		return TCPM_SUCCESS;
	}

	return TCPM_ERROR_NOT_FOUND;
}
EXPORT_SYMBOL(tcpm_inquire_pd_source_apdo);

bool tcpm_inquire_during_pps_charge(struct tcpc_device *tcpc)
{
	int ret;
	struct tcpm_power_cap_val cap = {0};

	ret = tcpm_inquire_select_source_cap(tcpc, &cap);
	if (ret != 0)
		return false;

	return (cap.type == TCPM_POWER_CAP_VAL_TYPE_AUGMENT);
}
EXPORT_SYMBOL(tcpm_inquire_during_pps_charge);

#endif	/* CONFIG_USB_PD_REV30_PPS_SINK */

#if CONFIG_USB_PD_REV30_BAT_INFO

static void tcpm_alert_bat_changed(
	struct pd_port *pd_port, enum pd_battery_reference ref)
{
#if CONFIG_USB_PD_REV30_ALERT_LOCAL
	uint8_t fixed = 0, swap = 0;

	if (ref >= PD_BAT_REF_SWAP0)
		swap = 1 << (ref - PD_BAT_REF_SWAP0);
	else
		fixed = 1 << (ref - PD_BAT_REF_FIXED0);

	pd_port->pe_data.local_alert |=
		ADO(ADO_ALERT_BAT_CHANGED, fixed, swap);
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */
}

static inline uint16_t tcpm_calc_battery_cap(
	struct pd_battery_info *battery_info, uint16_t soc)
{
	uint16_t wh;
	struct pd_battery_capabilities *bat_cap;

	bat_cap = &battery_info->bat_cap;

	if (bat_cap->bat_last_full_cap != BSDO_BAT_CAP_UNKNOWN)
		wh = bat_cap->bat_last_full_cap;
	else if (bat_cap->bat_design_cap != BSDO_BAT_CAP_UNKNOWN)
		wh = bat_cap->bat_design_cap;
	else
		wh = BSDO_BAT_CAP_UNKNOWN;

	if (wh != BSDO_BAT_CAP_UNKNOWN)
		wh = wh * soc / 1000;

	return wh;
}

static int tcpm_update_bsdo(
	struct pd_port *pd_port, enum pd_battery_reference ref,
	uint16_t soc, uint16_t wh, uint8_t status)
{
	uint8_t status_old;
	struct pd_battery_info *battery_info;

	battery_info = pd_get_battery_info(pd_port, ref);
	if (battery_info == NULL)
		return TCPM_ERROR_PARAMETER;

	status_old = BSDO_BAT_INFO(battery_info->bat_status);

	if (soc != 0)
		wh = tcpm_calc_battery_cap(battery_info, soc);

	battery_info->bat_status = BSDO(wh, status);

	if (status_old != status) {
		tcpm_alert_bat_changed(pd_port, ref);
		return TCPM_ALERT;
	}

	return TCPM_SUCCESS;
}

static inline int tcpm_update_bat_status_wh_no_mutex(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t wh)
{
	return tcpm_update_bsdo(&tcpc->pd_port, ref, 0, wh, status);
}

int tcpm_update_bat_status_wh(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t wh)
{
	int ret;

	mutex_lock(&tcpc->pd_port.pd_lock);
	ret = tcpm_update_bat_status_wh_no_mutex(tcpc, ref, status, wh);
	mutex_unlock(&tcpc->pd_port.pd_lock);

	if (ret == TCPM_ALERT) {
		ret = TCPM_SUCCESS;
		tcpm_put_tcp_dummy_event(tcpc);
	}

	return ret;
}
EXPORT_SYMBOL(tcpm_update_bat_status_wh);

static inline int tcpm_update_bat_status_soc_no_mutex(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t soc)
{
	return tcpm_update_bsdo(&tcpc->pd_port, ref, soc, 0, status);
}

int tcpm_update_bat_status_soc(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint8_t status, uint16_t soc)
{
	int ret;

	mutex_lock(&tcpc->pd_port.pd_lock);
	ret = tcpm_update_bat_status_soc_no_mutex(tcpc, ref, status, soc);
	mutex_unlock(&tcpc->pd_port.pd_lock);

	if (ret == TCPM_ALERT) {
		ret = TCPM_SUCCESS;
		tcpm_put_tcp_dummy_event(tcpc);
	}

	return ret;
}
EXPORT_SYMBOL(tcpm_update_bat_status_soc);

static inline int tcpm_update_bat_last_full_no_mutex(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint16_t wh)
{
	struct pd_battery_info *battery_info;

	battery_info = pd_get_battery_info(&tcpc->pd_port, ref);
	if (battery_info == NULL)
		return TCPM_ERROR_PARAMETER;

	battery_info->bat_cap.bat_last_full_cap = wh;
	return TCPM_SUCCESS;
}

int tcpm_update_bat_last_full(struct tcpc_device *tcpc,
	enum pd_battery_reference ref, uint16_t wh)
{
	int ret;

	mutex_lock(&tcpc->pd_port.pd_lock);
	ret = tcpm_update_bat_last_full_no_mutex(tcpc, ref, wh);
	mutex_unlock(&tcpc->pd_port.pd_lock);

	return ret;
}
EXPORT_SYMBOL(tcpm_update_bat_last_full);

#endif	/* CONFIG_USB_PD_REV30_BAT_INFO */

#if CONFIG_USB_PD_REV30_STATUS_LOCAL

static void tcpm_alert_status_changed(
	struct tcpc_device *tcpc, uint8_t ado_type)
{
#if CONFIG_USB_PD_REV30_ALERT_LOCAL
	tcpc->pd_port.pe_data.local_alert |= ADO(ado_type, 0, 0);
#endif	/* CONFIG_USB_PD_REV30_ALERT_LOCAL */
}

int tcpm_update_pd_status_temp(struct tcpc_device *tcpc,
	enum pd_present_temperature_flag ptf_new, uint8_t temperature)
{
#if CONFIG_USB_PD_REV30_STATUS_LOCAL_TEMP
	uint8_t ado_type = 0;
	enum pd_present_temperature_flag ptf_now;
	struct pd_port *pd_port = &tcpc->pd_port;

	mutex_lock(&pd_port->pd_lock);

	ptf_now = PD_STATUS_TEMP_PTF(pd_port->pd_status_temp_status);

	if (ptf_now != ptf_new) {
		if ((ptf_new >= PD_PTF_WARNING) ||
			(ptf_now >= PD_PTF_WARNING))
			ado_type |= ADO_ALERT_OPER_CHANGED;

		if (ptf_new == PD_PTF_OVER_TEMP) {
			ado_type |= ADO_ALERT_OTP;
			pd_port->pe_data.pd_status_event |= PD_STATUS_EVENT_OTP;
		}

		pd_port->pd_status_temp_status =
			PD_STATUS_TEMP_SET_PTF(ptf_new);
	}

	pd_port->pd_status_temp = temperature;
	tcpm_alert_status_changed(tcpc, ado_type);
	mutex_unlock(&pd_port->pd_lock);

	if (ado_type)
		tcpm_put_tcp_dummy_event(tcpc);

	return TCPM_SUCCESS;
#else
	return TCPM_ERROR_NO_IMPLEMENT;
#endif /* CONFIG_USB_PD_REV30_STATUS_LOCAL_TEMP */
}
EXPORT_SYMBOL(tcpm_update_pd_status_temp);

int tcpm_update_pd_status_input(
	struct tcpc_device *tcpc, uint8_t input_new, uint8_t mask)
{
	uint8_t input;
	uint8_t ado_type = 0;
	struct pd_port *pd_port = &tcpc->pd_port;

	mutex_lock(&pd_port->pd_lock);
	input = pd_port->pd_status_present_in;
	input &= ~mask;
	input |= (input_new & mask);

	if (input != pd_port->pd_status_present_in) {
		ado_type = ADO_ALERT_SRC_IN_CHANGED;
		pd_port->pd_status_present_in = input;
	}

	tcpm_alert_status_changed(tcpc, ado_type);
	mutex_unlock(&pd_port->pd_lock);

	if (ado_type)
		tcpm_put_tcp_dummy_event(tcpc);

	return TCPM_SUCCESS;
}
EXPORT_SYMBOL(tcpm_update_pd_status_input);

int tcpm_update_pd_status_bat_input(
	struct tcpc_device *tcpc, uint8_t bat_input, uint8_t bat_mask)
{
	uint8_t input;
	uint8_t ado_type = 0;
	const uint8_t flag = PD_STATUS_INPUT_INT_POWER_BAT;
	struct pd_port *pd_port = &tcpc->pd_port;

	mutex_lock(&pd_port->pd_lock);
	input = pd_port->pd_status_bat_in;
	input &= ~bat_mask;
	input |= (bat_input & bat_mask);

	if (input != pd_port->pd_status_bat_in) {
		if (input)
			pd_port->pd_status_present_in |= flag;
		else
			pd_port->pd_status_present_in &= ~flag;

		ado_type = ADO_ALERT_SRC_IN_CHANGED;
	}

	tcpm_alert_status_changed(tcpc, ado_type);
	mutex_unlock(&pd_port->pd_lock);

	if (ado_type)
		tcpm_put_tcp_dummy_event(tcpc);

	return TCPM_SUCCESS;
}
EXPORT_SYMBOL(tcpm_update_pd_status_bat_input);

int tcpm_update_pd_status_event(struct tcpc_device *tcpc, uint8_t evt)
{
	uint8_t ado_type = 0;
	struct pd_port *pd_port = &tcpc->pd_port;

	if (evt & PD_STATUS_EVENT_OCP)
		ado_type |= ADO_ALERT_OCP;

	if (evt & PD_STATUS_EVENT_OTP)
		ado_type |= ADO_ALERT_OTP;

	if (evt & PD_STATUS_EVENT_OVP)
		ado_type |= ADO_ALERT_OVP;

	evt &= PD_STASUS_EVENT_MASK;

	mutex_lock(&pd_port->pd_lock);
	pd_port->pe_data.pd_status_event |= evt;
	tcpm_alert_status_changed(tcpc, ado_type);
	mutex_unlock(&pd_port->pd_lock);

	if (ado_type)
		tcpm_put_tcp_dummy_event(tcpc);

	return TCPM_SUCCESS;
}
EXPORT_SYMBOL(tcpm_update_pd_status_event);

#endif	/* CONFIG_USB_PD_REV30_STATUS_LOCAL */


bool tcpm_is_comm_capable(struct tcpc_device *tcpc)
{
	struct pd_port *pd_port = &tcpc->pd_port;

#if IS_ENABLED(CONFIG_AW35615_PD)
	if (aw35615_probe_flag) {
		int ret;

		if (aw_tcpm_ops != NULL) {
			ret = aw_tcpm_ops->pd_comm_capable(tcpc);
			if (ret >= 0) {
				if (ret)
					return true;
				else
					return false;
			}
		}
	}
#endif

	return pd_port->pe_data.dpm_flags & DPM_FLAGS_PARTNER_USB_COMM;
}
EXPORT_SYMBOL(tcpm_is_comm_capable);

#if CONFIG_USB_PD_BLOCK_TCPM

#if TCPM_DBG_ENABLE
static const char * const bk_event_ret_name[] = {
	"OK",
	"Unknown",	/* or not support by TCPM */
	"NotReady",
	"LocalCap",
	"PartnerCap",
	"SameRole",
	"InvalidReq",
	"RepeatReq",
	"WrongR",
	"PDRev",
	"ModalOperation",
#if CONFIG_USB_PD_VCONN_SAFE5V_ONLY
	"VconnHighVProt",
#endif	/* CONFIG_USB_PD_VCONN_SAFE5V_ONLY */

	"Detach",
	"SReset0",
	"SReset1",
	"HReset0",
	"HReset1",
	"Recovery",
	"BIST",
	"PEBusy",
	"Discard",
	"Unexpected",

	"Wait",
	"Reject",
	"TOUT",
	"NAK",
	"NotSupport",

	"BKTOUT",
	"NoResponse",
};
#endif /* TCPM_DBG_ENABLE */

#if CONFIG_USB_PD_TCPM_CB_2ND
static inline void tcpm_dpm_bk_copy_data(struct pd_port *pd_port)
{
	uint8_t size = pd_port->tcpm_bk_cb_data_max;

	if (size >= pd_get_msg_data_size(pd_port))
		size = pd_get_msg_data_size(pd_port);

	if (pd_port->tcpm_bk_cb_data != NULL) {
		memcpy(pd_port->tcpm_bk_cb_data,
			pd_get_msg_data_payload(pd_port), size);
	}
}
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

int tcpm_dpm_bk_event_cb(
	struct tcpc_device *tcpc, int ret, struct tcp_dpm_event *event)
{
	struct pd_port *pd_port = &tcpc->pd_port;

	if (pd_port->tcpm_bk_event_id != event->event_id) {
		TCPM_DBG("bk_event_cb_dummy: expect:%d real:%d\n",
			pd_port->tcpm_bk_event_id, event->event_id);
		return 0;
	}

	pd_port->tcpm_bk_ret = ret;
	pd_port->tcpm_bk_done = true;

#if CONFIG_USB_PD_TCPM_CB_2ND
	if (ret == TCP_DPM_RET_SUCCESS)
		tcpm_dpm_bk_copy_data(pd_port);
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	wake_up(&pd_port->tcpm_bk_wait_que);
	return 0;
}
EXPORT_SYMBOL(tcpm_dpm_bk_event_cb);

static inline int __tcpm_dpm_wait_bk_event(
	struct pd_port *pd_port, uint32_t tout_ms)
{
	int ret = TCP_DPM_RET_BK_TIMEOUT;

	wait_event_timeout(pd_port->tcpm_bk_wait_que, pd_port->tcpm_bk_done,
			   msecs_to_jiffies(tout_ms));

	if (pd_port->tcpm_bk_done)
		return pd_port->tcpm_bk_ret;
	mutex_lock(&pd_port->pd_lock);

	pd_port->tcpm_bk_event_id = TCP_DPM_EVT_UNKNOWN;

#if CONFIG_USB_PD_TCPM_CB_2ND
	pd_port->tcpm_bk_cb_data = NULL;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	mutex_unlock(&pd_port->pd_lock);

	return ret;
}

static inline int tcpm_dpm_wait_bk_event(
	struct pd_port *pd_port, uint32_t tout_ms)
{
	int ret = __tcpm_dpm_wait_bk_event(pd_port, tout_ms);
#if TCPM_DBG_ENABLE
	struct tcpc_device __maybe_unused *tcpc = pd_port->tcpc;

	if (ret < TCP_DPM_RET_NR && ret >= 0)
		TCPM_DBG("bk_event_cb -> %s\n", bk_event_ret_name[ret]);
#endif /* TCPM_DBG_ENABLE */

	return ret;
}

#if CONFIG_MTK_HANDLE_PPS_TIMEOUT
static void mtk_handle_tcp_event_result(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event, int ret)
{
	struct tcp_dpm_event evt_hreset = {
		.event_id = TCP_DPM_EVT_HARD_RESET,
	};

	if (ret == TCPM_SUCCESS || ret == TCP_DPM_RET_NOT_SUPPORT)
		return;

	if (event->event_id == TCP_DPM_EVT_GET_STATUS
		|| event->event_id == TCP_DPM_EVT_GET_PPS_STATUS) {
		tcpm_put_tcp_dpm_event(tcpc, &evt_hreset);
	}
}
#endif /* CONFIG_MTK_HANDLE_PPS_TIMEOUT */

static int __tcpm_put_tcp_dpm_event_bk(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event,
	uint32_t tout_ms, uint8_t *data, uint8_t size)
{
	int ret;
	struct pd_port *pd_port = &tcpc->pd_port;

	pd_port->tcpm_bk_done = false;
	pd_port->tcpm_bk_event_id = event->event_id;

#if CONFIG_USB_PD_TCPM_CB_2ND
	pd_port->tcpm_bk_cb_data = data;
	pd_port->tcpm_bk_cb_data_max = size;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	ret = tcpm_put_tcp_dpm_event(tcpc, event);
	if (ret != TCPM_SUCCESS)
		return ret;

	return tcpm_dpm_wait_bk_event(pd_port, tout_ms);
}

static int tcpm_put_tcp_dpm_event_bk(
	struct tcpc_device *tcpc, struct tcp_dpm_event *event,
	uint32_t tout_ms, uint8_t *data, uint8_t size)
{
	int ret = TCP_DPM_RET_TIMEOUT, retry = CONFIG_USB_PD_TCPM_CB_RETRY;
	struct pd_port *pd_port = &tcpc->pd_port;

	event->event_cb = tcpm_dpm_bk_event_cb;

	mutex_lock(&pd_port->tcpm_bk_lock);

	do {
		ret = __tcpm_put_tcp_dpm_event_bk(
			tcpc, event, tout_ms, data, size);
	} while ((ret == TCP_DPM_RET_TIMEOUT ||
		  ret == TCP_DPM_RET_DROP_DISCARD ||
		  ret == TCP_DPM_RET_DROP_UNEXPECTED) && (retry-- > 0));

	mutex_unlock(&pd_port->tcpm_bk_lock);

#if !CONFIG_USB_PD_TCPM_CB_2ND
	if ((data != NULL) && (ret == TCPM_SUCCESS))
		return TCPM_ERROR_EXPECT_CB2;
#endif	/* CONFIG_USB_PD_TCPM_CB_2ND */

	if (ret == TCP_DPM_RET_DENIED_REPEAT_REQUEST)
		ret = TCPM_SUCCESS;

#if CONFIG_MTK_HANDLE_PPS_TIMEOUT
	mtk_handle_tcp_event_result(tcpc, event, ret);
#endif /* CONFIG_MTK_HANDLE_PPS_TIMEOUT */

	return ret;
}

#endif	/* CONFIG_USB_PD_BLOCK_TCPM */

#endif /* CONFIG_USB_POWER_DELIVERY */
