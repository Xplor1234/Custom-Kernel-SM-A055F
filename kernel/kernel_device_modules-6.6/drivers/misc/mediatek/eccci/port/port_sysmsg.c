// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/kernel.h>

#include "ccci_auxadc.h"

#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_core.h"
#include "ccci_bm.h"
#include "port_sysmsg.h"
#include "ccci_swtp.h"
#include "fsm/ccci_fsm_internal.h"

#if defined(CONFIG_WT_PROJECT_S96901AA1) || defined(CONFIG_WT_PROJECT_S96901WA1)
#include <linux/hardware_info.h>
extern char Lcm_name[HARDWARE_MAX_ITEM_LONGTH];
#endif

#define MAX_QUEUE_LENGTH 16

struct md_rf_notify_struct {
	unsigned int bit;
	void (*notify_func)(unsigned int para0, unsigned int para1);
	const char *module_name;
};

#define MD_RF_NOTIFY(bit, func, name) \
extern void func(unsigned int para0, unsigned int para1);

#include "mdrf_notify_list.h"

#undef MD_RF_NOTIFY
#define MD_RF_NOTIFY(bit, func, name) \
	{bit, func, name},

static struct md_rf_notify_struct notify_members[] = {
	#include "mdrf_notify_list.h"
};

#define NOTIFY_LIST_ITM_NUM  ARRAY_SIZE(notify_members)

static void sys_msg_MD_RF_Notify(unsigned int bit_value, unsigned int data_1)
{
	int i;
	unsigned int data_send;

#if defined(CONFIG_WT_PROJECT_S96901AA1) || defined(CONFIG_WT_PROJECT_S96901WA1)
	pr_info("bit_value before select: %X,data_1:%X :\n",bit_value,data_1);

	if (strstr(Lcm_name, "ft8722_fhdp_wt_dsi_vdo_cphy_90hz_txd")) {
		bit_value = bit_value & 1;
		pr_info("ft8722_fhdp_wt_dsi_vdo_cphy_90hz_txd bit_value: %X,data_1:%X :\n",bit_value,data_1);
	} else if (strstr(Lcm_name, "ili7807s_fhdp_wt_dsi_vdo_cphy_90hz_tianma")) {
		bit_value = bit_value & 4;
		pr_info("ili7807s_fhdp_wt_dsi_vdo_cphy_90hz_tianma bit_value: %X,data_1:%X :\n",bit_value,data_1);
	} else if (strstr(Lcm_name, "hx83112f_fhdp_wt_dsi_vdo_cphy_90hz_dsbj")) {
		bit_value = bit_value & 8;
		pr_info("hx83112f_fhdp_wt_dsi_vdo_cphy_90hz_dsbj bit_value: %X,data_1:%X :\n",bit_value,data_1);
	} else if (strstr(Lcm_name, "hx83112f_fhdp_wt_dsi_vdo_cphy_90hz_djn")) {
		bit_value = bit_value & 16;
		pr_info("hx83112f_fhdp_wt_dsi_vdo_cphy_90hz_djn bit_value: %X,data_1:%X :\n",bit_value,data_1);
	} else {
		bit_value = bit_value & 32;
		pr_info("ili7807s_fhdp_wt_dsi_vdo_cphy_90hz_chuangwei bit_value: %X,data_1:%X :\n",bit_value,data_1);
	}

	if (bit_value>= 1) {
		bit_value = 1;
	} else {
		bit_value = 0;
	}

	pr_info("bit_value after select: %X,data_1:%X :\n",bit_value,data_1);
#endif

	for (i = 0; i < NOTIFY_LIST_ITM_NUM; i++) {
		data_send = (bit_value&(1<<notify_members[i].bit))
			>> notify_members[i].bit;

		CCCI_NORMAL_LOG(0, SYS, "0x121: notify to (%s)\n",
			notify_members[i].module_name);

		notify_members[i].notify_func(data_send, data_1);
	}
}

#ifndef TEST_MESSAGE_FOR_BRINGUP
#define TEST_MESSAGE_FOR_BRINGUP
#endif

#ifdef TEST_MESSAGE_FOR_BRINGUP
static inline int port_sys_notify_md(struct port_t *port, unsigned int msg,
	unsigned int data)
{
	return port_send_msg_to_md(port, msg, data, 1);
}

static inline int port_sys_echo_test(struct port_t *port, int data)
{
	CCCI_DEBUG_LOG(0, SYS,
		"system message: Enter ccci_sysmsg_echo_test data= %08x",
		data);
	port_sys_notify_md(port, TEST_MSG_ID_AP2MD, data);
	return 0;
}

static inline int port_sys_echo_test_l1core(struct port_t *port, int data)
{
	CCCI_DEBUG_LOG(0, SYS,
		"system message: Enter ccci_sysmsg_echo_test_l1core data= %08x",
		data);
	port_sys_notify_md(port, TEST_MSG_ID_L1CORE_AP2MD, data);
	return 0;
}
#endif
/* for backward compatibility */
struct ccci_sys_cb_func_info ccci_sys_cb_table_100[MAX_KERN_API];
struct ccci_sys_cb_func_info ccci_sys_cb_table_1000[MAX_KERN_API];

int register_ccci_sys_call_back(unsigned int id, ccci_sys_cb_func_t func)
{
	int ret = 0;
	struct ccci_sys_cb_func_info *info_ptr = NULL;

	if ((id >= 0x100) && ((id - 0x100) < MAX_KERN_API)) {
		info_ptr = &(ccci_sys_cb_table_100[id - 0x100]);
	} else if ((id >= 0x1000) && ((id - 0x1000) < MAX_KERN_API)) {
		info_ptr = &(ccci_sys_cb_table_1000[id - 0x1000]);
	} else {
		CCCI_ERROR_LOG(0, SYS,
			"register_sys_call_back fail: invalid func id(0x%x)\n",
			id);
		return -EINVAL;
	}

	if (info_ptr->func == NULL) {
		info_ptr->id = id;
		info_ptr->func = func;
	} else {
		CCCI_ERROR_LOG(0, SYS,
			"register_sys_call_back fail: func(0x%x) registered! %ps\n",
			id, info_ptr->func);
	}

	return ret;
}
EXPORT_SYMBOL(register_ccci_sys_call_back);

void exec_ccci_sys_call_back(int cb_id, int data)
{
	ccci_sys_cb_func_t func;
	int id;
	struct ccci_sys_cb_func_info *curr_table = NULL;

	id = cb_id & 0xFF;
	if (id >= MAX_KERN_API) {
		CCCI_ERROR_LOG(0, SYS,
			"exec_sys_cb fail: invalid func id(0x%x)\n", cb_id);
		return;
	}

	if ((cb_id & (0x1000 | 0x100)) == 0x1000) {
		curr_table = ccci_sys_cb_table_1000;
	} else if ((cb_id & (0x1000 | 0x100)) == 0x100) {
		curr_table = ccci_sys_cb_table_100;
	} else {
		CCCI_ERROR_LOG(0, SYS,
			"exec_sys_cb fail: invalid func id(0x%x)\n", cb_id);
		return;
	}

	func = curr_table[id].func;
	if (func != NULL)
		func(data);
	else
		CCCI_ERROR_LOG(0, SYS,
			"exec_sys_cb fail: func id(0x%x) not register!\n",
			cb_id);
}

static int battery_get_bat_voltage(void)
{
	union power_supply_propval prop;
	struct power_supply *psy;
	int ret;

	psy = power_supply_get_by_name("battery");
	if (psy == NULL) {
		CCCI_ERROR_LOG(-1, SYS, "%s can't get battery node\n",
			__func__);
		return 0;
	}
	ret = power_supply_get_property(psy,
		POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, SYS, "%s get battery fail\n",
			__func__);
		return 0;
	}

	return prop.intval;
}

static int sys_msg_send_battery(struct port_t *port)
{
	int data;

	data = battery_get_bat_voltage()/1000;
	CCCI_REPEAT_LOG(0, SYS, "get bat voltage %d\n", data);
	port_send_msg_to_md(port, MD_GET_BATTERY_INFO, data, 1);
	return 0;
}

static void sys_msg_handler(struct port_t *port, struct sk_buff *skb)
{
	struct ccci_header *ccci_h = (struct ccci_header *)skb->data;
	unsigned long rem_nsec;
	u64 ts_nsec, ref, cost;

	CCCI_NORMAL_LOG(0, SYS, "system message (%x %x %x %x)\n",
		ccci_h->data[0], ccci_h->data[1],
		ccci_h->channel, ccci_h->reserved);

	ts_nsec = sched_clock();
	ref = ts_nsec;
	rem_nsec = do_div(ts_nsec, 1000000000);
	CCCI_HISTORY_LOG(0, SYS, "[%5lu.%06lu]sysmsg-%08x %08x %04x\n",
			(unsigned long)ts_nsec, rem_nsec / 1000,
			ccci_h->data[1], ccci_h->reserved, ccci_h->seq_num);

	switch (ccci_h->data[1]) {
	case MD_WDT_MONITOR:
		/* abandoned */
		break;
#ifdef TEST_MESSAGE_FOR_BRINGUP
	case TEST_MSG_ID_MD2AP:
		port_sys_echo_test(port, ccci_h->reserved);
		break;
	case TEST_MSG_ID_L1CORE_MD2AP:
		port_sys_echo_test_l1core(port, ccci_h->reserved);
		break;
#endif
	case SIM_LOCK_RANDOM_PATTERN:
		fsm_monitor_send_message(CCCI_MD_MSG_RANDOM_PATTERN, 0);
		break;
#ifdef FEATURE_SCP_CCCI_SUPPORT
	case CCISM_SHM_INIT_ACK:
		/* Fall through */
	case CCISM_SHM_INIT_DONE:
#endif
	case MD_SIM_TYPE:
		/* Fall through */
	case MD_TX_POWER:
		/* Fall through */
	case MD_RF_MAX_TEMPERATURE_SUB6:
		/* Fall through */
	case MD_RF_ALL_TEMPERATURE_MMW:
		/* Fall through */
	case MD_SW_MD1_TX_POWER_REQ:
		/* Fall through */
	case MD_DISPLAY_DYNAMIC_MIPI:
		/* Fall through */
	case MD_NR_BAND_ACTIVATE_INFO:
		fallthrough;
	case LWA_CONTROL_MSG:
		fallthrough;
	case MD_DRAM_SLC:
		exec_ccci_sys_call_back(ccci_h->data[1], ccci_h->reserved);
		break;
	case MD_GET_BATTERY_INFO:
		sys_msg_send_battery(port);
		break;
	case MD_RF_HOPPING_NOTIFY:
		sys_msg_MD_RF_Notify(ccci_h->reserved, ccci_h->data[0]);
		break;
	};
	ccci_free_skb(skb);
	ts_nsec = sched_clock();
	cost = ts_nsec - ref;
	div_u64(cost, 1000);
	CCCI_HISTORY_LOG(0, SYS, "cost: %llu us\n", cost);

}

static int port_sys_init(struct port_t *port)
{
	CCCI_DEBUG_LOG(0, SYS,
		"kernel port %s is initializing\n", port->name);

	swtp_init();
	port->skb_handler = &sys_msg_handler;
	port->private_data = kthread_run(port_kthread_handler, port, "%s",
							port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	return 0;
}

struct port_ops sys_port_ops = {
	.init = &port_sys_init,
	.recv_skb = &port_recv_skb,
};
