// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/clk.h> /* for clk_prepare/un* */

#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_fsm_internal.h"
#include "md_sys1_platform.h"
#include "modem_secure_base.h"
#include "ccci_fsm_scp_c.h"

#ifdef FEATURE_SCP_CCCI_SUPPORT
#include "scp_ipi.h"

static struct ccci_clk_node scp_clk_table[] = {
	{ NULL, "infra-ccif2-ap"},
	{ NULL, "infra-ccif2-md"},
};


static struct ccci_fsm_scp ccci_scp_ctl;

static void ccci_scp_md_state_sync(enum MD_STATE old_state,
	enum MD_STATE new_state)
{
	//ccci_scp_ctl.old_state = new_state;
	/* 2. maybe can record the md_state to ccci_scp_ctl.md_state */
	schedule_work(&ccci_scp_ctl.scp_md_state_sync_work);
}

/*
 * for debug log:
 * 0 to disable; 1 for print to ram; 2 for print to uart
 * other value to desiable all log
 */
#ifdef CCCI_KMODULE_ENABLE
#ifndef CCCI_LOG_LEVEL /* for platform override */
#define CCCI_LOG_LEVEL CCCI_LOG_CRITICAL_UART
#endif
unsigned int ccci_debug_enable = CCCI_LOG_LEVEL;
#endif

static atomic_t scp_state = ATOMIC_INIT(SCP_CCCI_STATE_INVALID);
static struct ccci_ipi_msg_out *scp_ipi_tx_msg;
#if !IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_CM4_SUPPORT) //MD_GENERATION >= 6297
static struct ccci_ipi_msg_in scp_ipi_rx_msg;
#endif
static struct mutex scp_ipi_tx_mutex;
static struct work_struct scp_ipi_rx_work;
static wait_queue_head_t scp_ipi_rx_wq;
static struct ccci_skb_queue scp_ipi_rx_skb_list;
static unsigned int init_work_done;
static unsigned int scp_clk_last_state;
static atomic_t scp_md_sync_flg;
static unsigned int ipi_msg_out_len;

static inline void ccci_scp_ipi_msg_add_magic(struct ccci_ipi_msg_out *ipi_msg)
{
	ipi_msg->data[1] = SCP_MSG_CHECK_A;
	ipi_msg->data[2] = SCP_MSG_CHECK_B;
}

static int ccci_scp_ipi_send(int op_id, void *data)
{
	int ret = 0;
#if !IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_CM4_SUPPORT)
	int ipi_status = 0;
	unsigned int cnt = 0;
#endif

	if (atomic_read(&scp_state) == SCP_CCCI_STATE_INVALID) {
		CCCI_ERROR_LOG(0, FSM,
			"ignore IPI %d, SCP state %d!\n",
			op_id, atomic_read(&scp_state));
		return -CCCI_ERR_MD_NOT_READY;
	}

	mutex_lock(&scp_ipi_tx_mutex);
	memset(scp_ipi_tx_msg, 0, ipi_msg_out_len);

	scp_ipi_tx_msg->md_id = 0;
	scp_ipi_tx_msg->op_id = op_id;
	scp_ipi_tx_msg->data[0] = *((u32 *)data);
	if (ccci_scp_ctl.ipi_msg_out_data_num == 3) {
		ccci_scp_ipi_msg_add_magic(scp_ipi_tx_msg);
		CCCI_NORMAL_LOG(0, FSM,
			"IPI send op_id=%d,data0=0x%x,data1=0x%x,data2=0x%x,size=%d\n",
			scp_ipi_tx_msg->op_id,
			scp_ipi_tx_msg->data[0], scp_ipi_tx_msg->data[1], scp_ipi_tx_msg->data[2],
			ipi_msg_out_len);
	} else
		CCCI_NORMAL_LOG(0, FSM,
			"IPI send op_id=%d,data0=0x%x,size=%d\n",
			scp_ipi_tx_msg->op_id, scp_ipi_tx_msg->data[0], ipi_msg_out_len);

#if !IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_CM4_SUPPORT)
	while (1) {
		ipi_status = mtk_ipi_send(&scp_ipidev, IPI_OUT_APCCCI_0,
		0, scp_ipi_tx_msg, (ipi_msg_out_len / 4), 1);
		if (ipi_status != IPI_PIN_BUSY)
			break;
		cnt++;
		if (cnt > 10) {
			CCCI_ERROR_LOG(0, FSM, "IPI send 10 times!\n");
			/* aee_kernel_warning("ccci", "ipi:tx busy");*/
			break;
		}
	}
	if (ipi_status != IPI_ACTION_DONE) {
		CCCI_ERROR_LOG(0, FSM, "IPI send fail, ipi_status = %d!\n", ipi_status);
		ret = -CCCI_ERR_MD_NOT_READY;
	}
#else
	if (scp_ipi_send(IPI_APCCCI, scp_ipi_tx_msg, ipi_msg_out_len, 1, SCP_A_ID)
			!= SCP_IPI_DONE) {
		CCCI_ERROR_LOG(0, FSM, "IPI send fail!\n");
		ret = -CCCI_ERR_MD_NOT_READY;
	}
#endif
	mutex_unlock(&scp_ipi_tx_mutex);
	return ret;
}

static int scp_set_clk_cg(unsigned int on)
{
	int idx, ret;

	if (!(on == 0 || on == 1)) {
		CCCI_ERROR_LOG(MD_SYS1, FSM,
			"%s:on=%u is invalid\n", __func__, on);
		return -1;
	}

	if (on == scp_clk_last_state) {
		CCCI_NORMAL_LOG(MD_SYS1, FSM, "%s:on=%u skip set scp clk!\n",
			__func__, on);
		return 0;
	}

	/* Before OFF CCIF2 clk, set the ACK register to 1 */
	if (on == 0) {
		if (!ccci_scp_ctl.ccif2_ap_base || !ccci_scp_ctl.ccif2_md_base) {
			CCCI_ERROR_LOG(MD_SYS1, FSM, "%s can't ack ccif2\n",
				       __func__);
		} else {
			ccci_write32(ccci_scp_ctl.ccif2_ap_base, APCCIF_ACK, 0xFFFF);
			ccci_write32(ccci_scp_ctl.ccif2_md_base, APCCIF_ACK, 0xFFFF);
			CCCI_NORMAL_LOG(MD_SYS1, FSM, "%s, ack ccif2 reg done!\n",
					__func__);
		}
	}

	if (ccci_scp_ctl.scp_clk_free_run) {
		CCCI_NORMAL_LOG(MD_SYS1, FSM, "%s:on=%u set done, free run!\n", __func__, on);
		scp_clk_last_state = on;
		return 0;
	}

	for (idx = 0; idx < ARRAY_SIZE(scp_clk_table); idx++) {
		if (on) {
			ret = clk_prepare_enable(scp_clk_table[idx].clk_ref);
			if (ret) {
				CCCI_ERROR_LOG(MD_SYS1, FSM,
					"open scp clk fail:%s,ret=%d\n",
					scp_clk_table[idx].clk_name, ret);
				return -1;
			}
		} else
			clk_disable_unprepare(scp_clk_table[idx].clk_ref);
	}

	CCCI_NORMAL_LOG(MD_SYS1, FSM, "%s:on=%u set done!\n",
		__func__, on);
	scp_clk_last_state = on;

	return 0;
}

void ccci_notify_atf_set_scpmem(void)
{
	struct arm_smccc_res res = {0};

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, SCP_CLK_SET_DONE,
		0, 0, 0, 0, 0, 0, &res);
	CCCI_NORMAL_LOG(MD_SYS1, FSM, "%s [done] res = 0x%lX\n", __func__, res.a0);
}
static void ccci_scp_md_state_sync_work(struct work_struct *work)
{
	int ret = 0;
	int count = 0;
	enum MD_STATE_FOR_USER state;
	struct ccci_fsm_ctl *ctl = fsm_get_entity();

	if (!ctl) {
		CCCI_ERROR_LOG(0, FSM, "%s: ccci_fsm_entries is NULL !\n", __func__);
		return;
	}

	/* 3. maybe can replace the ctl->md_state to ccci_scp_ctl.md_state */
	switch (ctl->md_state) {
	case READY:
		while (count < SCP_BOOT_TIMEOUT/EVENT_POLL_INTEVAL) {
			if (atomic_read(&scp_state) == SCP_CCCI_STATE_BOOTING
				|| atomic_read(&scp_state) == SCP_CCCI_STATE_RBREADY
				|| atomic_read(&scp_state) == SCP_CCCI_STATE_STOP)
				break;
			count++;
			msleep(EVENT_POLL_INTEVAL);
		}
		if (count == SCP_BOOT_TIMEOUT/EVENT_POLL_INTEVAL)
			CCCI_ERROR_LOG(0, FSM,
				"SCP init not ready!\n");
		else {
			ret = scp_set_clk_cg(1);
			if (ret) {
				CCCI_ERROR_LOG(0, FSM,
					"fail to set scp clk, ret = %d\n", ret);
				break;
			}
			if (atomic_read(&scp_md_sync_flg) == 0) {
				ccci_notify_atf_set_scpmem();
				ret = ccci_port_send_msg_to_md(CCCI_SYSTEM_TX,
					CCISM_SHM_INIT, 0, 1);
				if (ret < 0)
					CCCI_ERROR_LOG(0, FSM, "fail to send CCISM_SHM_INIT %d\n",
						ret);
				atomic_set(&scp_md_sync_flg,1);
			}
		}
		break;
	case INVALID:
	case GATED:
		state = MD_STATE_INVALID;
		ccci_scp_ipi_send(CCCI_OP_MD_STATE, &state);
		break;
	case EXCEPTION:
		state = MD_STATE_EXCEPTION;
		ccci_scp_ipi_send(CCCI_OP_MD_STATE, &state);
		break;
	default:
		break;
	};
}

static void ccci_scp_ipi_rx_work(struct work_struct *work)
{
	struct ccci_ipi_msg_in *ipi_msg_ptr = NULL;
	struct sk_buff *skb = NULL;
	int ret;
	struct ccci_fsm_ctl *ctl = fsm_get_entity();

	while (!skb_queue_empty(&scp_ipi_rx_skb_list.skb_list)) {
		skb = ccci_skb_dequeue(&scp_ipi_rx_skb_list);
		if (skb == NULL) {
			CCCI_ERROR_LOG(-1, CORE,
				"ccci_skb_dequeue fail\n");
			return;
		}
		ipi_msg_ptr = (struct ccci_ipi_msg_in *)skb->data;
		if (!get_modem_is_enabled()) {
			CCCI_ERROR_LOG(0, CORE, "MD not exist\n");
			return;
		}
		switch (ipi_msg_ptr->op_id) {
		case CCCI_OP_SCP_STATE:
			switch (ipi_msg_ptr->data) {
			case SCP_CCCI_STATE_BOOTING:
				if (atomic_read(&scp_state) ==
					SCP_CCCI_STATE_RBREADY) {
					CCCI_NORMAL_LOG(0, FSM,
						"SCP reset detected\n");
					ccci_port_send_msg_to_md(
					CCCI_SYSTEM_TX, CCISM_SHM_INIT, 0, 1);
				} else {
					CCCI_NORMAL_LOG(0, FSM,
						"SCP boot up\n");
				}
				/* too early to init share memory here,
				 * EMI MPU may not be ready yet
				 */
				break;
			case SCP_CCCI_STATE_RBREADY:
				ccci_port_send_msg_to_md(CCCI_SYSTEM_TX,
				CCISM_SHM_INIT_DONE, 0, 1);

				break;
			case SCP_CCCI_STATE_STOP:
				if (ctl->md_state != READY) {
					CCCI_NORMAL_LOG(0, FSM,
						"MD INVALID,scp send ack to ap\n");
					ret = scp_set_clk_cg(0);
					atomic_set(&scp_md_sync_flg, 0);
					if (ret)
						CCCI_ERROR_LOG(0, FSM,
							"fail to set scp clk, ret = %d\n", ret);
				} else {
					CCCI_NORMAL_LOG(0, FSM,
						"md_state = %d, shouldn't off ccif2\n",
						ctl->md_state);
				}
				break;
			default:
				break;
			};
			atomic_set(&scp_state, ipi_msg_ptr->data);
			break;
		default:
			break;
		};
		ccci_free_skb(skb);
	}
}

#if !IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_CM4_SUPPORT)
/*
 * IPI for logger init
 * @param id:   IPI id
 * @param prdata: callback function parameter
 * @param data:  IPI data
 * @param len: IPI data length
 */
static int ccci_scp_ipi_handler(unsigned int id, void *prdata, void *data,
			unsigned int len)
{
	struct sk_buff *skb = NULL;

	if (len != sizeof(struct ccci_ipi_msg_in)) {
		CCCI_ERROR_LOG(-1, CORE,
			"IPI handler, data length wrong %d vs. %d\n",
			len, (int)sizeof(struct ccci_ipi_msg_in));
		return -1;
	}

	skb = ccci_alloc_skb(len, 0, 0);
	if (!skb)
		return -1;
	memcpy(skb_put(skb, len), data, len);
	ccci_skb_enqueue(&scp_ipi_rx_skb_list, skb);
	/* ipi_send use mutex, can not be called from ISR context */
	schedule_work(&scp_ipi_rx_work);

	return 0;
}
#else
static void ccci_scp_ipi_handler(int id, void *data, unsigned int len)
{
	struct ccci_ipi_msg_in *ipi_msg_ptr = (struct ccci_ipi_msg_in *)data;
	struct sk_buff *skb = NULL;

	if (len != sizeof(struct ccci_ipi_msg_in)) {
		CCCI_ERROR_LOG(-1, CORE,
			"IPI handler, data length wrong %d vs. %d\n",
			len, (int)sizeof(struct ccci_ipi_msg_in));
		return;
	}
	CCCI_NORMAL_LOG(0, CORE, "IPI handler %d/0x%x, %d\n",
		ipi_msg_ptr->op_id,
		ipi_msg_ptr->data, len);

	skb = ccci_alloc_skb(len, 0, 0);
	if (!skb)
		return;
	memcpy(skb_put(skb, len), data, len);
	ccci_skb_enqueue(&scp_ipi_rx_skb_list, skb);
	/* ipi_send use mutex, can not be called from ISR context */
	schedule_work(&scp_ipi_rx_work);
}
#endif
#endif

int fsm_ccism_init_ack_handler(int data)
{
#ifdef FEATURE_SCP_CCCI_SUPPORT
	struct ccci_smem_region *ccism_scp =
		ccci_md_get_smem_by_user_id(SMEM_USER_CCISM_SCP);
	if (!ccism_scp) {
		CCCI_ERROR_LOG(-1, FSM, "ccism_scp is NULL!\n");
		return -1;
	}
	memset_io(ccism_scp->base_ap_view_vir, 0, ccism_scp->size);
	ccci_scp_ipi_send(CCCI_OP_SHM_INIT,
		&ccism_scp->base_ap_view_phy);
#endif
	return 0;
}

/*when reciver md init done, then transmit md state to scp */
static int fsm_ccism_init_done_handler(int data)
{
#ifdef FEATURE_SCP_CCCI_SUPPORT
	data = ccci_fsm_get_md_state_for_user();
	ccci_scp_ipi_send(CCCI_OP_MD_STATE, &data);
#endif
	return 0;
}

#ifdef FEATURE_SCP_CCCI_SUPPORT
static int fsm_sim_type_handler(int data)
{
	struct ccci_per_md *per_md_data = ccci_get_per_md_data();
	if (!per_md_data) {
		CCCI_ERROR_LOG(-1, FSM, "per_md_data is NULL!\n");
		return -1;
	}
	per_md_data->sim_type = data;
	return 0;
}
#endif

#ifdef FEATURE_SCP_CCCI_SUPPORT
void fsm_scp_init0(void)
{
	enum MD_STATE md_stat = ccci_fsm_get_md_state();

	mutex_init(&scp_ipi_tx_mutex);

	if (!init_work_done) {
		INIT_WORK(&scp_ipi_rx_work, ccci_scp_ipi_rx_work);
		init_work_done = 1;
	}
	init_waitqueue_head(&scp_ipi_rx_wq);
	ccci_skb_queue_init(&scp_ipi_rx_skb_list, 16, 16, 0);

	CCCI_NORMAL_LOG(-1, FSM, "register IPI\n");

#if !IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_CM4_SUPPORT)
	if (mtk_ipi_register(&scp_ipidev, IPI_IN_APCCCI_0,
		(void *)ccci_scp_ipi_handler, NULL,
		&scp_ipi_rx_msg) != IPI_ACTION_DONE)
		CCCI_ERROR_LOG(-1, FSM, "register IPI fail!\n");
#else
	if (scp_ipi_registration(IPI_APCCCI, ccci_scp_ipi_handler,
		"AP CCCI") != SCP_IPI_DONE)
		CCCI_ERROR_LOG(-1, FSM, "register IPI fail!\n");
#endif
	atomic_set(&scp_state, SCP_CCCI_STATE_BOOTING);

	if (md_stat != INVALID)
		ccci_scp_md_state_sync(INVALID, md_stat);
}

#endif


#ifdef FEATURE_SCP_CCCI_SUPPORT

static int apsync_event(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	switch (event) {
	case SCP_EVENT_READY:
		fsm_scp_init0();
		break;
	case SCP_EVENT_STOP:
		atomic_set(&scp_md_sync_flg, 0);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block apsync_notifier = {
	.notifier_call = apsync_event,
};

static int ccif_scp_clk_init(struct device *dev)
{
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(scp_clk_table); idx++) {
		scp_clk_table[idx].clk_ref = devm_clk_get(dev,
			scp_clk_table[idx].clk_name);
		if (IS_ERR(scp_clk_table[idx].clk_ref)) {
			CCCI_ERROR_LOG(-1, FSM,
				"scp get %s failed\n",
				scp_clk_table[idx].clk_name);
			scp_clk_table[idx].clk_ref = NULL;
			return -1;
		}
	}

	return 0;
}

static int fsm_scp_hw_init(struct ccci_fsm_scp *scp_ctl, struct device *dev)
{
	scp_ctl->ccif2_ap_base = of_iomap(dev->of_node, 0);
	scp_ctl->ccif2_md_base = of_iomap(dev->of_node, 1);

	if (!scp_ctl->ccif2_ap_base || !scp_ctl->ccif2_md_base) {
		CCCI_ERROR_LOG(-1, FSM,
			"ccif2_ap_base=NULL or ccif2_md_base=NULL\n");
		return -1;
	}

	return 0;
}

static int fsm_scp_ipi_out_msg_init(struct ccci_fsm_scp *scp_ctl)
{
#if !IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_CM4_SUPPORT)
	enum table_item_num {
		send_item_num = 3,
		recv_item_num = 5
	};

	struct device_node *node = NULL;
	int ret;
	struct scp_ipi_info ipi_info;
	unsigned int ipi_count, i;

	node = of_find_compatible_node(NULL, NULL, "mediatek,scp");
	if (!node) {
		CCCI_ERROR_LOG(-1, FSM, "[%s] scp node is not exist\n",
			__func__);
		return -ENODEV;
	}

	ipi_count = of_property_count_u32_elems(node, "send-table") / send_item_num;
	if (ipi_count <= 0) {
		CCCI_ERROR_LOG(-1, FSM, "[%s] scp send table not found\n",
			__func__);
		return -ENODEV;
	}

	memset(&ipi_info, 0, sizeof(struct scp_ipi_info));

	for (i = 0; i < ipi_count; ++i) {
		ret = of_property_read_u32_index(node, "send-table",
			i * send_item_num, &ipi_info.chan_id);
		if (ret) {
			CCCI_ERROR_LOG(-1, FSM, "[%s] Cannot get ipi id ret:(%d), i:(%u)\n",
				__func__, ret, i);
			return -ENODEV;
		}

		if(ipi_info.chan_id == IPI_OUT_APCCCI_0) {
			ret = of_property_read_u32_index(node, "send-table",
				i * send_item_num + 2, &ipi_info.msg_size);
			if (ret) {
				CCCI_ERROR_LOG(-1, FSM, "[%s] Cannot get msg_size ret:(%d), i:(%u)\n",
					__func__, ret, i);
				return -ENODEV;
			}

			/* sizeof(ccci_ipi_msg_out) = 4 bytes
			 * data size = (4 * data_num) bytes
			 * ipi_info.msg_size * 4 = sizeof(ccci_ipi_msg_out) + data size
			 * so the data_num = ipi_info.msg_size - 1;
			 * ipi_msg_out_len = 4 + 4 * data_num = 4 * (data_num + 1)；
			 */
			scp_ctl->ipi_msg_out_data_num = ipi_info.msg_size - 1;
			ipi_msg_out_len = 4 * (ccci_scp_ctl.ipi_msg_out_data_num + 1);
			CCCI_NORMAL_LOG(0, FSM, "[%s] data_num = %d, total size = %d bytes\n",
				__func__, scp_ctl->ipi_msg_out_data_num, ipi_msg_out_len);

			scp_ipi_tx_msg = kzalloc(ipi_msg_out_len, GFP_KERNEL);
			if (!scp_ipi_tx_msg) {
				CCCI_ERROR_LOG(-1, FSM, "[%s] scp_ipi_tx_msg kzalloc fail\n",
					__func__);
				return -ENOMEM;
			}
			return 0;
		}
	}
	return -ENODEV;
#else
	scp_ctl->ipi_msg_out_data_num = 1;
	ipi_msg_out_len = 4 * (ccci_scp_ctl.ipi_msg_out_data_num + 1);
	scp_ipi_tx_msg = kzalloc(ipi_msg_out_len, GFP_KERNEL);
	if (!scp_ipi_tx_msg) {
		CCCI_ERROR_LOG(-1, FSM, "[%s] CM4 scp_ipi_tx_msg kzalloc fail\n",
			__func__);
		return -ENOMEM;
	}
	return 0;
#endif
}

int fsm_scp_init(struct ccci_fsm_scp *scp_ctl, struct device *dev)
{
	int ret = 0;

	ret = fsm_scp_hw_init(scp_ctl, dev);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, FSM, "ccci scp hw init fail\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node,
		"mediatek,scp-clk-free-run", &scp_ctl->scp_clk_free_run);
	if (ret < 0)
		scp_ctl->scp_clk_free_run = 0;

	if (!scp_ctl->scp_clk_free_run) {
		ret = ccif_scp_clk_init(dev);
		if (ret < 0) {
			CCCI_ERROR_LOG(-1, FSM, "ccif scp clk init fail\n\n");
			return ret;
		}
	} else
		CCCI_NORMAL_LOG(0, FSM, "No need control ccif2 clk\n");

	ret = fsm_scp_ipi_out_msg_init(scp_ctl);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, FSM, "ccif scp ipi msg size init fail\n");
		return ret;
	}

	scp_A_register_notify(&apsync_notifier);

	INIT_WORK(&scp_ctl->scp_md_state_sync_work,
		ccci_scp_md_state_sync_work);
	register_ccci_sys_call_back(CCISM_SHM_INIT_ACK,
		fsm_ccism_init_ack_handler);
	register_ccci_sys_call_back(CCISM_SHM_INIT_DONE,
		fsm_ccism_init_done_handler);

	register_ccci_sys_call_back(MD_SIM_TYPE, fsm_sim_type_handler);

	return ret;
}
#endif

#ifdef CCCI_KMODULE_ENABLE
#ifdef FEATURE_SCP_CCCI_SUPPORT

int ccci_scp_probe(struct platform_device *pdev)
{
	int ret;

	if (!get_modem_is_enabled()) {
		CCCI_ERROR_LOG(-1, FSM, "%s:MD not exist,exit\n", __func__);
		return -1;
	}

	ret = fsm_scp_init(&ccci_scp_ctl, &pdev->dev);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, FSM, "ccci get scp info fail");
		return ret;
	}
	ret = ccci_register_md_state_receiver(KERN_MD_STAT_RCV_SCP,
		ccci_scp_md_state_sync);
	if (ret)
		CCCI_ERROR_LOG(-1, FSM, "register md status receiver fail: %d", ret);

	return ret;
}

static const struct of_device_id ccci_scp_of_ids[] = {
	{.compatible = "mediatek,ccci_md_scp"},
	{}
};

static struct platform_driver ccci_scp_driver = {

	.driver = {
		.name = "ccci_md_scp",
		.of_match_table = ccci_scp_of_ids,
	},

	.probe = ccci_scp_probe,
};

static int __init ccci_scp_init(void)
{
	int ret;

	CCCI_NORMAL_LOG(-1, FSM, "ccci scp driver init start\n");

	ret = platform_driver_register(&ccci_scp_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, FSM, "ccci scp driver init fail %d", ret);
		return ret;
	}
	CCCI_NORMAL_LOG(-1, FSM, "ccci scp driver init end\n");
	return 0;
}

static void __exit ccci_scp_exit(void)
{
}

module_init(ccci_scp_init);
module_exit(ccci_scp_exit);

#endif
MODULE_AUTHOR("ccci");
MODULE_DESCRIPTION("ccci scp driver");
MODULE_LICENSE("GPL");

#endif
