// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/sched/clock.h>

#include "inc/tcpci_typec.h"
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#include "inc/tcpci_event.h"
#endif /* CONFIG_USB_POWER_DELIVERY */

/*
 * [BLOCK] TCPCI IRQ Handler
 */

static int tcpci_alert_cc_changed(struct tcpc_device *tcpc)
{
	return tcpc_typec_handle_cc_change(tcpc);
}

static inline int tcpci_alert_vsafe0v(struct tcpc_device *tcpc)
{
	tcpc_typec_handle_vsafe0v(tcpc);
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#if CONFIG_USB_PD_SAFE0V_DELAY
	tcpc_enable_timer(tcpc, PD_TIMER_VSAFE0V_DELAY);
#else
	pd_put_vbus_safe0v_event(tcpc, true);
#endif	/* CONFIG_USB_PD_SAFE0V_DELAY */
#endif	/* CONFIG_USB_POWER_DELIVERY */
	return 0;
}

void tcpci_vbus_level_refresh(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->access_lock);
	tcpc->vbus_level = tcpc->vbus_present ? TCPC_VBUS_VALID :
						TCPC_VBUS_INVALID;
	if (tcpc->vbus_safe0v) {
		if (tcpc->vbus_level == TCPC_VBUS_INVALID)
			tcpc->vbus_level = TCPC_VBUS_SAFE0V;
		else
			TCPC_INFO("ps_confused: %d\n", tcpc->vbus_level);
	}
	mutex_unlock(&tcpc->access_lock);
}

static int tcpci_vbus_level_changed(struct tcpc_device *tcpc)
{
	int rv = 0;

	TCPC_INFO("ps_change=%d\n", tcpc->vbus_level);

	rv = tcpc_typec_handle_ps_change(tcpc, tcpc->vbus_level);
	if (rv < 0)
		return rv;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#if CONFIG_USB_PD_SAFE5V_DELAY
	tcpc_enable_timer(tcpc, PD_TIMER_VSAFE5V_DELAY);
#else
	pd_put_vbus_changed_event(tcpc);
#endif /* CONFIG_USB_PD_SAFE5V_DELAY */
#endif /* CONFIG_USB_POWER_DELIVERY */

	if (tcpc->vbus_level == TCPC_VBUS_SAFE0V)
		rv = tcpci_alert_vsafe0v(tcpc);

	return rv;
}

int tcpci_alert_power_status_changed(struct tcpc_device *tcpc)
{
	int rv = 0;

	rv = tcpci_get_power_status(tcpc);
	if (rv < 0)
		return rv;

	if (tcpc->tcpc_flags & TCPC_FLAGS_ALERT_V10)
		rv = tcpci_vbus_level_changed(tcpc);

	return rv;
}
EXPORT_SYMBOL(tcpci_alert_power_status_changed);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
static int tcpci_alert_tx_success(struct tcpc_device *tcpc)
{
	uint8_t tx_state = PD_TX_STATE_GOOD_CRC;
	struct pd_event evt = {
		.event_type = PD_EVT_CTRL_MSG,
		.msg = PD_CTRL_GOOD_CRC,
		.pd_msg = NULL,
	};

	mutex_lock(&tcpc->access_lock);
	tcpc->io_time_diff = (local_clock() - tcpc->io_time_start) /
			      NSEC_PER_USEC;
	TCPC_DBG("%s io_time_diff = %lluus\n", __func__, tcpc->io_time_diff);
	tx_state = tcpc->pd_transmit_state;
	tcpc->pd_transmit_state = PD_TX_STATE_GOOD_CRC;
	mutex_unlock(&tcpc->access_lock);

	if (tx_state == PD_TX_STATE_WAIT_CRC_VDM)
		pd_put_vdm_event(tcpc, &evt, false);
	else
		pd_put_event(tcpc, &evt, false);

	return 0;
}

static int tcpci_alert_tx_failed(struct tcpc_device *tcpc)
{
	uint8_t tx_state = PD_TX_STATE_GOOD_CRC;

	mutex_lock(&tcpc->access_lock);
	tcpc->io_time_diff = 0;
	tx_state = tcpc->pd_transmit_state;
	tcpc->pd_transmit_state = PD_TX_STATE_NO_GOOD_CRC;
	mutex_unlock(&tcpc->access_lock);

	if (tx_state == PD_TX_STATE_WAIT_CRC_VDM)
		vdm_put_hw_event(tcpc, PD_HW_TX_FAILED);
	else
		pd_put_hw_event(tcpc, PD_HW_TX_FAILED);

	return 0;
}

static int tcpci_alert_tx_discard(struct tcpc_device *tcpc)
{
	uint8_t tx_state = PD_TX_STATE_GOOD_CRC;
	bool retry_crc_discard =
		!!(tcpc->tcpc_flags & TCPC_FLAGS_RETRY_CRC_DISCARD);

	TCPC_INFO("Discard\n");

	mutex_lock(&tcpc->access_lock);
	tcpc->io_time_diff = 0;
	tx_state = tcpc->pd_transmit_state;
	tcpc->pd_transmit_state = PD_TX_STATE_DISCARD;
	mutex_unlock(&tcpc->access_lock);

	if (tx_state == PD_TX_STATE_WAIT_CRC_VDM)
		vdm_put_hw_event(tcpc, PD_HW_TX_DISCARD);
	else {
		if (retry_crc_discard) {
#if CONFIG_USB_PD_RETRY_CRC_DISCARD
			mutex_lock(&tcpc->access_lock);
			tcpc->pd_discard_pending = true;
			mutex_unlock(&tcpc->access_lock);
			tcpc_enable_timer(tcpc, PD_TIMER_DISCARD);
#else
			TCPC_ERR("RETRY_CRC_DISCARD\n");
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
		} else {
			pd_put_hw_event(tcpc, PD_HW_TX_FAILED);
		}
	}

	return 0;
}

static void tcpci_alert_recv_msg(struct tcpc_device *tcpc)
{
	int rv = 0;
	struct pd_msg *pd_msg = NULL;

	tcpc->curr_pd_msg = NULL;

	pd_msg = pd_alloc_msg(tcpc);
	if (pd_msg == NULL)
		return;

	rv = tcpci_get_message(tcpc, pd_msg->payload, &pd_msg->msg_hdr,
			       &pd_msg->frame_type);
	if (rv < 0 || pd_msg->msg_hdr == 0) {
		TCPC_INFO("recv_msg failed: %d\n", rv);
		pd_free_msg(tcpc, pd_msg);
		return;
	}
	tcpc->curr_pd_msg = pd_msg;
}

static int tcpci_alert_recv_msg_put_event(struct tcpc_device *tcpc)
{
	if (tcpc->curr_pd_msg)
		pd_put_pd_msg_event(tcpc, tcpc->curr_pd_msg);
	return 0;
}

static int tcpci_alert_rx_overflow(struct tcpc_device *tcpc)
{
	TCPC_INFO("RX_OVERFLOW\n");
	return 0;
}

static int tcpci_alert_recv_hard_reset(struct tcpc_device *tcpc)
{
	cancel_delayed_work_sync(&tcpc->tx_pending_work);
	atomic_set(&tcpc->tx_pending, 0);
	wake_up(&tcpc->tx_wait_que);
	TCPC_INFO("HardResetAlert\n");
	pd_put_recv_hard_reset_event(tcpc);
	return tcpci_init_alert_mask(tcpc);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

static int tcpci_alert_vendor_defined(struct tcpc_device *tcpc)
{
	return tcpci_alert_vendor_defined_handler(tcpc);
}

static int tcpci_alert_fault(struct tcpc_device *tcpc)
{
	uint8_t fault_status = 0;

	tcpci_get_fault_status(tcpc, &fault_status);
	TCPC_INFO("FaultAlert=0x%02x\n", fault_status);
	tcpci_fault_status_clear(tcpc, fault_status);
	return 0;
}

int tcpci_alert_wakeup(struct tcpc_device *tcpc)
{
	TCPC_INFO("Wakeup\n");
	if (tcpc->typec_lpm) {
		if (tcpc->ops->set_low_power_mode)
			tcpc->ops->set_low_power_mode(tcpc, false,
						      TYPEC_CC_OPEN);
		tcpc_enable_lpm_timer(tcpc, true);
	}

	return 0;
}
EXPORT_SYMBOL(tcpci_alert_wakeup);

struct tcpci_alert_handler {
	uint32_t bit_mask;
	int (*handler)(struct tcpc_device *tcpc);
};

#define DECL_TCPCI_ALERT_HANDLER(xbit, xhandler) {\
	.bit_mask = 1 << xbit,\
	.handler = xhandler,\
}

static const struct tcpci_alert_handler tcpci_alert_handlers[] = {
	DECL_TCPCI_ALERT_HANDLER(15, tcpci_alert_vendor_defined),
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	DECL_TCPCI_ALERT_HANDLER(4, tcpci_alert_tx_failed),
	DECL_TCPCI_ALERT_HANDLER(5, tcpci_alert_tx_discard),
	DECL_TCPCI_ALERT_HANDLER(6, tcpci_alert_tx_success),
	DECL_TCPCI_ALERT_HANDLER(2, tcpci_alert_recv_msg_put_event),
	DECL_TCPCI_ALERT_HANDLER(3, tcpci_alert_recv_hard_reset),
	DECL_TCPCI_ALERT_HANDLER(10, tcpci_alert_rx_overflow),
#endif /* CONFIG_USB_POWER_DELIVERY */

	DECL_TCPCI_ALERT_HANDLER(9, tcpci_alert_fault),
	DECL_TCPCI_ALERT_HANDLER(0, tcpci_alert_cc_changed),
	DECL_TCPCI_ALERT_HANDLER(1, tcpci_alert_power_status_changed),

	DECL_TCPCI_ALERT_HANDLER(16, tcpci_alert_wakeup),
};

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
static inline bool tcpci_check_hard_reset_complete(
	struct tcpc_device *tcpc, uint32_t *alert_status)
{
	if ((*alert_status & TCPC_REG_ALERT_HRESET_SUCCESS)
			== TCPC_REG_ALERT_HRESET_SUCCESS) {
		*alert_status &= ~TCPC_REG_ALERT_HRESET_SUCCESS;
		pd_put_sent_hard_reset_event(tcpc);
		return true;
	}

	if (*alert_status & TCPC_REG_ALERT_TX_DISCARDED) {
		*alert_status &= ~TCPC_REG_ALERT_TX_DISCARDED;
		TCPC_INFO("HResetFailed\n");
		pd_put_hw_event(tcpc, PD_HW_TX_FAILED);
		return true;
	}

	return false;
}
#endif	/* CONFIG_USB_POWER_DELIVERY */

int tcpci_alert(struct tcpc_device *tcpc, bool masked)
{
	int rv = 0, i = 0;
	uint32_t alert_status = 0, alert_mask = 0;
	const uint8_t typec_role = tcpc->typec_role,
		      vbus_level = tcpc->vbus_level;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	mutex_lock(&tcpc->access_lock);
	tcpc->io_time_start = local_clock();
	mutex_unlock(&tcpc->access_lock);
#endif	/* CONFIG_USB_POWER_DELIVERY */

	//rv = tcpci_get_alert_status_and_mask(tcpc, &alert_status, &alert_mask);
	rv = tcpci_get_alert_status(tcpc, &alert_status);
	rv = tcpci_get_alert_mask(tcpc, &alert_mask);
	if (rv < 0)
		return rv;

	TCPC_INFO("Alert:0x%04x, Mask:0x%04x\n", alert_status, alert_mask);

	alert_status &= alert_mask;

	if (typec_role == TYPEC_ROLE_UNKNOWN ||
		typec_role >= TYPEC_ROLE_NR) {
		TYPEC_INFO("Wrong TypeC-Role: %d\n", typec_role);
		return tcpci_alert_status_clear(tcpc, alert_status);
	}

	/* mask all alert */
	if (masked) {
		rv = tcpci_set_alert_mask(tcpc, 0);
		if (rv < 0)
			return tcpci_alert_status_clear(tcpc, alert_status);
	}

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	if (alert_status & TCPC_REG_ALERT_RX_STATUS) {
		mutex_lock(&tcpc->rxbuf_lock);
		tcpci_alert_recv_msg(tcpc);
	}
#endif	/* CONFIG_USB_POWER_DELIVERY */

	tcpci_alert_status_clear(tcpc, alert_status);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	if (alert_status & TCPC_REG_ALERT_RX_STATUS) {
		mutex_unlock(&tcpc->rxbuf_lock);
		tcpc_event_thread_wake_up(tcpc);
	}

	if (tcpc->pd_transmit_state == PD_TX_STATE_WAIT_HARD_RESET) {
		if (tcpci_check_hard_reset_complete(tcpc, &alert_status))
			atomic_dec_if_positive(&tcpc->tx_pending);
	}
	i = __builtin_popcount(alert_status & TCPC_REG_ALERT_TX_MASK);
	while (i-- > 0)
		atomic_dec_if_positive(&tcpc->tx_pending);
	TCPC_DBG("%s tx_pending = %d\n", __func__,
					 atomic_read(&tcpc->tx_pending));
	if (!atomic_read(&tcpc->tx_pending))
		wake_up(&tcpc->tx_wait_que);
#endif	/* CONFIG_USB_POWER_DELIVERY */

	if ((tcpc->tcpc_flags & TCPC_FLAGS_ALERT_V10) &&
	    (alert_status & TCPC_REG_ALERT_EXT_VBUS_80))
		alert_status |= TCPC_REG_ALERT_POWER_STATUS;

	for (i = 0; i < ARRAY_SIZE(tcpci_alert_handlers); i++)
		if (tcpci_alert_handlers[i].bit_mask & alert_status)
			tcpci_alert_handlers[i].handler(tcpc);

	if (masked && !(alert_status & TCPC_REG_ALERT_RX_HARD_RST))
		rv = tcpci_set_alert_mask(tcpc, alert_mask);

	if (tcpc->tcpc_flags & TCPC_FLAGS_ALERT_V10)
		return rv;

	tcpci_vbus_level_refresh(tcpc);
	if (vbus_level != tcpc->vbus_level)
		rv = tcpci_vbus_level_changed(tcpc);

	return rv;
}
EXPORT_SYMBOL(tcpci_alert);

/*
 * [BLOCK] TYPEC device changed
 */

static inline int tcpci_set_wake_lock(struct tcpc_device *tcpc, bool pd_lock)
{
	if (!!pd_lock != !!tcpc->wake_lock_pd) {
		if (pd_lock) {
			TCPC_DBG("wake_lock=1\n");
			__pm_wakeup_event(tcpc->attach_wake_lock,
					  CONFIG_TCPC_ATTACH_WAKE_LOCK_TOUT);
		} else {
			TCPC_DBG("wake_lock=0\n");
			__pm_relax(tcpc->attach_wake_lock);
		}
		return 1;
	}

	return 0;
}

static int tcpci_set_wake_lock_pd(struct tcpc_device *tcpc, bool pd_lock)
{
	uint8_t wake_lock_pd = 0;

	mutex_lock(&tcpc->access_lock);

	wake_lock_pd = tcpc->wake_lock_pd;
	if (pd_lock)
		wake_lock_pd++;
	else if (wake_lock_pd > 0)
		wake_lock_pd--;

	if (wake_lock_pd == 0)
		__pm_wakeup_event(tcpc->detach_wake_lock, 1000);

	tcpci_set_wake_lock(tcpc, wake_lock_pd);
	tcpc->wake_lock_pd = wake_lock_pd;

	if (wake_lock_pd == 1)
		__pm_relax(tcpc->detach_wake_lock);

	mutex_unlock(&tcpc->access_lock);

	return 0;
}

int tcpci_report_usb_port_attached(struct tcpc_device *tcpc)
{
	TCPC_INFO("usb_port_attached\n");

	tcpci_set_wake_lock_pd(tcpc, true);

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
#if CONFIG_USB_PD_DISABLE_PE
	if (tcpc->disable_pe)
		return 0;
#endif	/* CONFIG_USB_PD_DISABLE_PE */

	if (tcpc->pd_inited_flag)
		pd_put_cc_attached_event(tcpc, tcpc->typec_attach_new);
#endif /* CONFIG_USB_POWER_DLEIVERY */

	return 0;
}

int tcpci_report_usb_port_detached(struct tcpc_device *tcpc)
{
	TCPC_INFO("usb_port_detached\n");

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	if (tcpc->pd_inited_flag)
		pd_put_cc_detached_event(tcpc);
	else {
		pd_event_buf_reset(tcpc);
		tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_PE_IDLE);
	}

	cancel_delayed_work_sync(&tcpc->tx_pending_work);
	atomic_set(&tcpc->tx_pending, 0);
#endif /* CONFIG_USB_POWER_DELIVERY */

	tcpci_set_wake_lock_pd(tcpc, false);

	return 0;
}

int tcpci_report_usb_port_changed(struct tcpc_device *tcpc)
{
	tcpci_notify_typec_state(tcpc);

	if (tcpc->typec_attach_new == TYPEC_UNATTACHED ||
	    tcpc->typec_attach_new == TYPEC_PROTECTION)
		tcpci_report_usb_port_detached(tcpc);
	else if (tcpc->typec_attach_old == TYPEC_UNATTACHED)
		tcpci_report_usb_port_attached(tcpc);
	else
		TCPC_DBG2("TCPC Attach Again\n");

	return 0;
}

/*
 * [BLOCK] TYPEC power control changed
 */

static inline int tcpci_report_power_control_on(struct tcpc_device *tcpc)
{
	tcpci_set_wake_lock_pd(tcpc, true);

	mutex_lock(&tcpc->access_lock);
	tcpc_disable_timer(tcpc, TYPEC_RT_TIMER_DISCHARGE);
	tcpci_enable_auto_discharge(tcpc, true);
	tcpci_enable_force_discharge(tcpc, false, 0);
	mutex_unlock(&tcpc->access_lock);

	return 0;
}

static inline int tcpci_report_power_control_off(struct tcpc_device *tcpc)
{
	mutex_lock(&tcpc->access_lock);
	tcpci_enable_force_discharge(tcpc, true, 0);
	tcpc_enable_timer(tcpc, TYPEC_RT_TIMER_DISCHARGE);
	mutex_unlock(&tcpc->access_lock);

	tcpci_set_wake_lock_pd(tcpc, false);

	return 0;
}

int tcpci_report_power_control(struct tcpc_device *tcpc, bool en)
{
	if (tcpc->typec_power_ctrl == en)
		return 0;

	tcpc->typec_power_ctrl = en;

	if (en)
		tcpci_report_power_control_on(tcpc);
	else
		tcpci_report_power_control_off(tcpc);

	return 0;
}
