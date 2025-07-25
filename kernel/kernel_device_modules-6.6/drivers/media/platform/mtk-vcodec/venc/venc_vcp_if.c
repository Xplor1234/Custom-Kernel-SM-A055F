// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Chia-Mao Hung<chia-mao.hung@mediatek.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/mtk_vcu_controls.h>
#include <linux/delay.h>
#include <soc/mediatek/smi.h>
#include <media/v4l2-mem2mem.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>

#include "../mtk_vcodec_drv.h"
#include "../mtk_vcodec_util.h"
#include "../mtk_vcodec_enc.h"
#include "../venc_drv_base.h"
#include "../venc_ipi_msg.h"
#include "../venc_vcu_if.h"
#include "mtk_vcodec_enc_pm.h"
#include "vcp_ipi_pin.h"
#if IS_ENABLED(CONFIG_MTK_EMI)
#include <soc/mediatek/emi.h>
#endif
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#include "vcp_status.h"
#endif
// TODO: need remove ISR ipis
#include "mtk_vcodec_intr.h"

#ifdef CONFIG_MTK_ENG_BUILD
#define IPI_TIMEOUT_MS          (10000U)
#else
#define IPI_TIMEOUT_MS          (5000U + ((mtk_vcodec_dbg | mtk_v4l2_dbg_level) ? 5000U : 0U))
#endif
#define IPI_SEND_TIMEOUT_MS	1000U
#define IPI_FIRST_VENC_SETPARAM_TIMEOUT_MS    (60000U)
#define IPI_POLLING_INTERVAL_US    10

struct vcp_enc_mem_list {
	struct vcodec_mem_obj mem;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct list_head list;
};

static void handle_enc_init_msg(struct mtk_vcodec_dev *dev, struct venc_vcu_inst *vcu, void *data)
{
	struct venc_vcu_ipi_msg_init *msg = data;
	__u64 shmem_pa_start = (__u64)vcp_get_reserve_mem_phys_ex(VENC_MEM_ID);
	__u64 inst_offset = ((msg->vcu_inst_addr & 0x0FFFFFFF) - (shmem_pa_start & 0x0FFFFFFF));

	if (vcu == NULL)
		return;

	mtk_vcodec_debug(vcu, "+ venc_inst = 0x%lx, vcu_inst_addr = 0x%llx, id = %d",
		(uintptr_t)msg->ap_inst_addr, msg->vcu_inst_addr, msg->msg_id);

	vcu->init_done = true;
	vcu->inst_addr = msg->vcu_inst_addr;
	vcu->vsi = (void *)((__u64)vcp_get_reserve_mem_virt_ex(VENC_MEM_ID) + inst_offset);
}

static void handle_query_cap_ack_msg(struct venc_vcu_ipi_query_cap_ack *msg)
{
	struct venc_vcu_inst *vcu = (struct venc_vcu_inst *)msg->ap_inst_addr;
	void *data;
	int size = 0;
	__u64 shmem_pa_start = (__u64)vcp_get_reserve_mem_phys_ex(VENC_MEM_ID);
	__u64 data_offset = ((msg->vcu_data_addr & 0x0FFFFFFF) - (shmem_pa_start & 0x0FFFFFFF));

	if (vcu == NULL)
		return;
	mtk_vcodec_debug(vcu, "+ ap_inst_addr = 0x%lx, vcu_data_addr = 0x%llx, id = %d",
		(uintptr_t)msg->ap_inst_addr, msg->vcu_data_addr, msg->id);

	/* mapping vcp address to kernel virtual address */
	data = (void *)((__u64)vcp_get_reserve_mem_virt_ex(VENC_MEM_ID) + data_offset);
	if (data == NULL)
		return;
	switch (msg->id) {
	case GET_PARAM_VENC_CAP_SUPPORTED_FORMATS:
		size = sizeof(struct mtk_video_fmt);
		memcpy((void *)msg->ap_data_addr, data,
			size * MTK_MAX_ENC_CODECS_SUPPORT);
		break;
	case GET_PARAM_VENC_CAP_FRAME_SIZES:
		size = sizeof(struct mtk_codec_framesizes);
		memcpy((void *)msg->ap_data_addr, data,
			size * MTK_MAX_ENC_CODECS_SUPPORT);
		break;
	case GET_PARAM_VENC_CAP_COMMON:
		size = sizeof(struct mtk_codec_capability);
		memcpy((void *)msg->ap_data_addr, data, size);
		break;
	default:
		break;
	}
	mtk_vcodec_debug(vcu, "- vcu_inst_addr = 0x%llx", vcu->inst_addr);
}

static struct device *get_dev_by_mem_type(struct venc_inst *inst, struct vcodec_mem_obj *mem)
{
	if (mem->type == MEM_TYPE_FOR_SW) {
		if (inst->ctx->dev->iommu_domain_swtich && (inst->ctx->id & 1))
			return vcp_get_io_device_ex(VCP_IOMMU_WORK);
		else
			return vcp_get_io_device_ex(VCP_IOMMU_VCP);
	} else if (mem->type == MEM_TYPE_FOR_SEC_SW)
		return vcp_get_io_device_ex(VCP_IOMMU_SEC);
	else if (mem->type == MEM_TYPE_FOR_HW ||
		 mem->type == MEM_TYPE_FOR_HW_CACHE ||
		 mem->type == MEM_TYPE_FOR_SEC_HW ||
		 mem->type == MEM_TYPE_FOR_SEC_WFD_HW)
		return inst->vcu_inst.ctx->dev->smmu_dev;
	else
		return NULL;
}

static int venc_vcp_ipi_send(struct venc_inst *inst, void *msg, int len,
	bool is_ack, bool need_wait_suspend, bool has_lock_dvfs)
{
	int ret, ipi_size;
	unsigned long timeout = 0;
	struct share_obj obj;
	unsigned int suspend_block_cnt = 0;
	int ipi_wait_type = IPI_SEND_WAIT;
	struct mtk_ipi_device *ipidev;
	struct venc_ap_ipi_msg_set_param *ap_out_msg;
	struct venc_ap_ipi_msg_common *msg_ap = (struct venc_ap_ipi_msg_common *)msg;
	struct venc_ap_ipi_msg_indp *msg_indp = (struct venc_ap_ipi_msg_indp *)msg;
	bool use_msg_indp = (is_ack || msg_ap->msg_id == AP_IPIMSG_ENC_INIT ||
		(msg_ap->msg_id & IPIMSG_TYPE_BITS) == IPIMSG_NO_INST_OFFSET); // msg use VENC_MSG_PREFIX

	if ((!use_msg_indp && msg_ap->vcu_inst_addr == 0) ||
	     (use_msg_indp && msg_indp->ap_inst_addr == 0 && !is_ack)) {
		mtk_vcodec_err(inst, "msg 0x%x inst addr null", msg_ap->msg_id);
		return -EINVAL;
	}

	if (preempt_count())
		ipi_wait_type = IPI_SEND_POLLING;

	if (!is_ack) {
		mutex_lock(&inst->ctx->dev->ipi_mutex);
		if (need_wait_suspend) {
			while (inst->ctx->dev->is_codec_suspending == 1) {
				mutex_unlock(&inst->ctx->dev->ipi_mutex);
				if (has_lock_dvfs)
					mutex_unlock(&inst->ctx->dev->enc_dvfs_mutex);

				suspend_block_cnt++;
				if (suspend_block_cnt > SUSPEND_TIMEOUT_CNT) {
					mtk_v4l2_debug(0, "VENC blocked by suspend\n");
					suspend_block_cnt = 0;
				}
				usleep_range(10000, 20000);

				if (has_lock_dvfs)
					mutex_lock(&inst->ctx->dev->enc_dvfs_mutex);
				mutex_lock(&inst->ctx->dev->ipi_mutex);
			}
		}

	}

	if (inst->vcu_inst.abort || inst->vcu_inst.daemon_pid != vcp_cmd_ex(VENC_FEATURE_ID, VCP_GET_GEN, "venc_srv"))
		goto ipi_err_unlock;

	while (!is_vcp_ready_ex(VENC_FEATURE_ID)) {
		mtk_v4l2_debug((((timeout % 20) == 10) ? 0 : 4), "[VCP] wait ready %lu ms", timeout);
		mdelay(1);
		timeout++;
		if (timeout > VCP_SYNC_TIMEOUT_MS) {
			mtk_vcodec_err(inst, "VCP_A_ID not ready");
			/* mtk_smi_dbg_hang_detect("VENC VCP"); */
#if IS_ENABLED(CONFIG_MTK_EMI)
			mtk_emidbg_dump();
#endif
			//BUG_ON(1);
			goto ipi_err_unlock;
		}
	}

	if (len > (sizeof(struct share_obj) - sizeof(int32_t) - sizeof(uint32_t))) {
		mtk_vcodec_err(inst, "ipi data size wrong %d > %lu", len, sizeof(struct share_obj));
		goto ipi_err_unlock;
	}

	memset(&obj, 0, sizeof(obj));
	memcpy(obj.share_buf, msg, len);
	obj.id = inst->vcu_inst.id;
	obj.len = len;
	ipi_size = ((sizeof(u32) * 2) + len + 3) / 4;
	if (!is_ack)
		inst->vcu_inst.failure = 0;
	inst->ctx->err_msg = msg_ap->msg_id;

	mtk_v4l2_debug(2, "[%d] id %d len %d msg 0x%x is_ack %d %d",
		inst->ctx->id, obj.id, obj.len, msg_ap->msg_id, is_ack, inst->vcu_inst.signaled);

	ipidev = vcp_get_ipidev(VENC_FEATURE_ID);
	if (!ipidev) {
		mtk_vcodec_err(inst, "[%d] vcp_get_ipidev(VENC_FEATURE_ID) get NULL", inst->ctx->id);
		goto ipi_err_wait_and_unlock;
	}
	ret = mtk_ipi_send(ipidev, IPI_OUT_VENC_0, ipi_wait_type, &obj,
		ipi_size, IPI_SEND_TIMEOUT_MS);

	if (ret != IPI_ACTION_DONE) {
		mtk_vcodec_err(inst, "mtk_ipi_send %X fail %d", msg_ap->msg_id, ret);
		if (!is_ack)
			goto ipi_err_wait_and_unlock;
	}

	if (is_ack)
		return 0;

	// if (!is_ack)

	inst->vcu_inst.in_ipi = true;
wait_ack:
	/* wait for VCP's ACK */
	timeout = msecs_to_jiffies(IPI_TIMEOUT_MS);
	if (msg_ap->msg_id == AP_IPIMSG_ENC_SET_PARAM &&
		mtk_vcodec_is_state(inst->ctx, MTK_STATE_INIT)) {
		ap_out_msg = (struct venc_ap_ipi_msg_set_param *) msg;
		if (ap_out_msg->param_id == VENC_SET_PARAM_ENC)
			timeout = msecs_to_jiffies(IPI_FIRST_VENC_SETPARAM_TIMEOUT_MS);
	}

	if (ipi_wait_type == IPI_SEND_POLLING) {
		ret = IPI_TIMEOUT_MS * 1000;
		while (inst->vcu_inst.signaled == false) {
			udelay(IPI_POLLING_INTERVAL_US);
			ret -= IPI_POLLING_INTERVAL_US;
			if (ret < 0) {
				ret = 0;
				break;
			}
		}
	} else
		ret = wait_event_timeout(inst->vcu_inst.wq_hd,
			inst->vcu_inst.signaled, timeout);
	inst->vcu_inst.in_ipi = false;
	inst->vcu_inst.signaled = false;

	if (ret == 0) {
		mtk_vcodec_err(inst, "wait vcp ipi %X ack time out! %d %d",
			msg_ap->msg_id, ret, inst->vcu_inst.failure);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
		dump_vcp_irq_status();
#endif
		goto ipi_err_wait_and_unlock;
	} else if (-ERESTARTSYS == ret) {
		mtk_vcodec_err(inst, "wait vcp ipi %X ack ret %d RESTARTSYS retry! (%d)",
			msg_ap->msg_id, ret, inst->vcu_inst.failure);
		goto wait_ack;
	} else if (ret < 0) {
		mtk_vcodec_err(inst, "wait vcp ipi %X ack fail ret %d! (%d)",
			msg_ap->msg_id, ret, inst->vcu_inst.failure);
	} else if (inst->vcu_inst.abort) {
		mtk_vcodec_err(inst, "wait vcp ipi %X ack abort ret %d! (%d)",
			msg_ap->msg_id, ret, inst->vcu_inst.failure);
		goto ipi_err_wait_and_unlock;
	}
	mutex_unlock(&inst->ctx->dev->ipi_mutex);

	return inst->vcu_inst.failure;

ipi_err_wait_and_unlock:
	timeout = 0;
	if (inst->vcu_inst.daemon_pid == vcp_cmd_ex(VENC_FEATURE_ID, VCP_GET_GEN, "venc_srv")) {
		vcp_cmd_ex(VENC_FEATURE_ID, VCP_SET_HALT, "venc_srv");
		while (inst->vcu_inst.daemon_pid == vcp_cmd_ex(VENC_FEATURE_ID, VCP_GET_GEN, "venc_srv")) {
			if (timeout > VCP_SYNC_TIMEOUT_MS) {
				mtk_v4l2_debug(0, "halt restart timeout %x\n",
					inst->vcu_inst.daemon_pid);
				break;
			}
			usleep_range(10000, 20000);
			timeout += 10;
		}
	}
	inst->vcu_inst.failure = VENC_IPI_MSG_STATUS_FAIL;
	inst->ctx->err_msg = msg_ap->msg_id;

ipi_err_unlock:
	inst->vcu_inst.abort = 1;
	if (!is_ack)
		mutex_unlock(&inst->ctx->dev->ipi_mutex);

	return -EIO;
}

static void handle_venc_mem_alloc(struct venc_vcu_ipi_mem_op *msg)
{
	struct venc_vcu_inst *vcu = (struct venc_vcu_inst *)msg->ap_inst_addr;
	struct venc_inst *inst = NULL;
	struct device *dev = NULL;
	struct vcp_enc_mem_list *tmp = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *sgt = NULL;

	if (msg->mem.type == MEM_TYPE_FOR_SHM) {
		msg->status = 0;
		msg->mem.va = (__u64)vcp_get_reserve_mem_virt_ex(VENC_MEM_ID);
		msg->mem.pa = (__u64)vcp_get_reserve_mem_phys_ex(VENC_MEM_ID);
		msg->mem.len = (__u64)vcp_get_reserve_mem_size_ex(VENC_MEM_ID);
		msg->mem.iova = msg->mem.pa;

		mtk_v4l2_debug(4, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %d size of %lu %lu\n",
			msg->mem.va, msg->mem.pa, msg->mem.iova, msg->mem.len, msg->mem.type,
			sizeof(msg->mem), sizeof(*msg));
	} else {
		if (IS_ERR_OR_NULL(vcu))
			return;

		inst = container_of(vcu, struct venc_inst, vcu_inst);
		dev = get_dev_by_mem_type(inst, &msg->mem);
		msg->status = mtk_vcodec_alloc_mem(&msg->mem, dev, &attach, &sgt, MTK_INST_ENCODER);

		mtk_vcodec_debug(vcu, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %d\n",
			msg->mem.va, msg->mem.pa, msg->mem.iova, msg->mem.len, msg->mem.type);
	}

	if (msg->status) {
		mtk_vcodec_err(vcu, "fail %d, va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			msg->status, msg->mem.va, msg->mem.pa,
			msg->mem.iova, msg->mem.len,  msg->mem.type);
		/* reset prevent VCP TF */
		msg->mem.pa = 0;
		msg->mem.iova = 0;
	} else if (msg->mem.type != MEM_TYPE_FOR_SHM) {
		tmp = kmalloc(sizeof(struct vcp_enc_mem_list), GFP_KERNEL);
		if (tmp) {
			mutex_lock(vcu->ctx_ipi_lock);
			tmp->attach = attach;
			tmp->sgt = sgt;
			tmp->mem = msg->mem;
			list_add_tail(&tmp->list, &vcu->bufs);
			mutex_unlock(vcu->ctx_ipi_lock);
		}
	}
}

static void handle_venc_mem_free(struct venc_vcu_ipi_mem_op *msg)
{
	struct venc_vcu_inst *vcu = (struct venc_vcu_inst *)msg->ap_inst_addr;
	struct venc_inst *inst = NULL;
	struct device *dev = NULL;
	struct vcp_enc_mem_list *tmp = NULL;
	struct list_head *p, *q;
	bool found = 0;

	if (IS_ERR_OR_NULL(vcu))
		return;
	mutex_lock(vcu->ctx_ipi_lock);
	list_for_each_safe(p, q, &vcu->bufs) {
		tmp = list_entry(p, struct vcp_enc_mem_list, list);
		if (!memcmp(&tmp->mem, &msg->mem, sizeof(struct vcodec_mem_obj))) {
			found = 1;
			list_del(p);
			break;
		}
	}
	mutex_unlock(vcu->ctx_ipi_lock);
	if (!found) {
		mtk_vcodec_err(vcu, "not found  %d, va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			msg->status, msg->mem.va, msg->mem.pa,
			msg->mem.iova, msg->mem.len,  msg->mem.type);
		return;
	}

	mtk_vcodec_debug(vcu, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %d\n",
		msg->mem.va, msg->mem.pa, msg->mem.iova, msg->mem.len,  msg->mem.type);

	inst = container_of(vcu, struct venc_inst, vcu_inst);
	dev = get_dev_by_mem_type(inst, &msg->mem);
	msg->status = mtk_vcodec_free_mem(&msg->mem, dev, tmp->attach, tmp->sgt);
	kfree(tmp);

	if (msg->status)
		mtk_vcodec_err(vcu, "fail %d, va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			msg->status, msg->mem.va, msg->mem.pa,
			msg->mem.iova, msg->mem.len,  msg->mem.type);

}

static void handle_enc_waitisr_msg(struct venc_vcu_inst *vcu,
	void *data, uint32_t timeout)
{
	struct venc_vcu_ipi_msg_waitisr *msg = data;
	struct mtk_vcodec_ctx *ctx = vcu->ctx;

	msg->irq_status = ctx->irq_status;
	msg->timeout = timeout;
}
static int check_codec_id(struct venc_vcu_ipi_msg_common *msg, unsigned int fmt)
{
	int codec_id = 0, ret = 0;

	switch (fmt) {
	case V4L2_PIX_FMT_H264:
		codec_id = VENC_H264;
		break;
	case V4L2_PIX_FMT_VP8:
		codec_id = VENC_VP8;
		break;
	case V4L2_PIX_FMT_MPEG4:
		codec_id = VENC_MPEG4;
		break;
	case V4L2_PIX_FMT_H263:
		codec_id = VENC_H263;
		break;
	case V4L2_PIX_FMT_HEVC:
		codec_id = VENC_H265;
		break;
	case V4L2_PIX_FMT_HEIF:
		codec_id = VENC_HEIF;
		break;
	default:
		pr_info("%s fourcc not supported", __func__);
		break;
	}

	if (codec_id == 0) {
		mtk_v4l2_err("[error] venc unsupported fourcc\n");
		ret = -1;
	} else if (msg->codec_id == codec_id) {
		pr_info("%s ipi id %d is correct\n", __func__, msg->codec_id);
		ret = 0;
	} else {
		mtk_v4l2_debug(2, "[Info] ipi id %d is incorrect\n", msg->codec_id);
		ret = -1;
	}
	return ret;
}

static int handle_enc_get_bs_buf(struct venc_vcu_inst *vcu, void *data)
{
	struct mtk_vcodec_mem *pbs_buf;
	struct mtk_vcodec_ctx *ctx = vcu->ctx;
	struct venc_vsi *vsi = (struct venc_vsi *)vcu->vsi;
	struct venc_vcu_ipi_msg_get_bs *msg = (struct venc_vcu_ipi_msg_get_bs *)data;
	long timeout_jiff;

	pbs_buf =  mtk_vcodec_get_bs(ctx);
	timeout_jiff = msecs_to_jiffies(1000);

	while (pbs_buf == NULL) {
		wait_event_interruptible_timeout(
			vcu->ctx->bs_wq,
			v4l2_m2m_num_dst_bufs_ready(vcu->ctx->m2m_ctx) > 0 ||
				vcu->ctx->state == MTK_STATE_FLUSH,
			timeout_jiff);
		pbs_buf = mtk_vcodec_get_bs(ctx);
	}

	vsi->venc.venc_bs_va = (u64)(uintptr_t)pbs_buf;
	msg->bs_addr = pbs_buf->dma_addr;
	msg->bs_size = pbs_buf->size;
	pbs_buf->buf_fd = msg->bs_fd;

	return 1;
}

static void venc_vcp_free_mq_node(struct mtk_vcodec_dev *dev,
	struct mtk_vcodec_msg_node *mq_node)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->mq.lock, flags);
	list_add(&mq_node->list, &dev->mq.nodes);
	spin_unlock_irqrestore(&dev->mq.lock, flags);
}

int vcp_enc_ipi_handler(void *arg)
{
	struct mtk_vcodec_dev *dev = (struct mtk_vcodec_dev *)arg;
	struct share_obj *obj = NULL;
	struct venc_vcu_ipi_msg_common *msg = NULL;
	struct venc_inst *inst = NULL;
	struct venc_vcu_inst *vcu;
	struct venc_vsi *vsi = NULL;
	struct mtk_vcodec_ctx *ctx;
	int ret = 0;
	struct mtk_vcodec_msg_node *mq_node = NULL;
	struct venc_vcu_ipi_mem_op *shem_msg;
	unsigned long flags;
	struct list_head *p, *q;
	struct mtk_vcodec_ctx *temp_ctx;
	int msg_valid = 0;
	struct sched_param sched_p = { .sched_priority = MTK_VCODEC_IPI_THREAD_PRIORITY };

	mtk_v4l2_debug_enter();
	BUILD_BUG_ON(sizeof(struct venc_ap_ipi_msg_init) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct venc_ap_ipi_query_cap) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct venc_ap_ipi_msg_set_param) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct venc_ap_ipi_msg_enc) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct venc_ap_ipi_msg_deinit) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(
		sizeof(struct venc_vcu_ipi_query_cap_ack) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct venc_vcu_ipi_msg_common) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct venc_vcu_ipi_msg_init) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(
		sizeof(struct venc_vcu_ipi_msg_set_param) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct venc_vcu_ipi_msg_enc) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct venc_vcu_ipi_msg_deinit) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(sizeof(struct venc_vcu_ipi_msg_waitisr) > SHARE_BUF_SIZE);
	BUILD_BUG_ON(
		sizeof(struct venc_vcu_ipi_mem_op) > SHARE_BUF_SIZE);

	sched_setscheduler(current, SCHED_FIFO, &sched_p);

	do {
		if (mq_node != NULL)
			venc_vcp_free_mq_node(dev, mq_node);

		mq_node = NULL;
		ret = wait_event_interruptible(dev->mq.wq, atomic_read(&dev->mq.cnt) > 0);
		if (ret < 0) {
			mtk_v4l2_debug(0, "wait event return %d (suspending %d)\n",
				ret, atomic_read(&dev->mq.cnt));
			continue;
		}

		spin_lock_irqsave(&dev->mq.lock, flags);
		mq_node = list_entry(dev->mq.head.next, struct mtk_vcodec_msg_node, list);
		list_del(&(mq_node->list));
		atomic_dec(&dev->mq.cnt);
		spin_unlock_irqrestore(&dev->mq.lock, flags);

		obj = &mq_node->ipi_data;
		msg = (struct venc_vcu_ipi_msg_common *)obj->share_buf;

		if (msg == NULL ||
		   (struct venc_vcu_inst *)(unsigned long)msg->ap_inst_addr == NULL) {
			mtk_v4l2_err(" msg invalid %lx (msg 0x%x)\n",
				(unsigned long)msg, msg ? msg->msg_id : 0);
			continue;
		}

		/* handling VSI (shared memory) preparation when VCP init service without inst*/
		if (msg->msg_id == VCU_IPIMSG_ENC_MEM_ALLOC) {
			shem_msg = (struct venc_vcu_ipi_mem_op *)obj->share_buf;
			if (shem_msg->mem.type == MEM_TYPE_FOR_SHM) {
				struct mtk_ipi_device *ipidev;
				struct venc_common_vsi *venc_com_vsi;

				handle_venc_mem_alloc((void *)shem_msg);
				shem_msg->vcp_addr[0] = (__u32)VCP_PACK_IOVA(
					vcp_get_reserve_mem_phys_ex(VENC_SET_PROP_MEM_ID));
				shem_msg->vcp_addr[1] = (__u32)VCP_PACK_IOVA(
					vcp_get_reserve_mem_phys_ex(VENC_VCP_LOG_INFO_ID));
				dev->com_vsi = (void *)vcp_get_reserve_mem_virt_ex(VENC_MEM_ID);
				venc_com_vsi = (struct venc_common_vsi *)dev->com_vsi;
				dev->tf_info = (struct mtk_tf_info *)&venc_com_vsi->tf_info;
				dev->vio_info = (struct mtk_vio_info *)&venc_com_vsi->vio_info;

				shem_msg->msg_id = AP_IPIMSG_ENC_MEM_ALLOC_DONE;
				ipidev = vcp_get_ipidev(VENC_FEATURE_ID);
				if (!ipidev)
					mtk_v4l2_err("[%d] vcp_get_ipidev(VENC_FEATURE_ID) get NULL", shem_msg->ctx_id);
				else {
					ret = mtk_ipi_send(ipidev, IPI_OUT_VENC_0, IPI_SEND_WAIT, obj,
						PIN_OUT_SIZE_VENC, 100);
					if (ret != IPI_ACTION_DONE)
						mtk_v4l2_err("mtk_ipi_send fail %d", ret);
				}
				continue;
			}
		}

		vcu = (struct venc_vcu_inst *)(unsigned long)msg->ap_inst_addr;

		/* Check IPI inst is valid */
		mutex_lock(&dev->ctx_mutex);
		msg_valid = 0;
		list_for_each_safe(p, q, &dev->ctx_list) {
			temp_ctx = list_entry(p, struct mtk_vcodec_ctx, list);
			inst = (struct venc_inst *)temp_ctx->drv_handle;
			if (inst != NULL && vcu == &inst->vcu_inst && vcu->ctx == temp_ctx) {
				msg_valid = 1;
				ctx = vcu->ctx;
				mutex_lock(&ctx->ipi_use_lock);
				break;
			}
		}
		if (!msg_valid) {
			if (vcu) {
				inst = container_of(vcu, struct venc_inst, vcu_inst);
				ctx = vcu->ctx;
			} else {
				inst = NULL;
				ctx = NULL;
			}
			mtk_v4l2_err(" msg msg_id %X vcu not exist 0x%lx (ctx 0x%lx, inst 0x%lx)\n", msg->msg_id,
				(unsigned long)vcu, (unsigned long)ctx, (unsigned long)inst);
			mtk_vcodec_dump_ctx_list(dev, 0);
			mutex_unlock(&dev->ctx_mutex);
			continue;
		}
		mutex_unlock(&dev->ctx_mutex);

		if (vcu->abort || vcu->daemon_pid != vcp_cmd_ex(VENC_FEATURE_ID, VCP_GET_GEN, "venc_srv")) {
			mtk_vcodec_err(vcu, " msg msg_id %X vcu abort %d %d\n",
				msg->msg_id, vcu->daemon_pid, vcp_cmd_ex(VENC_FEATURE_ID, VCP_GET_GEN, "venc_srv"));
			mutex_unlock(&ctx->ipi_use_lock);
			continue;
		}
		inst = container_of(vcu, struct venc_inst, vcu_inst);

		mtk_v4l2_debug(2, "[%d] pop msg_id %X, ml_cnt %d, vcu %lx, status %d", vcu->ctx->id,
			msg->msg_id, atomic_read(&dev->mq.cnt), (unsigned long)vcu, msg->status);

		msg->ctx_id = ctx->id;
		vsi = (struct venc_vsi *)vcu->vsi;
		switch (msg->msg_id) {
		case VCU_IPIMSG_ENC_INIT_DONE:
			handle_enc_init_msg(dev, vcu, (void *)obj->share_buf);
			if (msg->status != VENC_IPI_MSG_STATUS_OK)
				vcu->failure = msg->status;
			else
				mtk_vcodec_set_state_from(ctx, MTK_STATE_INIT, MTK_STATE_FREE);
			goto return_venc_ipi_ack;
		case VCU_IPIMSG_ENC_PWR_CTRL_DONE: {
			struct venc_ap_ipi_pwr_ctrl *ack_msg =
				(struct venc_ap_ipi_pwr_ctrl *)obj->share_buf;
			struct mtk_smi_pwr_ctrl_info *ctrl_info =
				(struct mtk_smi_pwr_ctrl_info *)ack_msg->ap_data_addr;

			ctrl_info->ret = ack_msg->info.ret;
			goto return_venc_ipi_ack;
		}
		case VCU_IPIMSG_ENC_SET_PARAM_DONE:
		case VCU_IPIMSG_ENC_ENCODE_DONE:
		case VCU_IPIMSG_ENC_DEINIT_DONE:
		case VCU_IPIMSG_ENC_BACKUP_DONE:
		case VCU_IPIMSG_ENC_RESUME_DONE:
		case VCU_IPIMSG_ENC_SET_CONFIG_DONE:
return_venc_ipi_ack:
			vcu->signaled = true;
			wake_up(&vcu->wq_hd);
			break;
		case VCU_IPIMSG_ENC_QUERY_CAP_DONE:
			handle_query_cap_ack_msg((void *)obj->share_buf);
			vcu->signaled = true;
			wake_up(&vcu->wq_hd);
			break;
		case VCU_IPIMSG_ENC_PUT_BUFFER:
			mtk_enc_put_buf(ctx);
			msg->msg_id = AP_IPIMSG_ENC_PUT_BUFFER_DONE;
			venc_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
			break;
		case VCU_IPIMSG_ENC_MEM_ALLOC:
			handle_venc_mem_alloc((void *)obj->share_buf);
			msg->msg_id = AP_IPIMSG_ENC_MEM_ALLOC_DONE;
			venc_vcp_ipi_send(inst, msg, sizeof(struct venc_vcu_ipi_mem_op),
				true, false, false);
			break;
		case VCU_IPIMSG_ENC_MEM_FREE:
			handle_venc_mem_free((void *)obj->share_buf);
			msg->msg_id = AP_IPIMSG_ENC_MEM_FREE_DONE;
			venc_vcp_ipi_send(inst, msg, sizeof(struct venc_vcu_ipi_mem_op),
				true, false, false);
			break;
		// TODO: need remove HW locks /power ipis
		case VCU_IPIMSG_ENC_WAIT_ISR:
			if (msg->status == MTK_VENC_CORE_0)
				vcodec_trace_count("VENC_HW_CORE_0", 2);
			else
				vcodec_trace_count("VENC_HW_CORE_1", 2);

			if (-1 == mtk_vcodec_wait_for_done_ctx(ctx, msg->status,
				MTK_INST_IRQ_RECEIVED,
				WAIT_INTR_TIMEOUT_MS)) {
				handle_enc_waitisr_msg(vcu, msg, 1);
				mtk_vcodec_debug(vcu,
					"irq_status %x <-", ctx->irq_status);
			} else
				handle_enc_waitisr_msg(vcu, msg, 0);

			if (msg->status == MTK_VENC_CORE_0)
				vcodec_trace_count("VENC_HW_CORE_0", 1);
			else
				vcodec_trace_count("VENC_HW_CORE_1", 1);

			msg->msg_id = AP_IPIMSG_ENC_WAIT_ISR_DONE;
			venc_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
			break;
		case VCU_IPIMSG_ENC_POWER_ON:
			venc_lock(ctx, msg->status, 0);
			ctx->sysram_enable = vsi->config.sysram_enable;
			venc_encode_prepare(ctx, msg->status, &flags);
			msg->msg_id = AP_IPIMSG_ENC_POWER_ON_DONE;
			venc_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
			break;
		case VCU_IPIMSG_ENC_POWER_OFF:
			ctx->sysram_enable = vsi->config.sysram_enable;
			venc_encode_unprepare(ctx, msg->status, &flags);
			venc_unlock(ctx, msg->status);
			msg->msg_id = AP_IPIMSG_ENC_POWER_OFF_DONE;
			venc_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
			break;
		case VCU_IPIMSG_ENC_TRACE:
		{
			struct venc_vcu_ipi_msg_trace *trace_msg =
				(struct venc_vcu_ipi_msg_trace *)obj->share_buf;
			char buf[16];

			SNPRINTF(buf, 16, "VENC_TRACE_%d", trace_msg->trace_id);
			vcodec_trace_count(buf, trace_msg->flag);
		}
			break;
		case VCU_IPIMSG_ENC_CHECK_CODEC_ID:
		{
			if (check_codec_id(msg, ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc) == 0)
				msg->status = 0;
			else
				msg->status = -1;

			msg->msg_id = AP_IPIMSG_ENC_CHECK_CODEC_ID_DONE;
			venc_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
		}
			break;
		case VCU_IPIMSG_ENC_GET_BS_BUFFER:
		{
			handle_enc_get_bs_buf(vcu, (void *)obj->share_buf);
			msg->msg_id = AP_IPIMSG_ENC_GET_BS_BUFFER_DONE;
			venc_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
		}
			break;
		case VCU_IPIMSG_ENC_SMI_BUS_DUMP:
		{
			atomic_inc(&dev->smi_dump_ref_cnt);
			if (msg->status == MTK_VENC_POWERCTL_IN_VCP)
				mtk_smi_dbg_dump_for_venc();
			else
				mtk_smi_dbg_hang_detect("venc timeout");
			atomic_dec(&dev->smi_dump_ref_cnt);

			msg->msg_id = AP_IPIMSG_ENC_SMI_BUS_DUMP_DONE;
			venc_vcp_ipi_send(inst, msg, sizeof(*msg), true, false, false);
		}
			break;
		default:
			mtk_vcodec_err(vcu, "unknown msg id %x", msg->msg_id);
			break;
		}
		mtk_vcodec_debug(vcu, "- id=%X", msg->msg_id);
		mutex_unlock(&ctx->ipi_use_lock);
	} while (!kthread_should_stop());
	mtk_v4l2_debug_leave();

	return ret;
}

static int venc_vcp_ipi_isr(unsigned int id, void *prdata, void *data, unsigned int len)
{
	struct mtk_vcodec_dev *dev = (struct mtk_vcodec_dev *)prdata;
	struct venc_vcu_ipi_msg_common *msg = NULL;
	struct share_obj *obj = (struct share_obj *)data;
	struct mtk_vcodec_msg_node *mq_node;
	unsigned long flags;

	msg = (struct venc_vcu_ipi_msg_common *)obj->share_buf;

	// add to ipi msg list
	spin_lock_irqsave(&dev->mq.lock, flags);
	if (!list_empty(&dev->mq.nodes)) {
		mq_node = list_entry(dev->mq.nodes.next, struct mtk_vcodec_msg_node, list);
		memcpy(&mq_node->ipi_data, obj, sizeof(struct share_obj));
		list_move_tail(&mq_node->list, &dev->mq.head);
		atomic_inc(&dev->mq.cnt);
		spin_unlock_irqrestore(&dev->mq.lock, flags);
		mtk_v4l2_debug(8, "push ipi_id %x msg_id %x, ml_cnt %d",
			obj->id, msg->msg_id, atomic_read(&dev->mq.cnt));
		wake_up(&dev->mq.wq);
	} else {
		spin_unlock_irqrestore(&dev->mq.lock, flags);
		mtk_v4l2_err("mq no free nodes\n");
	}

	return 0;
}

static void venc_vcp_set_vcu(struct venc_vcu_inst *vcu)
{
	vcu->daemon_pid = vcp_cmd_ex(VENC_FEATURE_ID, VCP_GET_GEN, "venc_srv");
	if (vcu->ctx == vcu->ctx->dev_ctx)
		vcu->abort = false;
}

static int venc_vcp_backup(struct venc_inst *inst)
{
	struct venc_ap_ipi_msg_indp msg;
	int err = 0;

	if (!inst)
		return -EINVAL;

	mtk_vcodec_debug_enter(inst);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_ENC_BACKUP;
	msg.ap_inst_addr = (uintptr_t)&inst->vcu_inst;
	msg.ctx_id = inst->ctx->id;
	venc_vcp_set_vcu(&inst->vcu_inst);
	mtk_v4l2_debug(0, "[VDVFS] VENC suspend");
	err = venc_vcp_ipi_send(inst, &msg, sizeof(msg), false, false, false);

	mtk_vcodec_debug(inst, "- ret=%d", err);

	return err;
}

static int venc_vcp_resume(struct venc_inst *inst)
{
	struct venc_ap_ipi_msg_indp msg;
	int err = 0;

	if (!inst)
		return -EINVAL;

	mtk_vcodec_debug_enter(inst);

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_ENC_RESUME;
	msg.ap_inst_addr = (uintptr_t)&inst->vcu_inst;
	msg.ctx_id = inst->ctx->id;
	venc_vcp_set_vcu(&inst->vcu_inst);
	mtk_v4l2_debug(0, "[VDVFS] VENC resume");
	err = venc_vcp_ipi_send(inst, &msg, sizeof(msg), false, false, false);
	mtk_vcodec_debug(inst, "- ret=%d", err);

	return err;
}

static bool has_valid_vcp_inst(struct mtk_vcodec_dev *dev)
{
	struct mtk_vcodec_ctx *ctx;
	struct venc_inst *inst;
	int curr_daemon_pid = vcp_cmd_ex(VENC_FEATURE_ID, VCP_GET_GEN, "venc_srv");

	list_for_each_entry(ctx, &dev->ctx_list, list) {
		if (ctx != NULL && ctx->drv_handle != 0 && ctx != &dev->dev_ctx) {
			inst = (struct venc_inst *)ctx->drv_handle;
			if (inst->vcu_inst.init_done && !inst->vcu_inst.abort &&
			    inst->vcu_inst.daemon_pid == curr_daemon_pid)
				return true;
		}
	}
	return false;
}

static int vcp_venc_notify_callback(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	struct mtk_vcodec_dev *dev;
	struct list_head *p, *q;
	struct mtk_vcodec_ctx *ctx;
	int timeout = 0;
	struct venc_inst *inst = NULL;
	bool need_backup;

	if (!mtk_vcodec_is_vcp(MTK_INST_ENCODER))
		return 0;

	dev = container_of(this, struct mtk_vcodec_dev, vcp_notify);

	switch (event) {
	case VCP_EVENT_STOP:
		timeout = 0;
		while (atomic_read(&dev->mq.cnt)) {
			timeout++;
			mdelay(1);
			if (timeout > VCP_SYNC_TIMEOUT_MS) {
				mtk_v4l2_debug(0, "VCP_EVENT_STOP timeout\n");
				break;
			}
		}
		mutex_lock(&dev->ctx_mutex);
		// check release all ctx lock
		list_for_each_safe(p, q, &dev->ctx_list) {
			ctx = list_entry(p, struct mtk_vcodec_ctx, list);
			if (ctx != NULL && !mtk_vcodec_is_state(ctx, MTK_STATE_ABORT)) {
				inst = (struct venc_inst *)(ctx->drv_handle);
				if (inst != NULL) {
					inst->vcu_inst.failure = VENC_IPI_MSG_STATUS_FAIL;
					inst->vcu_inst.abort = 1;
					if (inst->vcu_inst.in_ipi) {
						inst->vcu_inst.signaled = true;
						wake_up(&inst->vcu_inst.wq_hd);
					}
				}
				if (ctx == ctx->dev_ctx)
					continue;
				mtk_vcodec_set_state(ctx, MTK_STATE_ABORT);
				venc_check_release_lock(ctx);
				mtk_venc_queue_error_event(ctx);
			}
		}
		mutex_unlock(&dev->ctx_mutex);
	break;
	case VCP_EVENT_SUSPEND:
		dev->is_codec_suspending = 1;
		// check no more ipi in progress
		mutex_lock(&dev->ipi_mutex);
		mutex_unlock(&dev->ipi_mutex);

		// send backup ipi to vcp by dev_ctx if vcp has inst
		mutex_lock(&dev->ctx_mutex);
		need_backup = has_valid_vcp_inst(dev);
		mutex_unlock(&dev->ctx_mutex);
		if (need_backup) {
			venc_vcp_backup((struct venc_inst *)dev->dev_ctx.drv_handle);
			dev->has_backup = true;
		}

		while (atomic_read(&dev->mq.cnt)) {
			timeout += 20;
			usleep_range(10000, 20000);
			if (timeout > VCP_SYNC_TIMEOUT_MS) {
				mtk_v4l2_debug(0, "VCP_EVENT_SUSPEND timeout\n");
				break;
			}
		}
	break;
	case VCP_EVENT_RESUME:
		// send backup ipi to vcp by dev_ctx if vcp has inst
		if (dev->has_backup) {
			venc_vcp_resume((struct venc_inst *)dev->dev_ctx.drv_handle);
			dev->has_backup = false;
		}
		dev->is_codec_suspending = 0;
		break;
	}
	return NOTIFY_DONE;
}

void venc_vcp_probe(struct mtk_vcodec_dev *dev)
{
	int ret, i;
	struct mtk_vcodec_msg_node *mq_node;
	struct mtk_ipi_device *ipidev;

	mtk_v4l2_debug_enter();
	INIT_LIST_HEAD(&dev->mq.head);
	spin_lock_init(&dev->mq.lock);
	init_waitqueue_head(&dev->mq.wq);
	atomic_set(&dev->mq.cnt, 0);

	INIT_LIST_HEAD(&dev->mq.nodes);
	for (i = 0; i < MTK_VCODEC_MAX_MQ_NODE_CNT; i++) {
		mq_node = kmalloc(sizeof(struct mtk_vcodec_msg_node), GFP_DMA | GFP_ATOMIC);
		list_add(&mq_node->list, &dev->mq.nodes);
	}

	if (!VCU_FPTR(vcu_load_firmware))
		mtk_vcodec_vcp |= 1 << MTK_INST_ENCODER;

	ipidev = vcp_get_ipidev(VENC_FEATURE_ID);
	if (!ipidev)
		mtk_v4l2_err("vcp_get_ipidev(VENC_FEATURE_ID) get NULL, can't register ipi");
	else {
		ret = mtk_ipi_register(ipidev, IPI_IN_VENC_0, venc_vcp_ipi_isr, dev, &dev->enc_ipi_data);
		if (ret)
			mtk_v4l2_debug(0, " ipi_register, ret %d\n", ret);
	}

	kthread_run(vcp_enc_ipi_handler, dev, "venc_ipi_recv");

	dev->vcp_notify.notifier_call = vcp_venc_notify_callback;
	dev->vcp_notify.priority = 1;
	vcp_A_register_notify_ex(VENC_FEATURE_ID, &dev->vcp_notify);

	mtk_v4l2_debug_leave();
}

void venc_vcp_remove(struct mtk_vcodec_dev *dev)
{
	int timeout = 0;
	struct mtk_vcodec_msg_node *mq_node, *next;
	unsigned long flags;

	while (atomic_read(&dev->mq.cnt)) {
		timeout++;
		mdelay(1);
		if (timeout > VCP_SYNC_TIMEOUT_MS) {
			mtk_v4l2_err("wait msgq empty timeout\n");
			break;
		}
	}

	spin_lock_irqsave(&dev->mq.lock, flags);
	list_for_each_entry_safe(mq_node, next, &dev->mq.nodes, list) {
		list_del(&(mq_node->list));
		kfree(mq_node);
	}
	spin_unlock_irqrestore(&dev->mq.lock, flags);
}

static unsigned int venc_h265_get_profile(struct venc_inst *inst,
	unsigned int profile)
{
	switch (profile) {
	case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN:
		return 1;
	case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10:
		return 2;
	case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_STILL_PICTURE:
		return 4;
	default:
		mtk_vcodec_debug(inst, "unsupported profile %d", profile);
		return 1;
	}
}

static unsigned int venc_h265_get_level(struct venc_inst *inst,
	unsigned int level, unsigned int tier)
{
	switch (level) {
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_1:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 2 : 3;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_2:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 8 : 9;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_2_1:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 10 : 11;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_3:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 13 : 14;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_3_1:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 15 : 16;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_4:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 18 : 19;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_4_1:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 20 : 21;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_5:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 23 : 24;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_5_1:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 25 : 26;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_5_2:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 27 : 28;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_6:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 29 : 30;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_6_1:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 31 : 32;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_6_2:
		return (tier == V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) ? 33 : 34;
	default:
		mtk_vcodec_debug(inst, "unsupported level %d", level);
		return 25;
	}
}

static unsigned int venc_mpeg4_get_profile(struct venc_inst *inst,
	unsigned int profile)
{
	switch (profile) {
	case V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE:
		return 0;
	case V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_SIMPLE:
		return 1;
	case V4L2_MPEG_VIDEO_MPEG4_PROFILE_CORE:
		return 2;
	case V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE_SCALABLE:
		return 3;
	case V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_CODING_EFFICIENCY:
		return 4;
	default:
		mtk_vcodec_debug(inst, "unsupported mpeg4 profile %d", profile);
		return 100;
	}
}

static unsigned int venc_mpeg4_get_level(struct venc_inst *inst,
	unsigned int level)
{
	switch (level) {
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_0:
		return 0;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_0B:
		return 1;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_1:
		return 2;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_2:
		return 3;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_3:
		return 4;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_3B:
		return 5;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_4:
		return 6;
	case V4L2_MPEG_VIDEO_MPEG4_LEVEL_5:
		return 7;
	default:
		mtk_vcodec_debug(inst, "unsupported mpeg4 level %d", level);
		return 4;
	}
}

int vcp_enc_encode(struct venc_inst *inst, unsigned int bs_mode,
				   struct venc_frm_buf *frm_buf,
				   struct mtk_vcodec_mem *bs_buf,
				   unsigned int *bs_size)
{

	struct venc_ap_ipi_msg_enc out;
	struct venc_ap_ipi_msg_set_param out_slb;
	struct venc_vsi *vsi = (struct venc_vsi *)inst->vcu_inst.vsi;
	unsigned int i, ret, ret_slb;

	mtk_vcodec_debug(inst, "bs_mode %d ->", bs_mode);

	if (sizeof(out) > SHARE_BUF_SIZE) {
		mtk_vcodec_err(inst, "venc_ap_ipi_msg_enc cannot be large than %d sizeof(out):%zu",
					   SHARE_BUF_SIZE, sizeof(out));
		return -EINVAL;
	}

	memset(&out, 0, sizeof(out));
	out.msg_id = AP_IPIMSG_ENC_ENCODE;
	out.vcu_inst_addr = inst->vcu_inst.inst_addr;
	out.ctx_id = inst->ctx->id;
	out.bs_mode = bs_mode;
	if (frm_buf) {
		out.fb_num_planes = frm_buf->num_planes;
		for (i = 0; i < frm_buf->num_planes; i++) {
			vsi->venc.input_addr[i] =
				frm_buf->fb_addr[i].dma_addr;
			vsi->venc.fb_dma[i] =
				frm_buf->fb_addr[i].dma_addr;
			out.input_size[i] =
				frm_buf->fb_addr[i].size;
			out.data_offset[i] =
				frm_buf->fb_addr[i].data_offset;
		}
		if (frm_buf->has_meta) {
			vsi->meta_size = sizeof(struct mtk_hdr_dynamic_info);
			vsi->meta_addr = frm_buf->meta_addr;
		} else {
			vsi->meta_size = 0;
			vsi->meta_addr = 0;
		}

		if (frm_buf->has_qpmap) {
			vsi->qpmap_addr = frm_buf->qpmap_dma_addr;
			vsi->qpmap_size = frm_buf->qpmap_dma->size;
		} else {
			vsi->qpmap_addr = 0;
			vsi->qpmap_size = 0;
		}

		if (frm_buf->dyparams_dma) {
			vsi->dynamicparams_addr = frm_buf->dyparams_dma_addr;
			vsi->dynamicparams_size = sizeof(struct inputqueue_dynamic_info);
			vsi->dynamicparams_offset = frm_buf->dyparams_offset;
			mtk_vcodec_debug(inst, "vsi dynamic params addr %llx size%d offset:%d",
				vsi->dynamicparams_addr,
				vsi->dynamicparams_size,
				vsi->dynamicparams_offset);
		} else {
			vsi->dynamicparams_addr = 0;
			vsi->dynamicparams_size = 0;
		}

		mtk_vcodec_debug(inst, " num_planes = %d input (dmabuf:%lx), size %d %llx",
			frm_buf->num_planes,
			(unsigned long)frm_buf->fb_addr[0].dmabuf,
			vsi->meta_size,
			vsi->meta_addr);
		mtk_vcodec_debug(inst, "vsi qpmap addr %llx size%d",
			vsi->qpmap_addr, vsi->qpmap_size);
	}

	if (bs_buf) {
		vsi->venc.bs_addr = bs_buf->dma_addr;
		vsi->venc.bs_dma = bs_buf->dma_addr;
		out.bs_size = bs_buf->size;

		if (bs_buf->dma_general_buf != 0) {
			vsi->general_buf_fd = bs_buf->general_buf_fd;
			vsi->general_buf_size = bs_buf->dma_general_buf->size;
			vsi->general_buf_dma = bs_buf->dma_general_addr;
			mtk_vcodec_debug(inst, "dma_general_buf=%p general_buf_fd=%d general_buf_dma=%llx size=%lu",
			    bs_buf->dma_general_buf, inst->vsi->general_buf_fd,
			    vsi->general_buf_dma,
			    bs_buf->dma_general_buf->size);
		} else {
			bs_buf->general_buf_fd = -1;
			vsi->general_buf_fd = -1;
			vsi->general_buf_size = 0;
			mtk_vcodec_debug(inst, "no general buf dmabuf");
		}

		mtk_vcodec_debug(inst, " output (dma:%lx)",
			(unsigned long)bs_buf->dmabuf);
	}

	if (inst->ctx->use_slbc && atomic_read(&mtk_venc_slb_cb.release_slbc)) {
		memset(&out_slb, 0, sizeof(out_slb));
		out_slb.msg_id = AP_IPIMSG_ENC_SET_PARAM;
		out_slb.vcu_inst_addr = inst->vcu_inst.inst_addr;
		out_slb.ctx_id = inst->ctx->id;
		out_slb.param_id = VENC_SET_PARAM_RELEASE_SLB;
		out_slb.data_item = 2;
		out_slb.data[0] = 1; //release_slb 1
		out_slb.data[1] = 0x0; //slbc_addr
		ret_slb = venc_vcp_ipi_send(inst, &out_slb, sizeof(out_slb), false, true, false);

		if (ret_slb)
			mtk_vcodec_err(inst, "set VENC_SET_PARAM_RELEASE_SLB fail %d", ret_slb);
		else {
			mtk_v4l2_debug(0, "slbc_release, %p\n", &inst->ctx->sram_data);
			slbc_release(&inst->ctx->sram_data);
			inst->ctx->use_slbc = 0;
			atomic_inc(&mtk_venc_slb_cb.later_cnt);
			inst->ctx->later_cnt_once = true;
			if (inst->ctx->enc_params.slbc_encode_performance)
				atomic_dec(&mtk_venc_slb_cb.perf_used_cnt);

			mtk_v4l2_debug(0, "slbc_release ref %d\n", inst->ctx->sram_data.ref);
			if (inst->ctx->sram_data.ref <= 0)
				atomic_set(&mtk_venc_slb_cb.release_slbc, 0);
		}

		mtk_v4l2_debug(0, "slb_cb %d/%d perf %d cnt %d/%d/%d",
			atomic_read(&mtk_venc_slb_cb.release_slbc),
			atomic_read(&mtk_venc_slb_cb.request_slbc),
			inst->ctx->enc_params.slbc_encode_performance,
			atomic_read(&mtk_venc_slb_cb.perf_used_cnt),
			atomic_read(&mtk_venc_slb_cb.later_cnt),
			inst->ctx->later_cnt_once);
	} else if (!inst->ctx->use_slbc && atomic_read(&mtk_venc_slb_cb.request_slbc) &&
		!inst->ctx->enc_params.slbc_cpu_used_performance) {
		if (slbc_request(&inst->ctx->sram_data) >= 0) {
			inst->ctx->use_slbc = 1;
			inst->ctx->slbc_addr = (unsigned int)(unsigned long)
				inst->ctx->sram_data.paddr;
		} else {
			mtk_vcodec_err(inst, "slbc_request fail\n");
			inst->ctx->use_slbc = 0;
		}
		if (inst->ctx->slbc_addr % 256 != 0 || inst->ctx->slbc_addr == 0) {
			mtk_vcodec_err(inst, "slbc_addr error 0x%x\n", inst->ctx->slbc_addr);
			inst->ctx->use_slbc = 0;
		}

		if (inst->ctx->use_slbc == 1) {
			if (inst->ctx->enc_params.slbc_encode_performance)
				atomic_inc(&mtk_venc_slb_cb.perf_used_cnt);

			atomic_dec(&mtk_venc_slb_cb.later_cnt);
			inst->ctx->later_cnt_once = false;
			if (atomic_read(&mtk_venc_slb_cb.later_cnt) <= 0)
				atomic_set(&mtk_venc_slb_cb.request_slbc, 0);

			memset(&out_slb, 0, sizeof(out_slb));
			out_slb.msg_id = AP_IPIMSG_ENC_SET_PARAM;
			out_slb.vcu_inst_addr = inst->vcu_inst.inst_addr;
			out_slb.ctx_id = inst->ctx->id;
			out_slb.param_id = VENC_SET_PARAM_RELEASE_SLB;
			out_slb.data_item = 2;
			out_slb.data[0] = 0; //release_slb 0
			out_slb.data[1] = inst->ctx->slbc_addr;
			ret_slb = venc_vcp_ipi_send(inst, &out_slb, sizeof(out_slb),
				false, true, false);
			if (ret_slb) {
				mtk_vcodec_err(inst, "set VENC_SET_PARAM_RELEASE_SLB fail %d",
					ret_slb);
			}
		}

		mtk_v4l2_debug(0, "slbc_request %d, 0x%x, 0x%lx\n",
			inst->ctx->use_slbc, inst->ctx->slbc_addr,
			(unsigned long)inst->ctx->sram_data.paddr);
		mtk_v4l2_debug(0, "slb_cb %d/%d perf %d cnt %d/%d/%d",
			atomic_read(&mtk_venc_slb_cb.release_slbc),
			atomic_read(&mtk_venc_slb_cb.request_slbc),
			inst->ctx->enc_params.slbc_encode_performance,
			atomic_read(&mtk_venc_slb_cb.perf_used_cnt),
			atomic_read(&mtk_venc_slb_cb.later_cnt),
			inst->ctx->later_cnt_once);
	}

	ret = venc_vcp_ipi_send(inst, &out, sizeof(out), false, true, false);
	if (ret) {
		mtk_vcodec_err(inst, "AP_IPIMSG_ENC_ENCODE %d fail %d",
					   bs_mode, ret);
		return ret;
	}

	mtk_vcodec_debug(inst, "bs_mode %d size %d key_frm %d <-",
		bs_mode, inst->vcu_inst.bs_size, inst->vcu_inst.is_key_frm);

	return 0;
}


static int venc_encode_header(struct venc_inst *inst,
	struct mtk_vcodec_mem *bs_buf,
	unsigned int *bs_size)
{
	int ret = 0;

	mtk_vcodec_debug_enter(inst);
	if (bs_buf == NULL)
		inst->vsi->venc.venc_bs_va = 0;
	else
		inst->vsi->venc.venc_bs_va = (u64)(uintptr_t)bs_buf;

	inst->vsi->venc.venc_fb_va = 0;

	mtk_vcodec_debug(inst, "vsi venc_bs_va 0x%llx",
			 inst->vsi->venc.venc_bs_va);

	ret = vcp_enc_encode(inst, VENC_BS_MODE_SEQ_HDR, NULL,
						 bs_buf, bs_size);

	return ret;
}

static int venc_encode_frame(struct venc_inst *inst,
	struct venc_frm_buf *frm_buf,
	struct mtk_vcodec_mem *bs_buf,
	unsigned int *bs_size)
{
	int ret = 0;
	unsigned int fm_fourcc = inst->ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
	unsigned int bs_fourcc = inst->ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc;

	mtk_vcodec_debug_enter(inst);

	if (bs_buf == NULL)
		inst->vsi->venc.venc_bs_va = 0;
	else
		inst->vsi->venc.venc_bs_va = (u64)(uintptr_t)bs_buf;

	if (frm_buf == NULL)
		inst->vsi->venc.venc_fb_va = 0;
	else {
		inst->vsi->venc.venc_fb_va = (u64)(uintptr_t)frm_buf;
		inst->vsi->venc.timestamp = frm_buf->timestamp;
	}
	ret = vcp_enc_encode(inst, VENC_BS_MODE_FRAME, frm_buf,
						 bs_buf, bs_size);
	if (ret)
		return ret;

	++inst->frm_cnt;
	mtk_vcodec_debug(inst, "Format: frame_va %llx (%s) bs_va:%llx (%s)",
		inst->vsi->venc.venc_fb_va, FOURCC_STR(fm_fourcc),
		inst->vsi->venc.venc_bs_va, FOURCC_STR(bs_fourcc));

	return ret;
}

static int venc_encode_frame_final(struct venc_inst *inst,
	struct venc_frm_buf *frm_buf,
	struct mtk_vcodec_mem *bs_buf,
	unsigned int *bs_size)
{
	int ret = 0;

	mtk_v4l2_debug(4, "check inst->vsi %p +", inst->vsi);
	if (inst == NULL || inst->vsi == NULL)
		return -EINVAL;

	if (bs_buf == NULL)
		inst->vsi->venc.venc_bs_va = 0;
	else
		inst->vsi->venc.venc_bs_va = (u64)(uintptr_t)bs_buf;
	if (frm_buf == NULL)
		inst->vsi->venc.venc_fb_va = 0;
	else
		inst->vsi->venc.venc_fb_va = (u64)(uintptr_t)frm_buf;

	ret = vcp_enc_encode(inst, VENC_BS_MODE_FRAME_FINAL, frm_buf,
						 bs_buf, bs_size);
	if (ret)
		return ret;

	*bs_size = inst->vcu_inst.bs_size;
	mtk_vcodec_debug(inst, "bs size %d <-", *bs_size);

	return ret;
}


static int venc_vcp_init(struct mtk_vcodec_ctx *ctx, unsigned long *handle)
{
	int ret = 0;
	struct venc_inst *inst;
	struct venc_ap_ipi_msg_init out;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst) {
		*handle = (unsigned long)NULL;
		return -ENOMEM;
	}

	inst->ctx = ctx;
	inst->vcu_inst.ctx = ctx;
	inst->vcu_inst.dev = ctx->dev->vcu_plat_dev;
	inst->vcu_inst.signaled = false;
	inst->vcu_inst.id = IPI_VENC_COMMON;

	inst->hw_base = mtk_vcodec_get_enc_reg_addr(inst->ctx, VENC_SYS);

	mtk_vcodec_debug_enter(inst);
	init_waitqueue_head(&inst->vcu_inst.wq_hd);

	inst->vcu_inst.ctx_ipi_lock = kzalloc(sizeof(struct mutex),
		GFP_KERNEL);
	if (!inst->vcu_inst.ctx_ipi_lock) {
		kfree(inst);
		*handle = (unsigned long)NULL;
		return -ENOMEM;
	}
	mutex_init(inst->vcu_inst.ctx_ipi_lock);
	INIT_LIST_HEAD(&inst->vcu_inst.bufs);

	memset(&out, 0, sizeof(out));
	out.msg_id = AP_IPIMSG_ENC_INIT;
	out.ctx_id = inst->ctx->id;
	out.ap_inst_addr = (unsigned long)&inst->vcu_inst;
	(*handle) = (unsigned long)inst;
	venc_vcp_set_vcu(&inst->vcu_inst);

	mtk_vcodec_add_ctx_list(ctx);

	ret = venc_vcp_ipi_send(inst, &out, sizeof(out), false, true, false);
	inst->vsi = (struct venc_vsi *)inst->vcu_inst.vsi;

	mtk_vcodec_debug_leave(inst);

	if (ret) {
		mtk_vcodec_del_ctx_list(ctx);
		kfree(inst->vcu_inst.ctx_ipi_lock);
		kfree(inst);
		(*handle) = (unsigned long)NULL;
	}

	return ret;
}

static int venc_vcp_encode(unsigned long handle,
					   enum venc_start_opt opt,
					   struct venc_frm_buf *frm_buf,
					   struct mtk_vcodec_mem *bs_buf,
					   struct venc_done_result *result)
{
	int ret = 0;
	struct venc_inst *inst = (struct venc_inst *)handle;

	if (inst == NULL || inst->vsi == NULL)
		return -EINVAL;

	mtk_vcodec_debug(inst, "opt %d ->", opt);

	switch (opt) {
	case VENC_START_OPT_ENCODE_SEQUENCE_HEADER: {
		unsigned int bs_size_hdr = 0;

		ret = venc_encode_header(inst, bs_buf, &bs_size_hdr);
		if (ret)
			goto encode_err;

		result->bs_size = bs_size_hdr;
		result->is_key_frm = false;
		break;
	}

	case VENC_START_OPT_ENCODE_FRAME: {
		/* only run @ worker then send ipi
		 * VPU flush cmd binding ctx & handle
		 * or cause cmd calllback ctx error
		 */
		ret = venc_encode_frame(inst, frm_buf, bs_buf,
			&result->bs_size);
		if (ret)
			goto encode_err;
		result->is_key_frm = inst->vcu_inst.is_key_frm;
		break;
	}

	case VENC_START_OPT_ENCODE_FRAME_FINAL: {
		ret = venc_encode_frame_final(inst,
			frm_buf, bs_buf, &result->bs_size);
		if (ret)
			goto encode_err;
		result->is_key_frm = inst->vcu_inst.is_key_frm;
		break;
	}

	default:
		mtk_vcodec_err(inst, "venc_start_opt %d not supported", opt);
		ret = -EINVAL;
		break;
	}

encode_err:
	mtk_vcodec_debug(inst, "opt %d <-", opt);

	return ret;
}

static int venc_vcp_set_pwr_ctrl(struct venc_inst *inst, struct mtk_smi_pwr_ctrl_info *ctrl_info)
{
	struct venc_ap_ipi_pwr_ctrl msg = {0};

	if (ctrl_info->type == MTK_SMI_GET_IF_IN_USE && !has_valid_vcp_inst(inst->ctx->dev)) {
		ctrl_info->ret = 0;
		return 0;
	}

	msg.msg_id = AP_IPIMSG_ENC_PWR_CTRL;
	msg.ctx_id = inst->ctx->id;
	msg.ap_inst_addr = (uintptr_t)&inst->vcu_inst;
	msg.ap_data_addr = (uintptr_t)ctrl_info;
	msg.info.type = ctrl_info->type;
	msg.info.hw_id = ctrl_info->hw_id;
	venc_vcp_set_vcu(&inst->vcu_inst);

	return venc_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
}

static void venc_get_free_buffers(struct venc_inst *inst,
			     struct ring_input_list *list,
			     struct venc_done_result *pResult)
{
	if (list->count < 0 || list->count >= VENC_MAX_FB_NUM) {
		mtk_vcodec_err(inst, "list count %d invalid ! (write_idx %d, read_idx %d)",
			list->count, list->write_idx, list->read_idx);
		if (list->write_idx < 0 || list->write_idx >= VENC_MAX_FB_NUM ||
		    list->read_idx < 0  || list->read_idx >= VENC_MAX_FB_NUM)
			list->write_idx = list->read_idx = 0;
		if (list->write_idx >= list->read_idx)
			list->count = list->write_idx - list->read_idx;
		else
			list->count = list->write_idx + VENC_MAX_FB_NUM - list->read_idx;
	}
	if (list->count == 0) {
		mtk_vcodec_debug(inst, "[FB] there is no free buffers");
		pResult->bs_va = 0;
		pResult->frm_va = 0;
		pResult->is_key_frm = false;
		pResult->bs_size = 0;
		return;
	}

	pResult->bs_size = list->bs_size[list->read_idx];
	pResult->is_key_frm = list->is_key_frm[list->read_idx];
	pResult->bs_va = list->venc_bs_va_list[list->read_idx];
	pResult->frm_va = list->venc_fb_va_list[list->read_idx];
	pResult->is_last_slc = list->is_last_slice[list->read_idx];
	pResult->flags = list->flags[list->read_idx];

	mtk_vcodec_debug(inst, "bsva %lx frva %lx bssize %d iskey %d is_last_slc=%d flags 0x%x",
		pResult->bs_va,
		pResult->frm_va,
		pResult->bs_size,
		pResult->is_key_frm,
		pResult->is_last_slc,
		pResult->flags);

	list->read_idx = (list->read_idx == VENC_MAX_FB_NUM - 1U) ?
			 0U : list->read_idx + 1U;
	list->count--;
}

static void venc_get_resolution_change(struct venc_inst *inst,
			     struct venc_vcu_config *Config,
			     struct venc_resolution_change *pResChange)
{
	pResChange->width = Config->pic_w;
	pResChange->height = Config->pic_h;
	pResChange->framerate = Config->framerate;
	pResChange->resolutionchange = Config->resolutionChange;

	if (Config->resolutionChange)
		Config->resolutionChange = 0;

	mtk_vcodec_debug(inst, "get reschange %d %d %d %d\n",
		 pResChange->width,
		 pResChange->height,
		 pResChange->framerate,
		 pResChange->resolutionchange);
}


static int venc_vcp_get_param(unsigned long handle,
						  enum venc_get_param_type type,
						  void *out)
{
	int ret = 0;
	struct venc_inst *inst = (struct venc_inst *)handle;
	struct venc_ap_ipi_query_cap msg;

	if (inst == NULL)
		return -EINVAL;

	mtk_vcodec_debug(inst, "%s: %d", __func__, type);
	inst->vcu_inst.ctx = inst->ctx;

	switch (type) {
	case GET_PARAM_VENC_CAP_FRAME_SIZES:
	case GET_PARAM_VENC_CAP_SUPPORTED_FORMATS:
	case GET_PARAM_VENC_CAP_COMMON:
		memset(&msg, 0, sizeof(msg));
		msg.msg_id = AP_IPIMSG_ENC_QUERY_CAP;
		msg.id = type;
		msg.ap_inst_addr = (uintptr_t)&inst->vcu_inst;
		msg.ap_data_addr = (uintptr_t)out;
		msg.ctx_id = inst->ctx->id;
		venc_vcp_set_vcu(&inst->vcu_inst);
		ret = venc_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
		break;
	case GET_PARAM_VENC_PWR_CTRL:
		ret = venc_vcp_set_pwr_ctrl(inst, (struct mtk_smi_pwr_ctrl_info *)out);
		break;
	case GET_PARAM_FREE_BUFFERS:
		if (inst->vsi == NULL)
			return -EINVAL;
		venc_get_free_buffers(inst, &inst->vsi->list_free, out);
		break;
	case GET_PARAM_ROI_RC_QP: {
		if (inst->vsi == NULL || out == NULL)
			return -EINVAL;
		*(int *)out = inst->vsi->config.roi_rc_qp;
		break;
	}
	case GET_PARAM_RESOLUTION_CHANGE:
		if (inst->vsi == NULL)
			return -EINVAL;
		venc_get_resolution_change(inst, &inst->vsi->config, out);
		break;
	default:
		mtk_vcodec_err(inst, "invalid get parameter type=%d", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

void set_venc_vcp_data(struct venc_inst *inst, enum vcp_reserve_mem_id_t id, void *string)
{
	//struct venc_ap_ipi_msg_set_param msg;

	void *string_va = (void *)(__u64)vcp_get_reserve_mem_virt_ex(id);
	void *string_pa = (void *)(__u64)vcp_get_reserve_mem_phys_ex(id);
	__u64 mem_size = (__u64)vcp_get_reserve_mem_size_ex(id);
	int string_len = strlen((char *)string);

	mtk_vcodec_debug(inst, "mem_size 0x%llx, string_va 0x%lx, string_pa 0x%lx",
		mem_size, (unsigned long)string_va, (unsigned long)string_pa);
	mtk_vcodec_debug(inst, "string: %s", (char *)string);
	mtk_vcodec_debug(inst, "string_len:%d", string_len);

	if (string_len <= (mem_size-1))
		memcpy(string_va, (char *)string, string_len + 1);
}


static int venc_set_config_data(struct venc_inst *inst)
{
	struct venc_ap_ipi_msg_indp msg;

	memset(&msg, 0, sizeof(msg));
	msg.msg_id = AP_IPIMSG_ENC_SET_CONFIG;
	msg.ap_inst_addr = (uintptr_t)&inst->vcu_inst;
	msg.ctx_id = inst->ctx->id;
	venc_vcp_set_vcu(&inst->vcu_inst);
	mtk_v4l2_debug(0, "venc_set_config_data");

	return venc_vcp_ipi_send(inst, &msg, sizeof(msg), false, true, false);
}


int vcp_enc_set_param(struct venc_inst *inst,
					  enum venc_set_param_type id,
					  struct venc_enc_param *enc_param)
{
	struct venc_ap_ipi_msg_set_param out;
	bool has_lock_dvfs = false;

	mtk_vcodec_debug(inst, "id %d ->", id);
	if (sizeof(out) > SHARE_BUF_SIZE) {
		mtk_vcodec_err(inst, "venc_ap_ipi_msg_set_param cannot be large than %d",
					   SHARE_BUF_SIZE);
		return -EINVAL;
	}

	memset(&out, 0, sizeof(out));
	out.msg_id = AP_IPIMSG_ENC_SET_PARAM;
	out.vcu_inst_addr = inst->vcu_inst.inst_addr;
	out.ctx_id = inst->ctx->id;
	out.param_id = id;
	switch (id) {
	case VENC_SET_PARAM_ENC:
		out.data_item = 0;
		break;
	case VENC_SET_PARAM_FORCE_INTRA:
		out.data_item = 0;
		break;
	case VENC_SET_PARAM_ADJUST_BITRATE:
		out.data_item = 1;
		out.data[0] = enc_param->bitrate;
		break;
	case VENC_SET_PARAM_ADJUST_FRAMERATE:
		out.data_item = 1;
		out.data[0] = enc_param->frm_rate;
		break;
	case VENC_SET_PARAM_GOP_SIZE:
		out.data_item = 1;
		out.data[0] = enc_param->gop_size;
		break;
	case VENC_SET_PARAM_INTRA_PERIOD:
		out.data_item = 1;
		out.data[0] = enc_param->intra_period;
		break;
	case VENC_SET_PARAM_SKIP_FRAME:
		out.data_item = 0;
		break;
	case VENC_SET_PARAM_PREPEND_HEADER:
		out.data_item = 0;
		break;
	case VENC_SET_PARAM_SCENARIO:
		out.data_item = 1;
		out.data[0] = enc_param->scenario;
		break;
	case VENC_SET_PARAM_NONREFP:
		out.data_item = 1;
		out.data[0] = enc_param->nonrefp;
		break;
	case VENC_SET_PARAM_NONREFPFREQ:
		out.data_item = 1;
		out.data[0] = enc_param->nonrefpfreq;
		break;
	case VENC_SET_PARAM_DETECTED_FRAMERATE:
		out.data_item = 1;
		out.data[0] = enc_param->detectframerate;
		break;
	case VENC_SET_PARAM_RFS_ON:
		out.data_item = 1;
		out.data[0] = enc_param->rfs;
		break;
	case VENC_SET_PARAM_PREPEND_SPSPPS_TO_IDR:
		out.data_item = 1;
		out.data[0] = enc_param->prependheader;
		break;
	case VENC_SET_PARAM_OPERATION_RATE:
		out.data_item = 2;
		out.data[0] = enc_param->operationrate;
		out.data[1] = enc_param->operationrate_adaptive;
		break;
	case VENC_SET_PARAM_BITRATE_MODE:
		out.data_item = 1;
		out.data[0] = enc_param->bitratemode;
		break;
	case VENC_SET_PARAM_ROI_ON:
		out.data_item = 1;
		out.data[0] = enc_param->roion;
		break;
	case VENC_SET_PARAM_HEIF_GRID_SIZE:
		out.data_item = 1;
		out.data[0] = enc_param->heif_grid_size;
		break;
	case VENC_SET_PARAM_COLOR_DESC:
		out.data_item = 0; // passed via vsi
		break;
	case VENC_SET_PARAM_SEC_MODE:
		out.data_item = 1;
		out.data[0] = enc_param->svp_mode;
		break;
	case VENC_SET_PARAM_TSVC:
		out.data_item = 1;
		out.data[0] = enc_param->tsvc;
		break;
	case VENC_SET_PARAM_ENABLE_HIGHQUALITY:
		out.data_item = 1;
		out.data[0] = enc_param->highquality;
		break;
	case VENC_SET_PARAM_ADJUST_MAX_QP:
		out.data_item = 1;
		out.data[0] = enc_param->max_qp;
		break;
	case VENC_SET_PARAM_ADJUST_MIN_QP:
		out.data_item = 1;
		out.data[0] = enc_param->min_qp;
		break;
	case VENC_SET_PARAM_ADJUST_I_P_QP_DELTA:
		out.data_item = 1;
		out.data[0] = enc_param->ip_qpdelta;
		break;
	case VENC_SET_PARAM_ADJUST_FRAME_LEVEL_QP:
		out.data_item = 1;
		out.data[0] = enc_param->framelvl_qp;
		break;
	case VENC_SET_PARAM_ADJUST_CHROMQA_QP:
		out.data_item = 2;
		out.data[0] = enc_param->cb_qp_offset;
		out.data[1] = enc_param->cr_qp_offset;
		break;
	case VENC_SET_PARAM_MBRC_TKSPD:
		out.data_item = 1;
		out.data[0] = enc_param->mbrc_tk_spd;
		break;
	case VENC_SET_PARAM_ADJUST_QP_CONTROL_MODE:
		out.data_item = 1;
		out.data[0] = enc_param->qp_control_mode;
		break;
	case VENC_SET_PARAM_ENABLE_DUMMY_NAL:
		out.data_item = 1;
		out.data[0] = enc_param->dummynal;
		break;
	case VENC_SET_PARAM_ENABLE_LOW_LATENCY_WFD:
		out.data_item = 1;
		out.data[0] = enc_param->lowlatencywfd;
		break;
	case VENC_SET_PARAM_SLICE_CNT:
		out.data_item = 1;
		out.data[0] = enc_param->slice_count;
		break;
	case VENC_SET_PARAM_TEMPORAL_LAYER_CNT:
		out.data_item = 2;
		out.data[0] = enc_param->temporal_layer_pcount;
		out.data[1] = enc_param->temporal_layer_bcount;
		break;
	case VENC_SET_PARAM_MMDVFS:
		out.data_item = 1;
		out.data[0] = enc_param->venc_dvfs_state;
		has_lock_dvfs = true;
		mtk_v4l2_debug(4, "[VDVFS][VENC] data VENC_SET_PARAM_MMDVFS");
		break;
	case VENC_SET_PARAM_VISUAL_QUALITY:
		out.data_item = 0; // passed via vsi
		break;
	case VENC_SET_PARAM_INIT_QP:
		out.data_item = 0; // passed via vsi
		break;
	case VENC_SET_PARAM_FRAME_QP_RANGE:
		out.data_item = 0; // passed via vsi
		break;
	case VENC_SET_PARAM_CONFIG:
		out.data_item = 0; // passed via vsi
		break;
	default:
		mtk_vcodec_err(inst, "id %d not supported", id);
		return -EINVAL;
	}

	if (venc_vcp_ipi_send(inst, &out, sizeof(out), false, true, has_lock_dvfs)) {
		mtk_vcodec_err(inst,
			"AP_IPIMSG_ENC_SET_PARAM %d fail", id);
		return -EINVAL;
	}

	mtk_vcodec_debug(inst, "id %d <-", id);

	return 0;
}


static int venc_vcp_set_param(unsigned long handle,
	enum venc_set_param_type type,
	struct venc_enc_param *enc_prm)
{
	int i;
	int ret = 0;
	struct venc_inst *inst = (struct venc_inst *)handle;
	unsigned int fmt = 0;

	if (inst == NULL)
		return -EINVAL;

	mtk_vcodec_debug(inst, "->type=%d", type);

	switch (type) {
	case VENC_SET_PARAM_ENC:
		if (inst->vsi == NULL)
			return -EINVAL;

		inst->vsi->config.input_fourcc = enc_prm->input_yuv_fmt;
		inst->vsi->config.bitrate = enc_prm->bitrate;
		inst->vsi->config.pic_w = enc_prm->width;
		inst->vsi->config.pic_h = enc_prm->height;
		inst->vsi->config.buf_w = enc_prm->buf_width;
		inst->vsi->config.buf_h = enc_prm->buf_height;
		inst->vsi->config.gop_size = enc_prm->gop_size;
		inst->vsi->config.framerate = enc_prm->frm_rate;
		inst->vsi->config.intra_period = enc_prm->intra_period;
		inst->vsi->config.operationrate = enc_prm->operationrate;
		inst->vsi->config.bitratemode = enc_prm->bitratemode;
		inst->vsi->config.roion = enc_prm->roion;
		inst->vsi->config.scenario = enc_prm->scenario;
		inst->vsi->config.prependheader = enc_prm->prependheader;
		inst->vsi->config.heif_grid_size = enc_prm->heif_grid_size;
		inst->vsi->config.max_w = enc_prm->max_w;
		inst->vsi->config.max_h = enc_prm->max_h;
		inst->vsi->config.num_b_frame = enc_prm->num_b_frame;
		inst->vsi->config.slbc_ready = enc_prm->slbc_ready;
		inst->vsi->config.slbc_addr = enc_prm->slbc_addr;
		inst->vsi->config.i_qp = enc_prm->i_qp;
		inst->vsi->config.p_qp = enc_prm->p_qp;
		inst->vsi->config.b_qp = enc_prm->b_qp;
		inst->vsi->config.svp_mode = enc_prm->svp_mode;
		inst->vsi->config.tsvc = enc_prm->tsvc;
		inst->vsi->config.highquality = enc_prm->highquality;
		inst->vsi->config.max_qp = enc_prm->max_qp;
		inst->vsi->config.min_qp = enc_prm->min_qp;
		inst->vsi->config.i_p_qp_delta = enc_prm->ip_qpdelta;
		inst->vsi->config.qp_control_mode = enc_prm->qp_control_mode;
		inst->vsi->config.frame_level_qp = enc_prm->framelvl_qp;
		inst->vsi->config.dummynal = enc_prm->dummynal;
		inst->vsi->config.lowlatencywfd = enc_prm->lowlatencywfd;
		inst->vsi->config.slice_count = enc_prm->slice_count;

		inst->vsi->config.hier_ref_layer = enc_prm->hier_ref_layer;
		inst->vsi->config.hier_ref_type = enc_prm->hier_ref_type;
		inst->vsi->config.temporal_layer_pcount = enc_prm->temporal_layer_pcount;
		inst->vsi->config.temporal_layer_bcount = enc_prm->temporal_layer_bcount;
		inst->vsi->config.max_ltr_num = enc_prm->max_ltr_num;
		inst->vsi->config.ctx_id = enc_prm->ctx_id;
		inst->vsi->config.priority = enc_prm->priority;
		inst->vsi->config.codec_fmt = enc_prm->codec_fmt;

		inst->vsi->config.qpvbr_upper_enable = enc_prm->qpvbr_upper_enable;
		inst->vsi->config.qpvbr_qp_upper_threshold = enc_prm->qpvbr_qp_upper_threshold;
		inst->vsi->config.qpvbr_qp_max_brratio = enc_prm->qpvbr_qp_max_brratio;
		inst->vsi->config.qpvbr_lower_enable = enc_prm->qpvbr_lower_enable;
		inst->vsi->config.qpvbr_qp_lower_threshold = enc_prm->qpvbr_qp_lower_threshold;
		inst->vsi->config.qpvbr_qp_min_brratio = enc_prm->qpvbr_qp_min_brratio;
		inst->vsi->config.cb_qp_offset = enc_prm->cb_qp_offset;
		inst->vsi->config.cr_qp_offset = enc_prm->cr_qp_offset;
		inst->vsi->config.mbrc_tk_spd = enc_prm->mbrc_tk_spd;
		inst->vsi->config.ifrm_q_ltr = enc_prm->ifrm_q_ltr;
		inst->vsi->config.pfrm_q_ltr = enc_prm->pfrm_q_ltr;
		inst->vsi->config.bfrm_q_ltr = enc_prm->bfrm_q_ltr;

		if (enc_prm->visual_quality) {
			memcpy(&inst->vsi->config.visual_quality,
				enc_prm->visual_quality,
				sizeof(struct mtk_venc_visual_quality));
		}

		if (enc_prm->init_qp) {
			memcpy(&inst->vsi->config.init_qp,
				enc_prm->init_qp,
				sizeof(struct mtk_venc_init_qp));
		}

		if (enc_prm->frame_qp_range) {
			memcpy(&inst->vsi->config.frame_qp_range,
				enc_prm->frame_qp_range,
				sizeof(struct mtk_venc_frame_qp_range));
		}

		if (enc_prm->nal_length) {
			memcpy(&inst->vsi->config.nal_length,
				enc_prm->nal_length,
				sizeof(struct mtk_venc_nal_length));
		}

		if (enc_prm->color_desc) {
			memcpy(&inst->vsi->config.color_desc,
				enc_prm->color_desc,
				sizeof(struct mtk_color_desc));
		}

		if (enc_prm->multi_ref) {
			memcpy(&inst->vsi->config.multi_ref,
				enc_prm->multi_ref,
				sizeof(struct mtk_venc_multi_ref));
		}

		if (enc_prm->vui_info) {
			memcpy(&inst->vsi->config.vui_info,
				enc_prm->vui_info,
				sizeof(struct mtk_venc_vui_info));
		}

		inst->vsi->config.slice_header_spacing =
			enc_prm->slice_header_spacing;
		inst->vsi->config.mlvec_mode =
			enc_prm->mlvec_mode;

		fmt = inst->ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc;
		mtk_vcodec_debug(inst, "fmt:%s(0x%x)", FOURCC_STR(fmt), fmt);

		if (fmt == V4L2_PIX_FMT_H264) {
			inst->vsi->config.profile = enc_prm->profile;
			inst->vsi->config.level = enc_prm->level;
		} else if (fmt == V4L2_PIX_FMT_HEVC ||
				fmt == V4L2_PIX_FMT_HEIF) {
			inst->vsi->config.profile =
				venc_h265_get_profile(inst, enc_prm->profile);
			inst->vsi->config.level =
				venc_h265_get_level(inst, enc_prm->level,
					enc_prm->tier);
		} else if (fmt == V4L2_PIX_FMT_MPEG4) {
			inst->vsi->config.profile =
				venc_mpeg4_get_profile(inst, enc_prm->profile);
			inst->vsi->config.level =
				venc_mpeg4_get_level(inst, enc_prm->level);
		}
		inst->vsi->config.wfd = 0;
		ret = vcp_enc_set_param(inst, type, enc_prm);
		if (ret)
			break;

		for (i = 0; i < MTK_VCODEC_MAX_PLANES; i++) {
			enc_prm->sizeimage[i] =
				inst->vsi->sizeimage[i];
			mtk_vcodec_debug(inst, "sizeimage[%d] size=0x%x", i,
							 enc_prm->sizeimage[i]);
		}
		inst->ctx->async_mode = !(inst->vsi->sync_mode);

		break;
	case VENC_SET_PARAM_PREPEND_HEADER:
		inst->prepend_hdr = 1;
		ret = vcp_enc_set_param(inst, type, enc_prm);
		break;
	case VENC_SET_PARAM_COLOR_DESC:
		if (inst->vsi == NULL)
			return -EINVAL;

		memcpy(&inst->vsi->config.color_desc, enc_prm->color_desc,
			sizeof(struct mtk_color_desc));
		ret = vcp_enc_set_param(inst, type, enc_prm);
		break;
	case VENC_SET_PARAM_PROPERTY:
		mtk_vcodec_debug(inst, "enc_prm->set_vcp_buf:%s", enc_prm->set_vcp_buf);
		set_venc_vcp_data(inst, VENC_SET_PROP_MEM_ID, enc_prm->set_vcp_buf);
		break;
	case VENC_SET_PARAM_VCP_LOG_INFO:
		mtk_vcodec_debug(inst, "enc_prm->set_vcp_buf:%s", enc_prm->set_vcp_buf);
		set_venc_vcp_data(inst, VENC_VCP_LOG_INFO_ID, enc_prm->set_vcp_buf);
		break;
	case VENC_SET_PARAM_MMDVFS:
		if (inst->vsi == NULL)
			return -EINVAL;
		mtk_v4l2_debug(4, "[VDVFS][VENC] VENC_SET_PARAM_MMDVFS");
		ret = vcp_enc_set_param(inst, type, enc_prm);
		break;
	case VENC_SET_PARAM_VISUAL_QUALITY:
		if (inst->vsi == NULL)
			return -EINVAL;
		memcpy(&inst->vsi->config.visual_quality, enc_prm->visual_quality,
			sizeof(struct mtk_venc_visual_quality));
		ret = vcp_enc_set_param(inst, type, enc_prm);
		break;
	case VENC_SET_PARAM_INIT_QP:
		if (inst->vsi == NULL)
			return -EINVAL;
		memcpy(&inst->vsi->config.init_qp, enc_prm->init_qp,
			sizeof(struct mtk_venc_init_qp));
		ret = vcp_enc_set_param(inst, type, enc_prm);
		break;
	case VENC_SET_PARAM_FRAME_QP_RANGE:
		if (inst->vsi == NULL)
			return -EINVAL;
		memcpy(&inst->vsi->config.frame_qp_range, enc_prm->frame_qp_range,
			sizeof(struct mtk_venc_frame_qp_range));
		ret = vcp_enc_set_param(inst, type, enc_prm);
		break;
	case VENC_SET_PARAM_CONFIG:
		if (enc_prm->config_data) {
			struct venc_common_vsi *venc_com_vsi;

			venc_com_vsi = (struct venc_common_vsi *)inst->ctx->dev->com_vsi;
			memcpy(venc_com_vsi->config_data, enc_prm->config_data,
			sizeof(__u8)*VENC_CONFIG_LENGTH);
			ret = venc_set_config_data(inst);
		}
		break;
	default:
		if (inst->vsi == NULL)
			return -EINVAL;

		ret = vcp_enc_set_param(inst, type, enc_prm);
		inst->ctx->async_mode = !(inst->vsi->sync_mode);
		break;
	}

	mtk_vcodec_debug_leave(inst);

	return ret;
}

static int venc_vcp_deinit(unsigned long handle)
{
	int ret = 0;
	struct venc_inst *inst = (struct venc_inst *)handle;
	struct venc_ap_ipi_msg_deinit out;
	struct vcp_enc_mem_list *tmp = NULL;
	struct list_head *p, *q;
	struct device *dev = NULL;

	memset(&out, 0, sizeof(out));
	out.msg_id = AP_IPIMSG_ENC_DEINIT;
	out.vcu_inst_addr = inst->vcu_inst.inst_addr;
	out.ctx_id = inst->ctx->id;

	mtk_vcodec_debug_enter(inst);
	ret = venc_vcp_ipi_send(inst, &out, sizeof(out), false, true, false);
	mtk_vcodec_debug_leave(inst);

	mtk_vcodec_del_ctx_list(inst->ctx);

	mutex_lock(inst->vcu_inst.ctx_ipi_lock);
	list_for_each_safe(p, q, &inst->vcu_inst.bufs) {
		tmp = list_entry(p, struct vcp_enc_mem_list, list);
		dev = get_dev_by_mem_type(inst, &tmp->mem);
		mtk_vcodec_free_mem(&tmp->mem, dev, tmp->attach, tmp->sgt);
		mtk_v4l2_debug(0, "[%d] leak free va 0x%llx pa 0x%llx iova 0x%llx len %d type %d",
			inst->ctx->id, tmp->mem.va, tmp->mem.pa,
			tmp->mem.iova, tmp->mem.len,  tmp->mem.type);

		list_del(p);
		kfree(tmp);
	}
	mutex_unlock(inst->vcu_inst.ctx_ipi_lock);
	mutex_destroy(inst->vcu_inst.ctx_ipi_lock);
	kfree(inst->vcu_inst.ctx_ipi_lock);
	kfree(inst);

	return ret;
}

static const struct venc_common_if venc_vcp_if = {
	.init = venc_vcp_init,
	.encode = venc_vcp_encode,
	.get_param = venc_vcp_get_param,
	.set_param = venc_vcp_set_param,
	.deinit = venc_vcp_deinit,
};

const struct venc_common_if *get_enc_vcp_if(void)
{
	return &venc_vcp_if;
}
