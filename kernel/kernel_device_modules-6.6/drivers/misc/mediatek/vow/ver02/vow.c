// SPDX-License-Identifier: GPL-2.0
/*
 *  vow.c  --  VoW platform driver
 *
 *  Copyright (c) 2020 MediaTek Inc.
 *  Author: Michael HSiao <michael.hsiao@mediatek.com>
 */

/*****************************************************************************
 * Header Files
 *****************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/miscdevice.h>
/* #include <linux/wakelock.h> */
#include <linux/pm_wakeup.h>
#include <linux/compat.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/notifier.h>  /* FOR SCP REVOCER */
#ifdef SIGTEST
#include <asm/siginfo.h>
#endif
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <linux/pinctrl/consumer.h>
#include <uapi/linux/sched/types.h>
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include "scp.h"
#endif  /* #if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT) */
#include "vow.h"
#include "vow_assert.h"

/*****************************************************************************
 * Variable Definition
 ****************************************************************************/
static unsigned int VowDrv_Wait_Queue_flag;
static unsigned int VoiceData_Wait_Queue_flag;
static unsigned int DumpData_Wait_Queue_flag;
static unsigned int ScpRecover_Wait_Queue_flag;
static DECLARE_WAIT_QUEUE_HEAD(VowDrv_Wait_Queue);
static DECLARE_WAIT_QUEUE_HEAD(VoiceData_Wait_Queue);
static DECLARE_WAIT_QUEUE_HEAD(DumpData_Wait_Queue);
static DECLARE_WAIT_QUEUE_HEAD(ScpRecover_Wait_Queue);
static DEFINE_SPINLOCK(vowdrv_lock);
static DEFINE_SPINLOCK(vowdrv_dump_lock);
static DEFINE_SPINLOCK(vow_rec_queue_lock);
static struct wakeup_source *vow_suspend_lock;
static struct wakeup_source *vow_ipi_suspend_lock;
static struct wakeup_source *vow_dump_suspend_lock;
//static struct dump_package_t dump_package;
static int init_flag = -1;
static const uint32_t kReadVowDumpSize = 0xA00 * 2; // 320(10ms) x 8 x 2ch= 5120 = 0x1400
static const uint32_t kReadVowDumpSize_Big = 0x1F40 * 2; // 320(10ms) x 25 x 2ch= 16000 = 0x3E80

/* for vow sub feature option */
static bool dual_ch_transfer;

/*****************************************************************************
 * Function  Declaration
 ****************************************************************************/
static void vow_service_getVoiceData(void);
static void vow_service_getDumpData(void);
static void vow_ipi_reg_ok(short uuid,
			   int confidence_lv,
			   unsigned int extradata_len,
			   unsigned int payloaddump_len);
static bool VowDrv_SetFlag(int type, unsigned int set);
static int VowDrv_GetHWStatus(void);
static bool VowDrv_SetBargeIn(unsigned int set, unsigned int irq_id);
//static int vow_service_SearchSpeakerModelWithUuid(int uuid);
static int vow_service_SearchSpeakerModelWithKeyword(int keyword);
static int vow_service_SearchSpeakerModelWithId(int id);
/* ++ for payload dump feature(data from scp) ++ */
static void vow_service_ReadPayloadDumpData(unsigned int buf_length);
static DEFINE_MUTEX(vow_payloaddump_mutex);
/* -- for payload dump feature(data from scp) -- */
static DEFINE_MUTEX(vow_vmalloc_lock);
static DEFINE_MUTEX(vow_extradata_mutex);
static DEFINE_MUTEX(vow_sendspkmdl_mutex);

/*****************************************************************************
 * VOW SERVICES
 *****************************************************************************/

static struct
{
	struct vow_speaker_model_t  vow_speaker_model[MAX_VOW_SPEAKER_MODEL];
	unsigned long        vow_info_apuser[MAX_VOW_INFO_LEN];
	unsigned long        voicedata_user_addr;
	unsigned long        voicedata_user_size;
	short                *voicedata_kernel_ptr;
	char                 *voicedata_scp_ptr;
	dma_addr_t           voicedata_scp_addr;
	char                 *extradata_ptr;
	dma_addr_t           extradata_addr;
	char                 *extradata_mem_ptr;
	unsigned int         extradata_bytelen;
	/* ++ for payload dump feature(data from scp) ++ */
	bool                 payloaddump_enable;
	char                 *payloaddump_scp_ptr;
	char                 *payloaddump_r_scp_ptr;
	dma_addr_t           payloaddump_scp_addr;
	dma_addr_t           payloaddump_r_scp_addr;
	unsigned long        payloaddump_user_addr;
	unsigned long        payloaddump_user_max_size;
	unsigned long        payloaddump_user_return_size_addr;
	short                *payloaddump_kernel_ptr;
	unsigned int         payloaddump_length;
	/* -- for payload dump feature(data from scp) -- */
	unsigned int         kernel_voicedata_idx;
	bool                 scp_command_flag;
	bool                 recording_flag;
	int                  scp_command_id;
	int                  scp_command_keywordid;
	int                  confidence_level;
	int                  eint_status;
	int                  pwr_status;
	bool                 suspend_lock;
	bool                 firstRead;
	unsigned long        voicedata_user_return_size_addr;
	unsigned int         transfer_length;
	struct device_node   *node;
	struct pinctrl       *pinctrl;
	struct pinctrl_state *pins_eint_on;
	struct pinctrl_state *pins_eint_off;
	bool                 bypass_enter_phase3;
	unsigned int         enter_phase3_cnt;
	unsigned int         force_phase_stage;
	bool                 swip_log_enable;
	struct vow_eint_data_struct_t  vow_eint_data_struct;
	unsigned long long   scp_recognize_ok_cycle;
	unsigned long long   ap_received_ipi_cycle;
	bool                 tx_keyword_start;
	short                *interleave_pcmdata_ptr;
	bool                 dump_pcm_flag;
	bool                 scp_recovering;
	bool                 vow_recovering;
	bool                 split_dumpfile_flag;
	bool                 mcps_flag;
	bool                 bargein_enable;
	bool                 virtual_input;
	unsigned int         scp_dual_mic_switch;
	unsigned int         provider_type;
	unsigned int         google_engine_version;
	unsigned int         vow_mic_number;
	unsigned int         vow_speaker_number;
	char                 alexa_engine_version[VOW_ENGINE_INFO_LENGTH_BYTE];
	char                 google_engine_arch[VOW_ENGINE_INFO_LENGTH_BYTE];
	unsigned long        custom_model_size;
	unsigned int         scp_recover_data;
	unsigned long        scp_recover_user_return_size_addr;
	unsigned int         delay_wakeup_time;
	unsigned int         payloaddump_cb_type;
	unsigned int         chre_status;
} vowserv;

struct vow_dump_info_t {
//	dma_addr_t    phy_addr;           // address of reserved buffer
	char          *vir_addr;          // virtual (kernel) addess of reserved buffer
	uint32_t      size;               // size of reserved buffer (bytes)
	uint32_t      scp_dump_offset[VOW_MAX_CH_NUM]; // return data offset from scp
	uint32_t      scp_dump_size[VOW_MAX_CH_NUM];   // return data size from scp
	short         *kernel_dump_addr;  // kernel internal buffer address
	unsigned int  kernel_dump_idx;    // current index of kernel_dump_addr
	unsigned int  kernel_dump_size;   // size of kernel_dump_ptr buffer (bytes)
	unsigned long user_dump_addr;     // addr of user dump buffer
	unsigned long user_dump_idx;      // current index of user_dump_addr
	unsigned long user_dump_size;     // size of user dump buffer
	unsigned long user_return_size_addr; // addr of return size of user
};

struct vow_rec_queue_t {
	struct vow_rec_queue_info_t  rec_ipi_queue[REC_QUEUE_NUM];
	unsigned int                 rec_ipi_queue_w;
	unsigned int                 rec_ipi_queue_r;
	unsigned int                 rec_ipi_queue_cnt;
};

#define NUM_DELAY_INFO (2)
uint32_t delay_info[NUM_DELAY_INFO];
struct vow_dump_info_t vow_dump_info[NUM_DUMP_DATA];
struct vow_rec_queue_t vow_rec_queue;

/* not used */
//#define VOW_SMART_DEVICE_SUPPORT

static void vow_init_rec_ipi_queue(void)
{
	memset((void *)vow_rec_queue.rec_ipi_queue, 0,
	       sizeof(struct vow_rec_queue_info_t) * REC_QUEUE_NUM);
	vow_rec_queue.rec_ipi_queue_w = 0;
	vow_rec_queue.rec_ipi_queue_r = 0;
	vow_rec_queue.rec_ipi_queue_cnt = 0;
}

static void vow_set_rec_ipi_queue(unsigned int offset, unsigned int length)
{
	vow_rec_queue.rec_ipi_queue[vow_rec_queue.rec_ipi_queue_w].rec_buf_offset = offset;
	vow_rec_queue.rec_ipi_queue[vow_rec_queue.rec_ipi_queue_w].rec_buf_length = length;
	vow_rec_queue.rec_ipi_queue_w++;
	if (vow_rec_queue.rec_ipi_queue_w >= REC_QUEUE_NUM)
		vow_rec_queue.rec_ipi_queue_w = 0;

	if (vow_rec_queue.rec_ipi_queue_cnt >= REC_QUEUE_NUM) {
		VOWDRV_DEBUG("%s, queue full\n", __func__);
		vow_rec_queue.rec_ipi_queue_cnt = REC_QUEUE_NUM;
		// queue full, need to shift R to clear the oldest one.
		vow_rec_queue.rec_ipi_queue_r++;
		if (vow_rec_queue.rec_ipi_queue_r >= REC_QUEUE_NUM)
			vow_rec_queue.rec_ipi_queue_r = 0;
	} else
		vow_rec_queue.rec_ipi_queue_cnt++;

	/*VOWDRV_DEBUG("%s, w=%d, cnt=%d",*/
	/*	__func__,*/
	/*	vow_rec_queue.rec_ipi_queue_w,*/
	/*	vow_rec_queue.rec_ipi_queue_cnt);*/
}

static struct vow_rec_queue_info_t *vow_get_rec_ipi_queue(void)
{
	unsigned int r_idx;

	if (vow_rec_queue.rec_ipi_queue_cnt > 0) {
		r_idx = vow_rec_queue.rec_ipi_queue_r;
		vow_rec_queue.rec_ipi_queue_r++;
		if (vow_rec_queue.rec_ipi_queue_r >= REC_QUEUE_NUM)
			vow_rec_queue.rec_ipi_queue_r = 0;

		vow_rec_queue.rec_ipi_queue_cnt--;
		/*VOWDRV_DEBUG("%s, r=%d, cnt=%d",*/
		/*	__func__,*/
		/*	vow_rec_queue.rec_ipi_queue_r,*/
		/*	vow_rec_queue.rec_ipi_queue_cnt);*/
		return &vow_rec_queue.rec_ipi_queue[r_idx];
	} else
		return NULL;
}

/*****************************************************************************
 * DSP IPI HANDELER
 *****************************************************************************/
static void vow_ipi_rx_handle_data_msg(void *msg_data)
{
	struct vow_ipi_combined_info_t *ipi_ptr;
	unsigned long flags;
	unsigned long rec_queue_flags;

	ipi_ptr = (struct vow_ipi_combined_info_t *)msg_data;

	if (vowserv.dump_pcm_flag == true) {
		spin_lock_irqsave(&vowdrv_dump_lock, flags);
		if ((ipi_ptr->ipi_type_flag & INPUT_DUMP_IDX_MASK)) {
			if (vow_dump_info[DUMP_INPUT].scp_dump_size[0] != 0) {
				VOWDRV_DEBUG("%s WARNING dump idx 0x%x %d not handled\n",
					     __func__,
					     INPUT_DUMP_IDX_MASK,
					     vow_dump_info[DUMP_INPUT].scp_dump_size[0]);
			}
			if ((ipi_ptr->mic_offset < BARGEIN_DUMP_BYTE_CNT_MIC) &&
			    (ipi_ptr->mic_dump_size == (BARGEIN_DUMP_BYTE_CNT_MIC >> 1))) {
				vow_dump_info[DUMP_INPUT].scp_dump_size[0] =
					ipi_ptr->mic_dump_size;
				vow_dump_info[DUMP_INPUT].scp_dump_offset[0] =
					ipi_ptr->mic_offset;
				if (vowserv.vow_mic_number == 2) {
					if (ipi_ptr->mic_offset_R <
						BARGEIN_DUMP_TOTAL_BYTE_CNT_MIC) {
						vow_dump_info[DUMP_INPUT].scp_dump_size[1] =
							ipi_ptr->mic_dump_size;
						vow_dump_info[DUMP_INPUT].scp_dump_offset[1] =
							ipi_ptr->mic_offset_R;
					} else {
						vow_dump_info[DUMP_INPUT].scp_dump_size[0] = 0;
						vow_dump_info[DUMP_INPUT].scp_dump_size[1] = 0;
						VOWDRV_DEBUG("%s mic offset_R 0x%x BOUND 0x%x\n",
							     __func__,
							     ipi_ptr->mic_offset_R,
							     (unsigned int)
							     BARGEIN_DUMP_TOTAL_BYTE_CNT_MIC);
					}
				}
			} else {
				vow_dump_info[DUMP_INPUT].scp_dump_size[0] = 0;
				VOWDRV_DEBUG("%s mic offset 0x%x size 0x%x BOUNDARY 0x%x\n",
					     __func__,
					     ipi_ptr->mic_offset,
					     ipi_ptr->mic_dump_size,
					     (unsigned int)BARGEIN_DUMP_BYTE_CNT_MIC);
			}
		}
		/* IPIMSG_VOW_BARGEIN_PCMDUMP_OK */
		if ((ipi_ptr->ipi_type_flag & BARGEIN_DUMP_IDX_MASK)) {
			if ((ipi_ptr->echo_offset < BARGEIN_DUMP_BYTE_CNT_ECHO) &&
				(ipi_ptr->echo_dump_size ==
				(BARGEIN_DUMP_BYTE_CNT_ECHO >> 1))) {
				vow_dump_info[DUMP_BARGEIN].scp_dump_size[0] =
					ipi_ptr->echo_dump_size;
				vow_dump_info[DUMP_BARGEIN].scp_dump_offset[0] =
					ipi_ptr->echo_offset;
				if (VOW_MAX_ECHO_NUM == 2) {
					if (ipi_ptr->echo_offset_R <
					    BARGEIN_DUMP_TOTAL_BYTE_CNT_ECHO) {
						vow_dump_info[DUMP_BARGEIN].scp_dump_size[1] =
							ipi_ptr->echo_dump_size;
						vow_dump_info[DUMP_BARGEIN].scp_dump_offset[1] =
							ipi_ptr->echo_offset_R;
					} else {
						vow_dump_info[DUMP_BARGEIN].scp_dump_size[0] = 0;
						vow_dump_info[DUMP_BARGEIN].scp_dump_size[1] = 0;
						VOWDRV_DEBUG("%s echo offset_R 0x%x BOUND 0x%x\n",
							     __func__,
							     ipi_ptr->echo_offset_R,
							     (unsigned int)
							     BARGEIN_DUMP_TOTAL_BYTE_CNT_ECHO);
					}
				}
			} else {
				vow_dump_info[DUMP_BARGEIN].scp_dump_size[0] = 0;
				VOWDRV_DEBUG("%s echo offset 0x%x, size 0x%x BOUNDARY 0x%x\n",
					     __func__,
					     ipi_ptr->echo_offset,
					     ipi_ptr->echo_dump_size,
					     (unsigned int)BARGEIN_DUMP_BYTE_CNT_ECHO);
			}
		}
		if ((ipi_ptr->ipi_type_flag & AECOUT_DUMP_IDX_MASK)) {
			if ((ipi_ptr->aecout_dump_offset < AECOUT_DUMP_BYTE_CNT) &&
			    (ipi_ptr->aecout_dump_size == (AECOUT_DUMP_BYTE_CNT >> 1))) {
				vow_dump_info[DUMP_AECOUT].scp_dump_size[0] =
					ipi_ptr->aecout_dump_size;
				vow_dump_info[DUMP_AECOUT].scp_dump_offset[0] =
					ipi_ptr->aecout_dump_offset;
				if (vowserv.vow_mic_number == 2) {
					if (ipi_ptr->aecout_dump_offset_R <
					    AECOUT_DUMP_TOTAL_BYTE_CNT) {
						vow_dump_info[DUMP_AECOUT].scp_dump_size[1] =
							ipi_ptr->aecout_dump_size;
						vow_dump_info[DUMP_AECOUT].scp_dump_offset[1] =
							ipi_ptr->aecout_dump_offset_R;
					} else {
						vow_dump_info[DUMP_AECOUT].scp_dump_size[0] = 0;
						vow_dump_info[DUMP_AECOUT].scp_dump_size[1] = 0;
						VOWDRV_DEBUG("%s aecout_R 0x%x BOUND 0x%x\n",
							     __func__,
							     ipi_ptr->aecout_dump_offset_R,
							     (unsigned int)
							     AECOUT_DUMP_TOTAL_BYTE_CNT);
					}
				}
			} else {
				vow_dump_info[DUMP_AECOUT].scp_dump_size[0] = 0;
				VOWDRV_DEBUG("%s aecout offset 0x%x size 0x%x BOUNDARY 0x%x\n",
					     __func__,
					     ipi_ptr->aecout_dump_offset,
					     ipi_ptr->aecout_dump_size,
					     (unsigned int)AECOUT_DUMP_BYTE_CNT);
			}
		}
		if ((ipi_ptr->ipi_type_flag & VFFPOUT_DUMP_IDX_MASK)) {
			if ((ipi_ptr->vffpout_dump_offset < VFFPOUT_DUMP_BYTE_CNT) &&
			    (ipi_ptr->vffpout_dump_offset_2nd_ch < VFFPOUT_DUMP_TOTAL_BYTE_CNT) &&
			    (ipi_ptr->vffpout_dump_size == (VFFPOUT_DUMP_BYTE_CNT >> 1))) {
				vow_dump_info[DUMP_VFFPOUT].scp_dump_size[0] =
					ipi_ptr->vffpout_dump_size;
				vow_dump_info[DUMP_VFFPOUT].scp_dump_offset[0] =
					ipi_ptr->vffpout_dump_offset;
				/* 1st and 2nd are the same */
				vow_dump_info[DUMP_VFFPOUT].scp_dump_size[1] =
					ipi_ptr->vffpout_dump_size;
				vow_dump_info[DUMP_VFFPOUT].scp_dump_offset[1] =
					ipi_ptr->vffpout_dump_offset_2nd_ch;
			} else {
				vow_dump_info[DUMP_VFFPOUT].scp_dump_size[0] = 0;
				vow_dump_info[DUMP_VFFPOUT].scp_dump_size[1] = 0;
				VOWDRV_DEBUG("%s vffpout offset_2ch 0x%x BOUNDARY 0x%x\n",
					     __func__,
					     ipi_ptr->vffpout_dump_offset_2nd_ch,
					     (unsigned int)VFFPOUT_DUMP_TOTAL_BYTE_CNT);
				VOWDRV_DEBUG("%s vffpout offset 0x%x size 0x%x BOUNDARY 0x%x\n",
					     __func__,
					     ipi_ptr->vffpout_dump_offset,
					     ipi_ptr->vffpout_dump_size,
					     (unsigned int)VFFPOUT_DUMP_BYTE_CNT);
			}
		}
		if ((ipi_ptr->ipi_type_flag & VFFPIN_DUMP_IDX_MASK)) {
			if ((ipi_ptr->vffpin_dump_offset < VFFPIN_DUMP_BYTE_CNT) &&
			    (ipi_ptr->vffpin_dump_size == (VFFPIN_DUMP_BYTE_CNT >> 1))) {
				vow_dump_info[DUMP_VFFPIN].scp_dump_size[0] =
					ipi_ptr->vffpin_dump_size;
				vow_dump_info[DUMP_VFFPIN].scp_dump_offset[0] =
					ipi_ptr->vffpin_dump_offset;
				if (vowserv.vow_mic_number == 2) {
					if (ipi_ptr->vffpin_dump_offset_R <
					    VFFPIN_DUMP_TOTAL_BYTE_CNT) {
						vow_dump_info[DUMP_VFFPIN].scp_dump_size[1] =
							ipi_ptr->vffpin_dump_size;
						vow_dump_info[DUMP_VFFPIN].scp_dump_offset[1] =
							ipi_ptr->vffpin_dump_offset_R;
					} else {
						vow_dump_info[DUMP_VFFPIN].scp_dump_size[0] = 0;
						vow_dump_info[DUMP_VFFPIN].scp_dump_size[1] = 0;
						VOWDRV_DEBUG("%s vffpin R 0x%x BOUND 0x%x\n",
							     __func__,
							     ipi_ptr->vffpin_dump_offset_R,
							     (unsigned int)
							     VFFPIN_DUMP_TOTAL_BYTE_CNT);
					}
				}
			} else {
				vow_dump_info[DUMP_VFFPIN].scp_dump_size[0] = 0;
				VOWDRV_DEBUG("%s vffpin offset 0x%x size 0x%x BOUNDARY 0x%x\n",
					     __func__,
					     ipi_ptr->vffpin_dump_offset,
					     ipi_ptr->vffpin_dump_size,
					     (unsigned int)VFFPIN_DUMP_BYTE_CNT);
			}
		}
		spin_unlock_irqrestore(&vowdrv_dump_lock, flags);
		vow_service_getDumpData();
	}
	/* IPIMSG_VOW_DATAREADY */
	if ((ipi_ptr->ipi_type_flag & DEBUG_DUMP_IDX_MASK) &&
		(vowserv.recording_flag)) {
		spin_lock_irqsave(&vow_rec_queue_lock, rec_queue_flags);
		vow_set_rec_ipi_queue(ipi_ptr->voice_buf_offset, ipi_ptr->voice_length);
		spin_unlock_irqrestore(&vow_rec_queue_lock, rec_queue_flags);
		if (ipi_ptr->voice_length > 320)
			VOWDRV_DEBUG("vow,v_len=%x\n", ipi_ptr->voice_length);
		/*else*/
		/*	VOWDRV_DEBUG("...vow,v_len=%x\n", ipi_ptr->voice_length);*/
		vow_service_getVoiceData();
	}
}

void vow_ipi_rx_internal(unsigned int msg_id,
			 void *msg_data)
{
#ifdef DEBUG_IPI_RX
	unsigned long long   ipi_rx_begin_cycle;
	unsigned long long   ipi_rx_end_cycle;
	unsigned int   ipi_rx_diff_time;

	ipi_rx_begin_cycle = get_cycles();
#endif

	switch (msg_id) {
	case IPIMSG_VOW_COMBINED_INFO: {
		struct vow_ipi_combined_info_t *ipi_ptr;
		bool bypass_flag;

		ipi_ptr = (struct vow_ipi_combined_info_t *)msg_data;
		/* IPIMSG_VOW_RECOGNIZE_OK */
		/*VOWDRV_DEBUG("[vow] IPIMSG_VOW_COMBINED_INFO, flag=0x%x\n",*/
		/*	       ipi_ptr->ipi_type_flag);*/
		bypass_flag = false;
		if (ipi_ptr->ipi_type_flag & RECOG_OK_IDX_MASK) {
			if ((vowserv.recording_flag == true) &&
			    (vowserv.tx_keyword_start == true)) {
				VOWDRV_DEBUG("%s(), bypass this recog ok\n",
					__func__);
				bypass_flag = true;
			}
			if (bypass_flag == false) {
				/* toggle wakelock for abort suspend flow */
				__pm_stay_awake(vow_ipi_suspend_lock);
				__pm_relax(vow_ipi_suspend_lock);
				VOWDRV_DEBUG("%s(), receive recog_ok_ipi\n",
					__func__);
				vowserv.ap_received_ipi_cycle =
					get_cycles();
				vowserv.scp_recognize_ok_cycle =
					ipi_ptr->recog_ok_os_timer;
				vowserv.enter_phase3_cnt++;
				if (vowserv.bypass_enter_phase3 == false) {
					vow_ipi_reg_ok(
					    (short)ipi_ptr->recog_ok_keywordid,
					    ipi_ptr->confidence_lv,
					    ipi_ptr->extra_data_len,
					    ipi_ptr->payloaddump_len);
				}
			}
		}
		// Copy scp shared buffer into kernel internal buffer
		vow_ipi_rx_handle_data_msg(msg_data);
		break;
	}
	case IPIMSG_VOW_RETURN_VALUE: {
		unsigned int return_id;
		unsigned int return_value;
		unsigned int ipi_value = *(unsigned int *)msg_data;

		return_id    = (ipi_value >> WORD_H);
		return_value = (ipi_value & WORD_L_MASK);
		VOWDRV_DEBUG("%s(), IPIMSG_VOW_RETURN_VALUE, id:%d, val:%d\r",
			__func__, return_id, return_value);
		switch (return_id) {
		case VOW_FLAG_FORCE_PHASE1_DEBUG:
		case VOW_FLAG_FORCE_PHASE2_DEBUG:
			vowserv.force_phase_stage = return_value;
			break;
		case VOW_FLAG_SWIP_LOG_PRINT:
			vowserv.swip_log_enable = return_value;
			break;
		default:
			break;
		}
		break;
	}
	case IPIMSG_VOW_ALEXA_ENGINE_VER: {
		//VOWDRV_DEBUG("%s(), IPIMSG_VOW_ALEXA_ENGINE_VER %s\r",
		//	__func__, (char *)msg_data);
		memcpy(vowserv.alexa_engine_version, msg_data,
				 sizeof(vowserv.alexa_engine_version));
		}
		break;
	case IPIMSG_VOW_GOOGLE_ENGINE_VER: {
		unsigned int *temp = (unsigned int *)msg_data;

		//VOWDRV_DEBUG("%s(), IPIMSG_VOW_GOOGLE_ENGINE_VER 0x%x\r",
		//	__func__, temp[0]);
		vowserv.google_engine_version = temp[0];
		}
		break;
	case IPIMSG_VOW_GOOGLE_ARCH: {
		//VOWDRV_DEBUG("%s(), IPIMSG_VOW_GOOGLE_ARCH %s\r",
		//	__func__, (char *)msg_data);
		memcpy(vowserv.google_engine_arch, msg_data,
				 sizeof(vowserv.google_engine_arch));
		}
		break;
	default:
		break;
	}

#ifdef DEBUG_IPI_RX
	ipi_rx_end_cycle = get_cycles();
	ipi_rx_diff_time =
				(unsigned int)CYCLE_TO_NS *
				(unsigned int)(ipi_rx_end_cycle
				- ipi_rx_begin_cycle);
	if (ipi_rx_diff_time > 5000000) {
		VOWDRV_DEBUG("IPI RX handler timeout, msg_id = %d, cost %d(ns)\n",
			     msg_id, ipi_rx_diff_time);
	}
#endif
}

bool vow_ipi_rceive_ack(unsigned int msg_id,
			unsigned int msg_data)
{
	bool result = false;

	switch (msg_id) {
	case IPIMSG_VOW_ENABLE:
	case IPIMSG_VOW_DISABLE:
	case IPIMSG_VOW_SET_MODEL:
	//case IPIMSG_VOW_SET_SMART_DEVICE:
	case IPIMSG_VOW_APINIT:
	case IPIMSG_VOW_PCM_DUMP_ON:
	case IPIMSG_VOW_PCM_DUMP_OFF:
	case IPIMSG_VOW_SET_FLAG:
		result = true;
		break;
	case IPIMSG_VOW_SET_BARGEIN_ON:
	case IPIMSG_VOW_SET_BARGEIN_OFF:
		result = true;
		break;
	default:
		VOWDRV_DEBUG("%s(), no relate msg id\r", __func__);
		break;
	}
	return result;
}

static void vow_ipi_reg_ok(short keyword,
			   int confidence_lv,
			   unsigned int extradata_len,
			   unsigned int payloaddump_len)
{
	int slot;

	vowserv.scp_command_flag = true;
	/* transfer keyword id to model handle id */
	slot = vow_service_SearchSpeakerModelWithKeyword(keyword);
	if (slot < 0) {
		VOWDRV_DEBUG("%s(), Fail !! Not keyword event !!, exit\n", __func__);
		return;
	}
	/* vowserv.scp_command_id = vowserv.vow_speaker_model[slot].id; */
	vowserv.scp_command_keywordid = keyword;
	vowserv.confidence_level = confidence_lv;
	if (extradata_len <= VOW_EXTRA_DATA_SIZE)
		vowserv.extradata_bytelen = extradata_len;
	else
		vowserv.extradata_bytelen = 0;
	/* for payload dump feature(data from scp) */
	vowserv.payloaddump_length = payloaddump_len;
	VOWDRV_DEBUG("[vow PDR] payloaddump_length = 0x%x\n", vowserv.payloaddump_length);

	/* VOWDRV_DEBUG("%s(), extradata_bytelen = %d\r", */
	/*	     __func__, vowserv.extradata_bytelen); */
	VowDrv_Wait_Queue_flag = 1;
	wake_up_interruptible(&VowDrv_Wait_Queue);
}
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
static void vow_register_feature(enum feature_id id)
{
	if (!vow_check_scp_status()) {
		VOWDRV_DEBUG("SCP is off, bypass register id(%d)\n", id);
		return;
	}
	scp_register_feature(id);
}

static void vow_deregister_feature(enum feature_id id)
{
	if (!vow_check_scp_status()) {
		VOWDRV_DEBUG("SCP is off, bypass deregister id(%d)\n", id);
		return;
	}
	scp_deregister_feature(id);
}
#endif
static void vow_service_getVoiceData(void)
{
	if (VoiceData_Wait_Queue_flag == 0) {
		VoiceData_Wait_Queue_flag = 1;
		wake_up_interruptible(&VoiceData_Wait_Queue);
	} else {
		/* VOWDRV_DEBUG("getVoiceData but no one wait for it, */
		/* may lost it!!\n"); */
	}
}

static void vow_service_getDumpData(void)
{
	if (DumpData_Wait_Queue_flag == 0) {
		DumpData_Wait_Queue_flag = 1;
		wake_up_interruptible(&DumpData_Wait_Queue);
	} else {
		/* VOWDRV_DEBUG("getDumpData but no one wait for it, */
		/* may lost it!!\n"); */
	}
}

static void vow_service_getScpRecover(void)
{
	if (ScpRecover_Wait_Queue_flag == 0) {
		ScpRecover_Wait_Queue_flag = 1;
		wake_up_interruptible(&ScpRecover_Wait_Queue);
	} else {
		/* VOWDRV_DEBUG("getScpRecover but no one wait for it, */
		/* may lost it!!\n"); */
	}
}

/*****************************************************************************
 * DSP SERVICE FUNCTIONS
 *****************************************************************************/
static void vow_service_Init(void)
{
	int I;
	int ipi_ret;
	struct device_node *scp_node = NULL;
	const char *scp_dram_region = NULL;
	int scp_dram_region_support = 1; // default support SCP DRAM region since T
	unsigned int vow_ipi_buf[4];
	int ipi_size = 0;
	unsigned long rec_queue_flags;

	VOWDRV_DEBUG("%s(): %x\n", __func__, init_flag);
	/* common part */
	vowserv.scp_command_flag = false;
	vowserv.tx_keyword_start = false;
	/*Initialization*/
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	vowserv.voicedata_scp_ptr =
		(char *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
		+ VOW_VOICEDATA_OFFSET;
	vowserv.voicedata_scp_addr =
		scp_get_reserve_mem_phys(VOW_MEM_ID)
		+ VOW_VOICEDATA_OFFSET;
	/*init L/R ch audio data in DRAM*/
	/* if open dual ch transfer and "ABF support = no" in scp, we can get R ch sample = 0x101*/
	memset(vowserv.voicedata_scp_ptr, 1, VOW_VOICEDATA_SIZE * VOW_MAX_MIC_NUM);
	/*Extra data*/
	vowserv.extradata_ptr =
		(char *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
		+ VOW_EXTRA_DATA_OFFSET;
	vowserv.extradata_addr =
		scp_get_reserve_mem_phys(VOW_MEM_ID)
		+ VOW_EXTRA_DATA_OFFSET;
	/* for payload dump feature(data from scp) */
	/* use VOW_PAYLOADDUMP_OFFSET/VOW_PAYLOADDUMP_R_OFFSET to exchange payload data */
	vowserv.payloaddump_scp_ptr =
		(char *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
		+ VOW_PAYLOADDUMP_OFFSET;
	vowserv.payloaddump_scp_addr =
		scp_get_reserve_mem_phys(VOW_MEM_ID)
		+ VOW_PAYLOADDUMP_OFFSET;
	vowserv.payloaddump_r_scp_ptr =
		(char *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
		+ VOW_PAYLOADDUMP_R_OFFSET;
	vowserv.payloaddump_r_scp_addr =
		scp_get_reserve_mem_phys(VOW_MEM_ID)
		+ VOW_PAYLOADDUMP_R_OFFSET;
	VOWDRV_DEBUG("%s(), [PDR]offset = 0x%lx, r_offset = 0x%lx\n\r",
		     __func__, VOW_PAYLOADDUMP_OFFSET, VOW_PAYLOADDUMP_R_OFFSET );

#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
	//audio_load_task(TASK_SCENE_VOW);

	if (init_flag != 1) {
		/*Initialization*/
		VowDrv_Wait_Queue_flag = 0;
		VoiceData_Wait_Queue_flag = 0;
		DumpData_Wait_Queue_flag = 0;
		ScpRecover_Wait_Queue_flag = 0;
		vowserv.recording_flag = false;
		vowserv.suspend_lock = 0;
		vowserv.firstRead = false;
		spin_lock_irqsave(&vow_rec_queue_lock, rec_queue_flags);
		vow_init_rec_ipi_queue();
		spin_unlock_irqrestore(&vow_rec_queue_lock, rec_queue_flags);
		vowserv.bypass_enter_phase3 = false;
		vowserv.enter_phase3_cnt = 0;
		vowserv.scp_recovering = false;
		vowserv.vow_recovering = false;
		vowserv.mcps_flag = false;
		vowserv.virtual_input = false;
		spin_lock(&vowdrv_lock);
		vowserv.pwr_status = VOW_PWR_OFF;
		vowserv.eint_status = VOW_EINT_DISABLE;
		spin_unlock(&vowdrv_lock);
		vowserv.force_phase_stage = NO_FORCE;
		vowserv.swip_log_enable = true;
		memset((void *)&vowserv.vow_eint_data_struct, 0,
					sizeof(vowserv.vow_eint_data_struct));
		vowserv.voicedata_user_addr = 0;
		vowserv.voicedata_user_size = 0;
		vowserv.voicedata_user_return_size_addr = 0;
		for (I = 0; I < MAX_VOW_SPEAKER_MODEL; I++) {
			vowserv.vow_speaker_model[I].model_ptr = NULL;
			vowserv.vow_speaker_model[I].id = -1;
			vowserv.vow_speaker_model[I].keyword = -1;
			vowserv.vow_speaker_model[I].uuid = 0;
			vowserv.vow_speaker_model[I].flag = 0;
			vowserv.vow_speaker_model[I].enabled = 0;
		}
		/* extra data memory locate */
		mutex_lock(&vow_extradata_mutex);
		if (vowserv.extradata_mem_ptr == NULL)
			vowserv.extradata_mem_ptr = vmalloc(VOW_EXTRA_DATA_SIZE);
		mutex_unlock(&vow_extradata_mutex);
		vowserv.extradata_bytelen = 0;
		/* ++ for payload dump feature(data from scp) ++ */
		vowserv.payloaddump_enable = false;
		vowserv.payloaddump_user_addr = 0;
		vowserv.payloaddump_user_max_size = 0;
		vowserv.payloaddump_user_return_size_addr = 0;
		mutex_lock(&vow_payloaddump_mutex);
		vowserv.payloaddump_kernel_ptr = NULL;
		mutex_unlock(&vow_payloaddump_mutex);
		vowserv.payloaddump_length = 0;
		/* -- for payload dump feature(data from scp) -- */
		vowserv.voicedata_kernel_ptr = NULL;
		mutex_lock(&vow_vmalloc_lock);
		vowserv.kernel_voicedata_idx = 0;
		mutex_unlock(&vow_vmalloc_lock);
		memset((void *)vow_dump_info, 0, sizeof(vow_dump_info));
		init_flag = 1;
		vowserv.dump_pcm_flag = false;
		vowserv.split_dumpfile_flag = false;
		vowserv.interleave_pcmdata_ptr = NULL;
		vowserv.custom_model_size = 0;
		// update here when vow support more than 2 mic
		vowserv.vow_mic_number = VOW_MAX_MIC_NUM;
		vowserv.vow_speaker_number = VOW_DEFAULT_SPEAKER_NUM;
		vowserv.scp_dual_mic_switch = VOW_ENABLE_DUAL_MIC;
		vowserv.provider_type = 0;
		vowserv.bargein_enable = false;
		vowserv.scp_recover_data = VOW_SCP_EVENT_NONE;
		vowserv.scp_recover_user_return_size_addr = 0;
		vowserv.delay_wakeup_time = 0;
		vowserv.payloaddump_cb_type = PAYLOADDUMP_OFF;
		/* set meaningless default value to platform identifier and version */
		memset(vowserv.google_engine_arch, 0, VOW_ENGINE_INFO_LENGTH_BYTE);
		if (sprintf(vowserv.google_engine_arch, "12345678-1234-1234-1234-123456789012") < 0)
			VOWDRV_DEBUG("%s(), sprintf fail", __func__);
		vowserv.google_engine_version = DEFAULT_GOOGLE_ENGINE_VER;
		memset(vowserv.alexa_engine_version, 0, VOW_ENGINE_INFO_LENGTH_BYTE);
	} else {
		ipi_ret = vow_ipi_send(IPIMSG_VOW_GET_ALEXA_ENGINE_VER,
				       0,
				       NULL,
				       VOW_IPI_BYPASS_ACK);
		if (ipi_ret != IPI_SCP_SEND_PASS)
			VOWDRV_DEBUG("%s(), IPIMSG_GET_ALEXA_ENGINE_VER send error %d\n", __func__, ipi_ret);

		ipi_ret = vow_ipi_send(IPIMSG_VOW_GET_GOOGLE_ENGINE_VER,
				       0,
				       NULL,
				       VOW_IPI_BYPASS_ACK);
		if (ipi_ret != IPI_SCP_SEND_PASS)
			VOWDRV_DEBUG("%s(), IPIMSG_GET_GOOGLE_ENGINE_VER send error %d\n", __func__, ipi_ret);

		ipi_ret = vow_ipi_send(IPIMSG_VOW_GET_GOOGLE_ARCH,
				       0,
				       NULL,
				       VOW_IPI_BYPASS_ACK);
		if (ipi_ret != IPI_SCP_SEND_PASS)
			VOWDRV_DEBUG("%s(), IPIMSG_GET_GOOGLE_ARCH send error %d\n", __func__, ipi_ret);

		for (I = 0; I < MAX_VOW_SPEAKER_MODEL; I++) {
			if ((vowserv.vow_speaker_model[I].flag > 1) ||
			    (vowserv.vow_speaker_model[I].enabled > 1)) {
				VOWDRV_DEBUG("reset speaker_model[%d]", I);
				vowserv.vow_speaker_model[I].model_ptr = NULL;
				vowserv.vow_speaker_model[I].id = -1;
				vowserv.vow_speaker_model[I].keyword = -1;
				vowserv.vow_speaker_model[I].uuid = 0;
				vowserv.vow_speaker_model[I].flag = 0;
				vowserv.vow_speaker_model[I].enabled = 0;
			}
		}
		/* check if SCP support dram region feature, compatible with legacy platform */
		scp_node = of_find_compatible_node(NULL, NULL, "mediatek,scp");
		if (scp_node) {
			if (of_property_read_string(scp_node, "scp-dram-region", &scp_dram_region) == 0) {
				VOWDRV_DEBUG("%s(), scp-dram-region: %s\n", __func__, scp_dram_region);
				if (!strncmp(scp_dram_region, "enable", strlen("enable")))
					scp_dram_region_support = 1;
				else
					scp_dram_region_support = 0;
			} else {
				VOWDRV_DEBUG("%s(), Cannot read scp-dram-region property.\n", __func__);
				scp_dram_region_support = 0;
				of_node_put(scp_node);
			}
		} else {
			VOWDRV_DEBUG("%s(), Cannot find scp node in device tree\n", __func__);
		}
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
		// For legacy platform need to send reserve memory infor to SCP with IPI message
		if (scp_dram_region_support == 0) {
			dma_addr_t vow_vir_addr = scp_get_reserve_mem_phys(VOW_MEM_ID);
			unsigned int vow_mem_size = scp_get_reserve_mem_size(VOW_MEM_ID);
			dma_addr_t vow_bargein_vir_addr = scp_get_reserve_mem_phys(VOW_BARGEIN_MEM_ID);
			unsigned int vow_bargein_mem_size = scp_get_reserve_mem_size(VOW_BARGEIN_MEM_ID);

			vow_ipi_buf[0] = vow_vir_addr;
			vow_ipi_buf[1] = vow_mem_size;
			vow_ipi_buf[2] = vow_bargein_vir_addr;
			vow_ipi_buf[3] = vow_bargein_mem_size;
			ipi_size = 4;
			VOWDRV_DEBUG("[VOW_MEM_ID]vir: 0x%llx, size: 0x%x, [VOW_BARGEIN_MEM_ID]vir: 0x%llx, 0x%x\n",
				vow_vir_addr, vow_mem_size, vow_bargein_vir_addr, vow_bargein_mem_size);
		}
#else
		VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
		ipi_ret = vow_ipi_send(IPIMSG_VOW_APINIT,
					ipi_size,
					&vow_ipi_buf[0],
					VOW_IPI_BYPASS_ACK);
#if VOW_PRE_LEARN_MODE
		VowDrv_SetFlag(VOW_FLAG_PRE_LEARN, true);
#endif
	}
}

static int vow_service_GetParameter(unsigned long arg)
{
	unsigned long vow_info_ap[MAX_VOW_INFO_LEN];

	if (copy_from_user((void *)(&vow_info_ap[0]),
			   (const void __user *)(arg),
			   sizeof(vow_info_ap))) {
		VOWDRV_DEBUG("vow get parameter fail\n");
		return -EFAULT;
	}
	if (vow_info_ap[3] > VOW_MODEL_SIZE ||
	    vow_info_ap[3] < VOW_MODEL_SIZE_THRES) {
		VOWDRV_DEBUG("vow Modle Size is incorrect %d\n",
			     (unsigned int)vow_info_ap[3]);
		return -EFAULT;
	}
	memcpy(vowserv.vow_info_apuser, vow_info_ap,
				 sizeof(vow_info_ap));
	VOWDRV_DEBUG(
	"vow get parameter: id %lu, keyword %lu, mdl_ptr 0x%lx, mdl_sz %lu\n",
		     vowserv.vow_info_apuser[0],
		     vowserv.vow_info_apuser[1],
		     vowserv.vow_info_apuser[2],
		     vowserv.vow_info_apuser[3]);
	VOWDRV_DEBUG(
	"vow get parameter: return size addr 0x%lx, uuid %ld, data 0x%lx\n",
		     vowserv.vow_info_apuser[4],
		     vowserv.vow_info_apuser[5],
		     vowserv.vow_info_apuser[6]);

	return 0;
}

static void vow_interleaving(short *out_buf,
			     short *l_sample,
			     short *r_sample,
			     unsigned int buf_length) // unit: byte for 1ch
{
	int i;
	int smpl_max;

	smpl_max = buf_length / 2;  // byte change to sample number
	for (i = 0; i < smpl_max; i++) {
		*out_buf++ = *l_sample++;
		*out_buf++ = *r_sample++;
	}
}

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
static int vow_service_CopyModel(int slot)
{
	if (slot >= MAX_VOW_SPEAKER_MODEL || slot < 0) {
		VOWDRV_DEBUG("%s(), slot id=%d, over range\n", __func__, slot);
		return -EDOM;
	}
	if (copy_from_user((void *)(vowserv.vow_speaker_model[slot].model_ptr),
			   (const void __user *)(vowserv.vow_info_apuser[2]),
			   vowserv.vow_info_apuser[3])) {
		VOWDRV_DEBUG("vow Copy Speaker Model Fail\n");
		return -EFAULT;
	}
	vowserv.vow_speaker_model[slot].flag = 1;
	vowserv.vow_speaker_model[slot].enabled = 0;
	vowserv.vow_speaker_model[slot].id = vowserv.vow_info_apuser[0];
	vowserv.vow_speaker_model[slot].keyword = vowserv.vow_info_apuser[1];
	vowserv.vow_speaker_model[slot].uuid = vowserv.vow_info_apuser[5];
	vowserv.vow_speaker_model[slot].model_size = vowserv.vow_info_apuser[3];

	return 0;
}
#endif  /* #if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT) */

static int vow_service_FindFreeSpeakerModel(void)
{
	int I;

	I = 0;
	VOWDRV_DEBUG("vow FMAX_VOW_SPEAKER_MODEL: %d\n", MAX_VOW_SPEAKER_MODEL);
	do {
		if (vowserv.vow_speaker_model[I].flag == 0)
			break;
		I++;
	} while (I < MAX_VOW_SPEAKER_MODEL);

	VOWDRV_DEBUG("vow FindFreeSpeakerModel:%d\n", I);

	if (I == MAX_VOW_SPEAKER_MODEL) {
		VOWDRV_DEBUG("vow Find Free Speaker Model Fail\n");
		return -1;
	}
	return I;
}

static int vow_service_SearchSpeakerModelWithId(int id)
{
	int I;

	I = 0;
	do {
		if (vowserv.vow_speaker_model[I].id == id)
			break;
		I++;
	} while (I < MAX_VOW_SPEAKER_MODEL);

	if (I == MAX_VOW_SPEAKER_MODEL) {
		VOWDRV_DEBUG("vow Search Speaker Model By ID Fail:%x\n", id);
		return -1;
	}
	return I;
}

static int vow_service_SearchSpeakerModelWithKeyword(int keyword)
{
	int I;

	I = 0;
	do {
		if (vowserv.vow_speaker_model[I].keyword == keyword) {
			VOWDRV_DEBUG("vow Search Speaker Model By Keyword Success !, keyword:%x\n",
				     keyword);
			break;
		}
		I++;
	} while (I < MAX_VOW_SPEAKER_MODEL);

	if (I == MAX_VOW_SPEAKER_MODEL) {
		return -1;
	}
	return I;
}

static bool vow_service_SendSpeakerModel(int slot, bool release_flag)
{
	int ipi_ret;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	unsigned int vow_ipi_buf[5];

	if (slot >= MAX_VOW_SPEAKER_MODEL || slot < 0) {
		VOWDRV_DEBUG("%s(), slot id=%d, over range\n", __func__, slot);
		return false;
	}

	if (release_flag == VOW_CLEAN_MODEL) {
		if (vowserv.vow_speaker_model[slot].flag == 0) {
			VOWDRV_DEBUG("%s(), slot:%d, no model need to clean\n",
				     __func__, slot);
			return false;
		}
		vow_ipi_buf[0] = VOW_MODEL_CLEAR;
	} else {  /* VOW_SET_MODEL */
		if ((vowserv.vow_speaker_model[slot].flag == 0) ||
		    (vowserv.vow_speaker_model[slot].id == -1) ||
		    (vowserv.vow_speaker_model[slot].keyword == -1) ||
		    (vowserv.vow_speaker_model[slot].uuid == 0) ||
		    (vowserv.vow_speaker_model[slot].model_size == 0)) {
			VOWDRV_DEBUG("%s(), slot:%d, no model need to set\n",
				     __func__, slot);
			return false;
		}
		vow_ipi_buf[0] = VOW_MODEL_SPEAKER;
	}
	vow_ipi_buf[1] = vowserv.vow_speaker_model[slot].uuid;
	vow_ipi_buf[2] = slot;
	vow_ipi_buf[3] = vowserv.vow_speaker_model[slot].model_size;
	vow_ipi_buf[4] = vowserv.vow_speaker_model[slot].keyword;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_DEBUG_SUPPORT)
	VOWDRV_DEBUG(
	    "Model:slot_%d, model_%x, addr_%x, id_%x, size_%x, keyword_%x\n",
		      slot,
		      vow_ipi_buf[0],
		      vow_ipi_buf[2],
		      vow_ipi_buf[1],
		      vow_ipi_buf[3],
		      vow_ipi_buf[4]);
#endif

	ipi_ret = vow_ipi_send(IPIMSG_VOW_SET_MODEL,
			       5,
			       &vow_ipi_buf[0],
			       VOW_IPI_BYPASS_ACK);
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
	if ((ipi_ret == IPI_SCP_DIE) ||
	    (ipi_ret == IPI_SCP_SEND_PASS) ||
	    (ipi_ret == IPI_SCP_RECOVERING))
		return true;
	// IPI_SCP_SEND_FAIL, IPI_SCP_NO_SUPPORT
	return false;
}

static bool vow_service_ReleaseSpeakerModel(int id)
{
	int I;
	bool ret = false;

	I = vow_service_SearchSpeakerModelWithId(id);

	if (I == -1) {
		VOWDRV_DEBUG("vow release Speaker Model Fail, id:%x\n", id);
		return false;
	}
	VOWDRV_DEBUG("vow ReleaseSpeakerModel:id_%x, slot_%d\n", id, I);

	ret = vow_service_SendSpeakerModel(I, VOW_CLEAN_MODEL);

	mutex_lock(&vow_sendspkmdl_mutex);
	vowserv.vow_speaker_model[I].model_ptr = NULL;
	vowserv.vow_speaker_model[I].uuid = 0;
	vowserv.vow_speaker_model[I].id = -1;
	vowserv.vow_speaker_model[I].keyword = -1;
	vowserv.vow_speaker_model[I].flag = 0;
	vowserv.vow_speaker_model[I].enabled = 0;
	mutex_unlock(&vow_sendspkmdl_mutex);

	return ret;
}

static bool vow_service_SetSpeakerModel(unsigned long arg)
{
	bool ret = false;
	int I;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	char *ptr8;
#endif

	I = vow_service_FindFreeSpeakerModel();
	if (I == -1)
		return false;

	if (vow_service_GetParameter(arg) != 0)
		return false;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	mutex_lock(&vow_sendspkmdl_mutex);
	vowserv.vow_speaker_model[I].model_ptr =
	   (void *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
	   + (VOW_MODEL_SIZE * I);
	mutex_unlock(&vow_sendspkmdl_mutex);

	if (vow_service_CopyModel(I) != 0)
		return false;

	mutex_lock(&vow_sendspkmdl_mutex);
	ptr8 = (char *)vowserv.vow_speaker_model[I].model_ptr;
	VOWDRV_DEBUG("SetSPKModel:slot: %d, ID: %x, keyword: %x, UUID: %x, flag: %x\n",
		      I,
		      vowserv.vow_speaker_model[I].id,
		      vowserv.vow_speaker_model[I].keyword,
		      vowserv.vow_speaker_model[I].uuid,
		      vowserv.vow_speaker_model[I].flag);
	VOWDRV_DEBUG("vow SetSPKModel:CheckValue:%x %x %x %x %x %x\n",
		      *(char *)&ptr8[0], *(char *)&ptr8[1],
		      *(char *)&ptr8[2], *(char *)&ptr8[3],
		      *(short *)&ptr8[160], *(int *)&ptr8[7960]);
	mutex_unlock(&vow_sendspkmdl_mutex);

	ret = vow_service_SendSpeakerModel(I, VOW_SET_MODEL);
	/* if setup speaker model fail, then just clean this model information */
	if (ret == false) {
		VOWDRV_DEBUG("vow setup speaker model fail, then ignore this load model\n");
		mutex_lock(&vow_sendspkmdl_mutex);
		vowserv.vow_speaker_model[I].model_ptr = NULL;
		vowserv.vow_speaker_model[I].uuid = 0;
		vowserv.vow_speaker_model[I].id = -1;
		vowserv.vow_speaker_model[I].keyword = -1;
		vowserv.vow_speaker_model[I].flag = 0;
		vowserv.vow_speaker_model[I].enabled = 0;
		mutex_unlock(&vow_sendspkmdl_mutex);
	}
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
	return ret;
}

static bool vow_service_SetCustomModel(unsigned long arg)
{
	int ipi_ret;
	struct vow_engine_info_t info;
	struct vow_engine_info_t *p_info = &info;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	phys_addr_t p_virt;
	phys_addr_t p_mdl_v;
	uint32_t data_size = 0;
	unsigned int vow_ipi_buf[1];
#endif

	if (copy_from_user((void *)&info,
			   (const void __user *)arg,
			   sizeof(struct vow_engine_info_t))) {
		VOWDRV_DEBUG("vow get cust info fail\n");
		return false;
	}
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	if (p_info->return_size_addr == 0 || p_info->data_addr == 0) {
		VOWDRV_DEBUG("vow get cust info fail 2\n");
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_DEBUG_SUPPORT)
		VOWDRV_DEBUG("return_size_addr %lu data_addr %lu",
		p_info->return_size_addr,
		p_info->data_addr);
#endif
		return false;
	}
	p_virt = scp_get_reserve_mem_virt(VOW_MEM_ID);
	p_mdl_v = p_virt + VOW_CUSTOM_MODEL_OFFSET;
	if (copy_from_user((void *)&data_size,
			   (const void __user *)p_info->return_size_addr,
			   sizeof(uint32_t))) {
		VOWDRV_DEBUG("vow get cust size fail\n");
		return false;
	}
	if (data_size > VOW_MAX_CUST_MODEL_SIZE || data_size <= 0) {
		VOWDRV_DEBUG("vow set cust model fail, invalid size %d\n", data_size);
		return false;
	}
	VOWDRV_DEBUG("vow set cust model, size %d\n", data_size);
	if (copy_from_user((void *)p_mdl_v,
			   (const void __user *)p_info->data_addr,
			   data_size)) {
		VOWDRV_DEBUG("vow copy cust model fail\n");
		return false;
	}

	vowserv.custom_model_size = (unsigned long)data_size;
	vow_ipi_buf[0] = (unsigned long)data_size;
	ipi_ret = vow_ipi_send(IPIMSG_VOW_SET_CUSTOM_MODEL,
			       1,
			       &vow_ipi_buf[0],
			       VOW_IPI_BYPASS_ACK);
#else
	VOWDRV_DEBUG("vow SCP is not supported\n");
#endif
	if ((ipi_ret == IPI_SCP_DIE) ||
	    (ipi_ret == IPI_SCP_SEND_PASS) ||
	    (ipi_ret == IPI_SCP_RECOVERING))
		return true;
	// IPI_SCP_SEND_FAIL, IPI_SCP_NO_SUPPORT
	return false;
}

static bool vow_service_SendModelStatus(int slot, bool enable)
{
	int ipi_ret;
	unsigned int vow_ipi_buf[2];

	if (slot >= MAX_VOW_SPEAKER_MODEL || slot < 0) {
		VOWDRV_DEBUG("%s(), slot id=%d, over range\n", __func__, slot);
		return false;
	}
	if (!vowserv.vow_speaker_model[slot].flag) {
		VOWDRV_DEBUG("%s(), this speaker model isn't load\n", __func__);
		return false;
	}

	vow_ipi_buf[0] = vowserv.vow_speaker_model[slot].keyword;
	vow_ipi_buf[1] = vowserv.vow_speaker_model[slot].confidence_lv;

	if (enable == VOW_MODEL_STATUS_START) {
		ipi_ret = vow_ipi_send(IPIMSG_VOW_MODEL_START,
				   2,
				   &vow_ipi_buf[0],
				   VOW_IPI_BYPASS_ACK);
	} else {  /* VOW_MODEL_STATUS_STOP */
		ipi_ret = vow_ipi_send(IPIMSG_VOW_MODEL_STOP,
				   2,
				   &vow_ipi_buf[0],
				   VOW_IPI_BYPASS_ACK);
	}

	if ((ipi_ret == IPI_SCP_DIE) ||
	    (ipi_ret == IPI_SCP_SEND_PASS) ||
	    (ipi_ret == IPI_SCP_RECOVERING))
		return true;
	// IPI_SCP_SEND_FAIL, IPI_SCP_NO_SUPPORT
	return false;
}

static void vow_register_vendor_feature(int uuid)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	switch (uuid) {
	case VENDOR_ID_MTK:
		vow_register_feature(VOW_VENDOR_M_FEATURE_ID);
		break;
	case VENDOR_ID_AMAZON:
		vow_register_feature(VOW_VENDOR_A_FEATURE_ID);
		break;
	case VENDOR_ID_OTHERS:
		vow_register_feature(VOW_VENDOR_G_FEATURE_ID);
		break;
	default:
		VOWDRV_DEBUG("VENDOR ID not support\n");
		break;
	}
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
}

static void vow_deregister_vendor_feature(int uuid)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	switch (uuid) {
	case VENDOR_ID_MTK:
		vow_deregister_feature(VOW_VENDOR_M_FEATURE_ID);
		break;
	case VENDOR_ID_AMAZON:
		vow_deregister_feature(VOW_VENDOR_A_FEATURE_ID);
		break;
	case VENDOR_ID_OTHERS:
		vow_deregister_feature(VOW_VENDOR_G_FEATURE_ID);
		break;
	default:
		VOWDRV_DEBUG("VENDOR ID not support\n");
		break;
	}
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
}

static bool vow_service_SetModelStatus(bool enable, unsigned long arg)
{
	bool ret = false;
	int slot;
	struct vow_model_start_t model_start;

	if (copy_from_user((void *)&model_start,
			   (const void __user *)(arg),
			   sizeof(struct vow_model_start_t))) {
		VOWDRV_DEBUG("vow get vow_model_start data fail\n");
		return false;
	}

	if (model_start.handle > INT_MAX || model_start.handle < INT_MIN) {
		VOWDRV_DEBUG("%s(), model_start.handle will cause truncated value\n",
					__func__);
		return false;
	}

	slot = vow_service_SearchSpeakerModelWithId(model_start.handle);
	if (slot < 0) {
		VOWDRV_DEBUG("%s(), no match id\n", __func__);
		return false;
	}
	if (enable == VOW_MODEL_STATUS_START) {
		if (vowserv.vow_speaker_model[slot].enabled == 0) {
			int uuid;

			uuid = vowserv.vow_speaker_model[slot].uuid;
			vow_register_vendor_feature(uuid);
		}
		vowserv.vow_speaker_model[slot].enabled = 1;
		vowserv.vow_speaker_model[slot].confidence_lv =
			model_start.confidence_level;
		vowserv.vow_speaker_model[slot].rx_inform_addr =
			model_start.dsp_inform_addr;
		vowserv.vow_speaker_model[slot].rx_inform_size_addr =
			model_start.dsp_inform_size_addr;
		VOWDRV_DEBUG("VOW_MODEL_START, id:%d, enabled:%d, conf_lv:%d\n",
			     (int)model_start.handle,
			     vowserv.vow_speaker_model[slot].enabled,
			     vowserv.vow_speaker_model[slot].confidence_lv);
		/* send model status to scp */
		ret = vow_service_SendModelStatus(slot, enable);
		if (ret == false)
			VOWDRV_DEBUG("vow_service_SendModelStatus, err\n");
	} else {  /* VOW_MODEL_STATUS_STOP */
		vowserv.vow_speaker_model[slot].confidence_lv =
			model_start.confidence_level;
		vowserv.vow_speaker_model[slot].rx_inform_addr =
			model_start.dsp_inform_addr;
		vowserv.vow_speaker_model[slot].rx_inform_size_addr =
			model_start.dsp_inform_size_addr;
		/* send model status to scp */
		ret = vow_service_SendModelStatus(slot, enable);
		if (ret == false)
			VOWDRV_DEBUG("vow_service_SendModelStatus, err\n");
		if (vowserv.vow_speaker_model[slot].enabled == 1) {
			int uuid;

			uuid = vowserv.vow_speaker_model[slot].uuid;
			vow_deregister_vendor_feature(uuid);
		}
		vowserv.vow_speaker_model[slot].enabled = 0;
		VOWDRV_DEBUG("VOW_MODEL_STOP, id:%d\n",
			     (int)model_start.handle);
	}
	return ret;
}

static bool vow_service_SetApDumpAddr(unsigned long arg)
{
	unsigned long vow_info[MAX_VOW_INFO_LEN];
	unsigned long id, flags;

	if (copy_from_user((void *)(&vow_info[0]), (const void __user *)(arg),
			   sizeof(vowserv.vow_info_apuser))) {
		VOWDRV_DEBUG("%s()check Ap dump parameter fail\n", __func__);
		return false;
	}

	/* add return condition */
	if ((vow_info[2] == 0) || ((vow_info[3] != kReadVowDumpSize) &&
	    (vow_info[3] != kReadVowDumpSize_Big)) || (vow_info[4] == 0) ||
	    (vow_info[0] >= NUM_DUMP_DATA)) {
		VOWDRV_DEBUG("%s(), vow SetdumpAddr error!\n", __func__);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_DEBUG_SUPPORT)
		VOWDRV_DEBUG("%s(): error id %d, addr_%x, size_%x, addr_%x\n",
		 __func__,
		 (unsigned int)vow_info[0],
		 (unsigned int)vow_info[2],
		 (unsigned int)vow_info[3],
		 (unsigned int)vow_info[4]);
#endif
		return false;
	}
	id = vow_info[0];
	spin_lock_irqsave(&vowdrv_dump_lock, flags);
	vow_dump_info[id].user_dump_addr = vow_info[2];
	vow_dump_info[id].user_dump_size = vow_info[3];
	vow_dump_info[id].user_return_size_addr = vow_info[4];
	vow_dump_info[id].user_dump_idx = 0;
	spin_unlock_irqrestore(&vowdrv_dump_lock, flags);
	//verb log
	//VOWDRV_DEBUG("%s(): id %d, addr_%x, size_%x, addr_%x\n",
	//	 __func__,
	//	 id,
	//	 (unsigned int)vow_info[2],
	//	 (unsigned int)vow_info[3],
	//	 (unsigned int)vow_info[4]);
	return true;
}

static bool vow_service_SetApAddr(unsigned long arg)
{
	unsigned long vow_info[MAX_VOW_INFO_LEN];
	unsigned int vow_vbuf_length = 0;

	if (dual_ch_transfer == false)
		vow_vbuf_length = VOW_VBUF_LENGTH;
	else
		vow_vbuf_length = VOW_VBUF_LENGTH * 2;

	if (copy_from_user((void *)(&vow_info[0]), (const void __user *)(arg),
			   sizeof(vow_info))) {
		VOWDRV_DEBUG("vow check parameter fail\n");
		return false;
	}

	/* add return condition */
	if ((vow_info[2] == 0) || (vow_info[3] != vow_vbuf_length) ||
	    (vow_info[4] == 0)) {
		VOWDRV_DEBUG("%s(), vow SetApAddr error!\n", __func__);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_DEBUG_SUPPORT)
		VOWDRV_DEBUG("%s(), vow SetApAddr:addr_%x, size_%x, addr_%x\n",
		 __func__,
		 (unsigned int)vow_info[2],
		 (unsigned int)vow_info[3],
		 (unsigned int)vow_info[4]);
#endif
		return false;
	}

	vowserv.voicedata_user_addr = vow_info[2];
	vowserv.voicedata_user_size = vow_info[3];
	vowserv.voicedata_user_return_size_addr = vow_info[4];

	return true;
}

static bool vow_service_SetVBufAddr(unsigned long arg)
{
	unsigned long vow_info[MAX_VOW_INFO_LEN];
	unsigned int vow_vbuf_length = 0;

	if (copy_from_user((void *)(&vow_info[0]), (const void __user *)(arg),
			   sizeof(vowserv.vow_info_apuser))) {
		VOWDRV_DEBUG("vow check parameter fail\n");
		return false;
	}
	if (dual_ch_transfer == false)
		vow_vbuf_length = VOW_VBUF_LENGTH;
	else
		vow_vbuf_length = VOW_VBUF_LENGTH * 2;

	/* add return condition */
	if ((vow_info[2] == 0) || (vow_info[3] != vow_vbuf_length) ||
	    (vow_info[4] == 0)) {
		VOWDRV_DEBUG("%s(), vow SetVBufAddr error!\n", __func__);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_DEBUG_SUPPORT)
		VOWDRV_DEBUG("vow SetVBufAddr:addr_%x, size_%x, addr_%x\n",
		 (unsigned int)vow_info[2],
		 (unsigned int)vow_info[3],
		 (unsigned int)vow_info[4]);
#endif
		return false;
	}

	mutex_lock(&vow_vmalloc_lock);
	if (vowserv.voicedata_kernel_ptr != NULL) {
		vfree(vowserv.voicedata_kernel_ptr);
		vowserv.voicedata_kernel_ptr = NULL;
	}
	vowserv.voicedata_kernel_ptr = vmalloc(vow_vbuf_length);
	mutex_unlock(&vow_vmalloc_lock);
	return true;
}

static bool vow_service_Enable(void)
{
	int ipi_ret;

	VOWDRV_DEBUG("+%s()\n", __func__);
	ipi_ret = vow_ipi_send(IPIMSG_VOW_ENABLE,
			       0,
			       NULL,
			       VOW_IPI_BYPASS_ACK);

	//VOWDRV_DEBUG("-%s():%d\n", __func__, ret);

	if ((ipi_ret == IPI_SCP_DIE) ||
	    (ipi_ret == IPI_SCP_SEND_PASS) ||
	    (ipi_ret == IPI_SCP_RECOVERING))
		return true;
	// IPI_SCP_SEND_FAIL, IPI_SCP_NO_SUPPORT
	return false;
}

static bool vow_service_Disable(void)
{
	int ipi_ret;

	VOWDRV_DEBUG("+%s()\n", __func__);

	ipi_ret = vow_ipi_send(IPIMSG_VOW_DISABLE,
			       0,
			       NULL,
			       VOW_IPI_BYPASS_ACK);

	/* release lock */
	if (vowserv.suspend_lock == 1) {
		vowserv.suspend_lock = 0;
		/* Let AP will suspend */
		__pm_relax(vow_suspend_lock);
	}
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	vow_deregister_feature(VOW_FEATURE_ID);
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
	if ((ipi_ret == IPI_SCP_DIE) ||
	    (ipi_ret == IPI_SCP_SEND_PASS) ||
	    (ipi_ret == IPI_SCP_RECOVERING))
		return true;
	// IPI_SCP_SEND_FAIL, IPI_SCP_NO_SUPPORT
	return false;
}

static void vow_check_boundary(unsigned int copy_len, unsigned int bound_len)
{
	if (copy_len > bound_len) {
		VOWDRV_DEBUG("[vow check]copy_len=0x%x, bound_len=0x%x\n",
			     copy_len, bound_len);
		VOW_ASSERT(0);
	}
}

/* for payload dump feature(data from scp) */
static void vow_service_ReadPayloadDumpData(unsigned int buf_length)
{
	unsigned int payloaddump_len = 0;

	if (vowserv.payloaddump_enable == false) {
		VOWDRV_DEBUG("%s(), payloaddump OFF\n", __func__);
		return;
	}
	if (vowserv.payloaddump_kernel_ptr == NULL) {
		VOWDRV_DEBUG("%s(), payloaddump_kernel_ptr is NULL!!\n", __func__);
		return;
	}
	if (vowserv.payloaddump_user_max_size == 0) {
		VOWDRV_DEBUG("%s(), MAX len = 0 !!\n", __func__);
		return;
	}
	if (dual_ch_transfer == false)
		payloaddump_len = buf_length;
	else
		payloaddump_len = buf_length * 2;
	if ((unsigned long)payloaddump_len > vowserv.payloaddump_user_max_size) {
		VOWDRV_DEBUG("%s(), [VOW PDR] buf_len=0x%x, MAX len=0x%lx\n",
			__func__, payloaddump_len, vowserv.payloaddump_user_max_size);
		return;
	}

	// copy from DRAM to get payload data
	mutex_lock(&vow_payloaddump_mutex);
	if (dual_ch_transfer == false) {
		memcpy(&vowserv.payloaddump_kernel_ptr[0],
		       vowserv.payloaddump_scp_ptr, payloaddump_len);
	} else {
		/* start interleaving L+R */
		vow_interleaving(&vowserv.payloaddump_kernel_ptr[0],
				(short *)(vowserv.payloaddump_scp_ptr),
				(short *)(vowserv.payloaddump_r_scp_ptr), buf_length);
	}
	mutex_unlock(&vow_payloaddump_mutex);

	//copy to user space
	if (copy_to_user(
		(void __user *)(vowserv.payloaddump_user_return_size_addr),
		&payloaddump_len,
		sizeof(unsigned int)))
		VOWDRV_DEBUG("%s(), copy payloaddump_len fail", __func__);

	mutex_lock(&vow_payloaddump_mutex);
	if (copy_to_user(
		(void __user *)vowserv.payloaddump_user_addr,
		vowserv.payloaddump_kernel_ptr,
		payloaddump_len))
		VOWDRV_DEBUG("%s(), copy payloaddump_kernel_ptr fail", __func__);

	mutex_unlock(&vow_payloaddump_mutex);
}

static void vow_service_SetDualChannelTransfer(unsigned long arg)
{
	//arg = 0: dual ch transfer is off, arg = 1: dual ch transfer is on
	if (arg > 1)
		VOWDRV_DEBUG("dual ch transfer setting error, %lu", arg);

	dual_ch_transfer = (bool)arg;
}

static bool vow_service_SetDelayWakeupTime(unsigned int arg)
{
	if (arg > 1000) {
		VOWDRV_DEBUG("delay wakeup time setting error, %d", arg);
		return false;
	}
	VowDrv_SetFlag(VOW_FLAG_WAKEUP_DELAY_TIME, arg);
	return true;
}

static bool vow_service_SetPayloaddump_callback(unsigned int arg)
{
	if (arg >= PAYLOADDUMP_MAX_NUM) {
		VOWDRV_DEBUG("payload dump callback setting error, %d", arg);
		return false;
	}
	VowDrv_SetFlag(VOW_FLAG_PAYLOADDUMP_CB_TYPE, arg);
	return true;
}

static int vow_service_ReadVoiceData_Internal(void)
{
	int stop_condition = 0;
	unsigned int vow_vbuf_length = 0;
	unsigned int buf_offset = 0;
	unsigned int buf_length = 0;
	struct vow_rec_queue_info_t *rec_queue;
	unsigned long rec_queue_flags;

	if (dual_ch_transfer == false)
		vow_vbuf_length = VOW_VBUF_LENGTH;
	else
		vow_vbuf_length = VOW_VBUF_LENGTH * 2;

	while(1) {
		spin_lock_irqsave(&vow_rec_queue_lock, rec_queue_flags);
		rec_queue = vow_get_rec_ipi_queue();
		spin_unlock_irqrestore(&vow_rec_queue_lock, rec_queue_flags);
		if (rec_queue == NULL) {
			/*VOWDRV_DEBUG("%s(), wait for next IPI", __func__);*/
			break;
		}
		/*VOWDRV_DEBUG("%s(), rec_queue: offset=0x%x, length=0x%x",*/
		/*	     __func__,*/
		/*	     rec_queue->rec_buf_offset, rec_queue->rec_buf_length);*/
		buf_offset = rec_queue->rec_buf_offset;
		buf_length = rec_queue->rec_buf_length;
		if (buf_length != 0) {
			unsigned int buffer_bound = 0;

			if ((vowserv.kernel_voicedata_idx + (buf_length >> 1))
			    > (vowserv.voicedata_user_size >> 1)) {
				VOWDRV_DEBUG(
				"[vow check]data_idx=0x%x(W), buf_length=0x%x(B)\n",
					vowserv.kernel_voicedata_idx, buf_length);
				VOWDRV_DEBUG(
				"[vow check] user_size=0x%x(B), buf_offset=0x%x(B)\n",
					(unsigned int)vowserv.voicedata_user_size,
					buf_offset);
				/* VOW_ASSERT(0); */
				mutex_lock(&vow_vmalloc_lock);
				vowserv.kernel_voicedata_idx = 0;
				mutex_unlock(&vow_vmalloc_lock);
			}

			if ((vowserv.kernel_voicedata_idx + buf_length) > vow_vbuf_length) {
				VOWDRV_DEBUG(
				"%s(), voicedata_idx=0x%x, buf_length=0x%x, vbuf_length=0x%x",
					__func__,
					vowserv.kernel_voicedata_idx,
					buf_length,
					vow_vbuf_length);
				stop_condition = 1;
				return stop_condition;
			}

			if (dual_ch_transfer == true)
				buffer_bound = VOW_MAX_MIC_NUM * VOW_VOICEDATA_SIZE;
			else
				buffer_bound = VOW_VOICEDATA_SIZE;

			if (buf_offset > buffer_bound) {
				VOWDRV_DEBUG(
				"%s(), buf_offset=0x%x, buf_length=0x%x, VOICEDATA_SZ=0x%x\n",
					__func__,
					buf_offset,
					buf_length,
					VOW_VOICEDATA_SIZE);
				stop_condition = 1;
				return stop_condition;
			}
			mutex_lock(&vow_vmalloc_lock);
			if (dual_ch_transfer == true) {
				/* start interleaving L+R */
				vow_interleaving(
				    &vowserv.voicedata_kernel_ptr[vowserv.kernel_voicedata_idx],
				    (short *)(vowserv.voicedata_scp_ptr + buf_offset),
				    (short *)(vowserv.voicedata_scp_ptr + buf_offset +
				    VOW_VOICEDATA_SIZE), buf_length);
				/* end interleaving*/
			} else {
				memcpy(
				    &vowserv.voicedata_kernel_ptr[vowserv.kernel_voicedata_idx],
				    vowserv.voicedata_scp_ptr + buf_offset, buf_length);
			}
			mutex_unlock(&vow_vmalloc_lock);
			if (buf_length > VOW_VOICE_RECORD_BIG_THRESHOLD) {
				/* means now is start to transfer */
				/* keyword buffer(64kB) to AP */
				VOWDRV_DEBUG("%s(), start tx keyword, 0x%x/0x%x\n",
					__func__,
					vowserv.kernel_voicedata_idx,
					buf_length);
				vowserv.tx_keyword_start = true;
			}
			mutex_lock(&vow_vmalloc_lock);
			if (dual_ch_transfer == true) {
				/* 2 Channels */
				vowserv.kernel_voicedata_idx += buf_length;
			} else {
				/* 1 Channel */
				vowserv.kernel_voicedata_idx += (buf_length >> 1);
			}
			mutex_unlock(&vow_vmalloc_lock);
		}
	} // while(1)
	if (vowserv.kernel_voicedata_idx >= (VOW_VOICE_RECORD_BIG_THRESHOLD >> 1))
		vowserv.transfer_length = VOW_VOICE_RECORD_BIG_THRESHOLD;
	else
		vowserv.transfer_length = VOW_VOICE_RECORD_THRESHOLD;

	if (vowserv.kernel_voicedata_idx >= (vowserv.transfer_length >> 1)) {

		/*VOWDRV_DEBUG("TX Leng:%d, %d, [%d]\n",*/
		/*	     vowserv.kernel_voicedata_idx,*/
		/*	     buf_length,*/
		/*	     vowserv.transfer_length);*/
		if (copy_to_user(
		    (void __user *)(vowserv.voicedata_user_return_size_addr),
		    &vowserv.transfer_length,
		    sizeof(unsigned int)))
			VOWDRV_DEBUG("%s(), copy_to_user fail", __func__);

		mutex_lock(&vow_vmalloc_lock);
		if (copy_to_user(
		    (void __user *)vowserv.voicedata_user_addr,
		    vowserv.voicedata_kernel_ptr,
		    vowserv.transfer_length))
			VOWDRV_DEBUG("%s(), copy_to_user fail", __func__);

		mutex_unlock(&vow_vmalloc_lock);

		/* move left data to buffer's head */
		if (vowserv.kernel_voicedata_idx > (vowserv.transfer_length >> 1)) {
			unsigned int tmp;
			unsigned int idx;

			tmp = (vowserv.kernel_voicedata_idx << 1)
			      - vowserv.transfer_length; // unit: byte
			idx = (vowserv.transfer_length >> 1); //unit: short
			vow_check_boundary(tmp + (idx << 1), vow_vbuf_length);
			mutex_lock(&vow_vmalloc_lock);
			memmove(&vowserv.voicedata_kernel_ptr[0],
			       &vowserv.voicedata_kernel_ptr[idx],
			       tmp);
			mutex_unlock(&vow_vmalloc_lock);
			mutex_lock(&vow_vmalloc_lock);
			vowserv.kernel_voicedata_idx -= idx; // unit: short
			mutex_unlock(&vow_vmalloc_lock);
		} else {
			mutex_lock(&vow_vmalloc_lock);
			vowserv.kernel_voicedata_idx = 0;
			mutex_unlock(&vow_vmalloc_lock);
		}

		/* speed up voicedata transfer to hal */
		if (vowserv.kernel_voicedata_idx >= VOW_VOICE_RECORD_THRESHOLD)
			vow_service_getVoiceData();

		stop_condition = 1;
	}
	if ((vowserv.tx_keyword_start == true) &&
	    (vowserv.kernel_voicedata_idx < VOW_VOICE_RECORD_THRESHOLD)) {
		/* means now is end of transfer keyword buffer(64kB) to AP */
		vowserv.tx_keyword_start = false;
		VOWDRV_DEBUG("%s(), end tx keyword, 0x%x\n",
			     __func__,
			     vowserv.kernel_voicedata_idx);
	}

	return stop_condition;
}

static void vow_service_GetVowDumpData(void)
{
	unsigned int size = 0, i = 0, idx = 0, user_size = 0;
	unsigned long flags;

	if (vowserv.dump_pcm_flag == false)
		return;
	for (i = 0; i < NUM_DUMP_DATA; i++) {
		//copy scp shared buffer to kernel
		struct vow_dump_info_t temp_dump_info;

		temp_dump_info = vow_dump_info[i];
		idx = temp_dump_info.kernel_dump_idx;
		if (temp_dump_info.vir_addr != NULL && temp_dump_info.scp_dump_size[0] != 0) {
			if ((i != DUMP_BARGEIN && vowserv.vow_mic_number == 2
				&& temp_dump_info.scp_dump_size[1] != 0) ||
			    (i == DUMP_BARGEIN && vowserv.vow_speaker_number == 2
				&& temp_dump_info.scp_dump_size[1] != 0)) {
				/* DRAM to kernel buffer and sample interleaving */
				if ((idx + (temp_dump_info.scp_dump_size[0] * 2)) >
				    temp_dump_info.kernel_dump_size) {
					size = temp_dump_info.kernel_dump_size - idx;
					VOWDRV_DEBUG("%s(), WARNING2 idx %d + %d, %d\n",
						     __func__,
						     idx, temp_dump_info.scp_dump_size[0] * 2,
						     temp_dump_info.kernel_dump_size);
				} else {
					size = temp_dump_info.scp_dump_size[0] * 2;
				}
				vow_interleaving(
					&temp_dump_info.kernel_dump_addr[idx],
					(short *)(temp_dump_info.vir_addr +
						temp_dump_info.scp_dump_offset[0]),
					(short *)(temp_dump_info.vir_addr +
						temp_dump_info.scp_dump_offset[1]),
					size / 2);
				idx += size;
			} else { // DRAM to kernel buffer. (only 1 channel)
				if ((idx + temp_dump_info.scp_dump_size[0]) >
				    temp_dump_info.kernel_dump_size) {
					size = temp_dump_info.kernel_dump_size - idx;
					VOWDRV_DEBUG("%s(), WARNING1 idx %d + %d, %d\n",
						     __func__,
						     idx, temp_dump_info.scp_dump_size[0] * 2,
						     temp_dump_info.kernel_dump_size);
				} else {
					size = temp_dump_info.scp_dump_size[0];
				}
				memcpy(&temp_dump_info.kernel_dump_addr[idx],
					temp_dump_info.vir_addr +
					temp_dump_info.scp_dump_offset[0],
					size);
				idx += size;
			}
			/*{*/
			/*	short *outPtr;*/
			/*	outPtr = (short *)temp_dump_info.kernel_dump_addr;*/
			/*	VOWDRV_DEBUG(*/
			/*	"%s(), %d idx %d size %d 0x%x, [scp]offset: 0x%x, sz: 0x%x\n",*/
			/*		     __func__, i, idx, size, outPtr,*/
			/*		     temp_dump_info.scp_dump_offset[0],*/
			/*		     temp_dump_info.scp_dump_size[0]);*/
			/*}*/
			/* copy kernel to user space */
			if ((temp_dump_info.user_dump_size != kReadVowDumpSize) &&
			    (temp_dump_info.user_dump_size != kReadVowDumpSize_Big)) {
				VOWDRV_DEBUG("%s(), return, user_dump_size=0x%x",
					     __func__,
					     (unsigned int)temp_dump_info.user_dump_size);
				return;
			}
			if (((temp_dump_info.user_dump_idx + idx) > temp_dump_info.user_dump_size)
			  && (temp_dump_info.user_dump_idx <= temp_dump_info.user_dump_size)) {
				size = temp_dump_info.user_dump_size -
				       temp_dump_info.user_dump_idx;
			} else {
				size = idx;
			}

			mutex_lock(&vow_vmalloc_lock);
			if (copy_to_user(
				(void __user *)(temp_dump_info.user_dump_addr +
				temp_dump_info.user_dump_idx),
				temp_dump_info.kernel_dump_addr,
				size))
				VOWDRV_DEBUG("%s(), copy_to_user fail", __func__);

			mutex_unlock(&vow_vmalloc_lock);
			temp_dump_info.user_dump_idx += size;
			user_size = (unsigned int)temp_dump_info.user_dump_idx;
			if (copy_to_user(
				(void __user *)(temp_dump_info.user_return_size_addr),
				&user_size,
				sizeof(unsigned int)))
				VOWDRV_DEBUG("%s(), copy_to_user fail", __func__);

			/*VOWDRV_DEBUG("%s(), %d, kernel_dump_addr: 0x%x, user_dump_idx: 0x%x\n",*/
			/*	     __func__, i,*/
			/*	     temp_dump_info.kernel_dump_addr,*/
			/*	     temp_dump_info.user_dump_idx);*/
			/* if there are left kernel buffer not copied to user buffer, */
			/* move them to buffer's head */
			if (idx > size) {
				unsigned int size_left;
				unsigned int idx_left;

				size_left = idx - size;
				if (temp_dump_info.kernel_dump_size != kReadVowDumpSize) {
					VOWDRV_DEBUG(
						"%s(), kernel_dump_size=%x, kReadVowDumpSize=%x\n",
						__func__,
						temp_dump_info.kernel_dump_size,
						kReadVowDumpSize);
					return;
				}
				vow_check_boundary(idx, temp_dump_info.kernel_dump_size);
				idx_left = size;
				mutex_lock(&vow_vmalloc_lock);
				memmove(&temp_dump_info.kernel_dump_addr[0],
					   &temp_dump_info.kernel_dump_addr[idx_left],
					   size_left);
				mutex_unlock(&vow_vmalloc_lock);
				temp_dump_info.kernel_dump_idx = size_left;
			} else
				temp_dump_info.kernel_dump_idx = 0;
			spin_lock_irqsave(&vowdrv_dump_lock, flags);
			vow_dump_info[i].kernel_dump_idx = temp_dump_info.kernel_dump_idx;
			vow_dump_info[i].user_dump_idx = temp_dump_info.user_dump_idx;
			vow_dump_info[i].scp_dump_size[0] = 0;
			vow_dump_info[i].scp_dump_size[1] = 0;
			spin_unlock_irqrestore(&vowdrv_dump_lock, flags);
			if (temp_dump_info.kernel_dump_idx != 0) {
				VOWDRV_DEBUG("-%s(), %d kernel_idx %d\n",
					 __func__, i, vow_dump_info[i].kernel_dump_idx);
			}
		} //if (temp_dump_info.scp_dump_size[0] != 0)
	} // for (i = 0; i < NUM_DUMP_DATA; i++)
}

static void vow_service_ReadDumpData(void)
{
	int ret = 0;

	if (DumpData_Wait_Queue_flag == 0) {
		ret = wait_event_interruptible_timeout(DumpData_Wait_Queue,
						 DumpData_Wait_Queue_flag,
						 msecs_to_jiffies(100));
		if (ret < 0)
			VOWDRV_DEBUG("wait %s fail, ret=%d\n", __func__, ret);
	}
	if (DumpData_Wait_Queue_flag == 1) {
		DumpData_Wait_Queue_flag = 0;
		if ((VowDrv_GetHWStatus() == VOW_PWR_OFF) ||
		    (vowserv.dump_pcm_flag == false)) {
			VOWDRV_DEBUG(
			    "stop read vow dump data: %d, %d\n",
			    VowDrv_GetHWStatus(),
			    vowserv.dump_pcm_flag);
		} else {
			/* To Read Dump Data from Kernel to User */
			vow_service_GetVowDumpData();
		}
	} else
		VOWDRV_DEBUG("%s, 100ms timeout,break\n", __func__);
}

static void vow_service_ReadVoiceData(void)
{
	int stop_condition = 0;
	int ret = 0;

	while (1) {
		/*VOWDRV_DEBUG("vow wait for intr, flag = %d\n", VoiceData_Wait_Queue_flag);*/
		if (VoiceData_Wait_Queue_flag == 0) {
			ret = wait_event_interruptible_timeout(VoiceData_Wait_Queue,
							 VoiceData_Wait_Queue_flag,
							 msecs_to_jiffies(100));
			if (ret < 0)
				VOWDRV_DEBUG("wait %s fail, ret=%d\n", __func__, ret);
		}

		if (VoiceData_Wait_Queue_flag == 1) {
			VoiceData_Wait_Queue_flag = 0;
			if ((VowDrv_GetHWStatus() == VOW_PWR_OFF) ||
			    (vowserv.recording_flag == false)) {
				mutex_lock(&vow_vmalloc_lock);
				vowserv.kernel_voicedata_idx = 0;
				mutex_unlock(&vow_vmalloc_lock);
				stop_condition = 1;
				VOWDRV_DEBUG(
				    "stop read vow voice data: %d, %d\n",
				    VowDrv_GetHWStatus(),
				    vowserv.recording_flag);
			} else {
				/* To Read Voice Data from Kernel to User */
				stop_condition =
				    vow_service_ReadVoiceData_Internal();
			}
			if (stop_condition == 1)
				break;
		} else {
			VOWDRV_DEBUG("%s, 100ms timeout,break\n", __func__);
			break;
		}
	}
}

static bool vow_service_get_scp_recover(unsigned long arg)
{
	struct vow_scp_recover_info_t scp_recover_temp;
	int ret = 0;

	if (copy_from_user((void *)&scp_recover_temp,
			   (const void __user *)arg,
			   sizeof(struct vow_scp_recover_info_t))) {
		VOWDRV_DEBUG("%s(), copy_from_user fail", __func__);
		return false;
	}
	vowserv.scp_recover_user_return_size_addr = scp_recover_temp.return_event_addr;
	if (vowserv.scp_recover_user_return_size_addr == 0)
		return false;

	if (ScpRecover_Wait_Queue_flag == 0) {
		ret = wait_event_interruptible(ScpRecover_Wait_Queue,
					 ScpRecover_Wait_Queue_flag);
		if (ret < 0)
			VOWDRV_DEBUG("wait %s fail, ret=%d\n", __func__, ret);
	}
	if (ScpRecover_Wait_Queue_flag == 1) {
		ScpRecover_Wait_Queue_flag = 0;
		// copy scp recover event to user
		if (copy_to_user((void __user *)vowserv.scp_recover_user_return_size_addr,
				 &vowserv.scp_recover_data,
				 sizeof(unsigned int))) {
			VOWDRV_DEBUG("%s(), copy_to_user fail", __func__);
		}
	}

	return true;
}

static void vow_hal_reboot(void)
{
	int ipi_ret;

	VOWDRV_DEBUG("%s(), Send VOW_HAL_REBOOT ipi\n", __func__);

	ipi_ret = vow_ipi_send(IPIMSG_VOW_HAL_REBOOT,
			       0,
			       NULL,
			       VOW_IPI_BYPASS_ACK);
	if (ipi_ret != IPI_SCP_SEND_PASS)
		VOWDRV_DEBUG("%s(), IPIMSG send error %d\n", __func__, ipi_ret);
}

static void vow_service_reset(void)
{
	int I;
	bool need_disable_vow = false;
	bool ret = false;

	VOWDRV_DEBUG("+%s()\n", __func__);
	vowserv.scp_command_flag = false;
	vowserv.tx_keyword_start = false;
	vowserv.provider_type = 0;
	vow_hal_reboot();
	for (I = 0; I < MAX_VOW_SPEAKER_MODEL; I++) {
		if (vowserv.vow_speaker_model[I].enabled  == 1) {
			int uuid;

			/* need to close it */
			ret = vow_service_SendModelStatus(
				I, VOW_MODEL_STATUS_STOP);
			if (ret == false)
				VOWDRV_DEBUG("%s(), err\n", __func__);

			uuid = vowserv.vow_speaker_model[I].uuid;
			vow_deregister_vendor_feature(uuid);
			vowserv.vow_speaker_model[I].enabled = 0;
			need_disable_vow = true;
		}
	}
	for (I = 0; I < MAX_VOW_SPEAKER_MODEL; I++) {
		if (vowserv.vow_speaker_model[I].flag  == 1) {
			int id;

			/* need to clear model */
			id = vowserv.vow_speaker_model[I].id;
			vow_service_ReleaseSpeakerModel(id);
		}
	}
	if (need_disable_vow)
		vow_service_Disable();

	VOWDRV_DEBUG("-%s()\n", __func__);
}

static int vow_pcm_dump_notify(bool enable)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	unsigned int vow_ipi_buf[1] = {0};
	int ipi_ret;

	/* dump flag */
	vow_ipi_buf[0] = vowserv.dump_pcm_flag;

	/* if scp reset happened, need re-send PCM dump IPI to SCP again */
	if (enable == true) {
		ipi_ret = vow_ipi_send(IPIMSG_VOW_PCM_DUMP_ON,
				       1,
				       &vow_ipi_buf[0],
				       VOW_IPI_BYPASS_ACK);
		if (ipi_ret != IPI_SCP_SEND_PASS)
			VOWDRV_DEBUG("%s(), IPIMSG_PCM_DUMP_ON send error %d\n", __func__, ipi_ret);
	} else {
		ipi_ret = vow_ipi_send(IPIMSG_VOW_PCM_DUMP_OFF,
				       1,
				       &vow_ipi_buf[0],
				       VOW_IPI_BYPASS_ACK);
		if (ipi_ret != IPI_SCP_SEND_PASS)
			VOWDRV_DEBUG("%s(), IPIMSG_PCM_DUMP_OFF send error %d\n", __func__, ipi_ret);
	}
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif  /* #if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT) */
	return 0;
}

static bool vow_service_AllocKernelDumpBuffer(void)
{
	unsigned int I = 0;

	mutex_lock(&vow_vmalloc_lock);
	for (I = 0; I < NUM_DUMP_DATA; I++) {
		if (vow_dump_info[I].kernel_dump_addr != NULL) {
			vfree(vow_dump_info[I].kernel_dump_addr);
			vow_dump_info[I].kernel_dump_addr = NULL;
		}
		if ((I == DUMP_AECOUT) || (I == DUMP_BARGEIN) || (I == DUMP_INPUT)) {
			vow_dump_info[I].kernel_dump_addr = vmalloc(kReadVowDumpSize);
			vow_dump_info[I].kernel_dump_size = kReadVowDumpSize;
		} else {
			vow_dump_info[I].kernel_dump_addr = vmalloc(kReadVowDumpSize_Big);
			vow_dump_info[I].kernel_dump_size = kReadVowDumpSize_Big;
		}
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_DEBUG_SUPPORT)
		VOWDRV_DEBUG("%s vow_dump_info[%d].addr = 0x%p, size = 0x%x\n",
			     __func__, I,
			     vow_dump_info[I].kernel_dump_addr,
			     vow_dump_info[I].kernel_dump_size);
#endif
		VOW_ASSERT(vow_dump_info[I].kernel_dump_addr != NULL);
		vow_dump_info[I].kernel_dump_idx = 0;
	}
	mutex_unlock(&vow_vmalloc_lock);
	return true;
}

static bool vow_service_FreeKernelDumpBuffer(void)
{
	unsigned int I = 0;

	mutex_lock(&vow_vmalloc_lock);
	for (I = 0; I < NUM_DUMP_DATA; I++) {
		if (vow_dump_info[I].kernel_dump_addr != NULL) {
			vfree(vow_dump_info[I].kernel_dump_addr);
			vow_dump_info[I].kernel_dump_addr = NULL;
		}
		vow_dump_info[I].kernel_dump_idx = 0;
		vow_dump_info[I].kernel_dump_size = 0;
	}
	mutex_unlock(&vow_vmalloc_lock);
	return true;
}

static int vow_pcm_dump_set(bool enable)
{
	VOWDRV_DEBUG("%s = %d, %d\n", __func__,
		     vowserv.dump_pcm_flag,
		     (unsigned int)enable);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	vow_dump_info[DUMP_BARGEIN].vir_addr =
	    (char *)(scp_get_reserve_mem_virt(VOW_BARGEIN_MEM_ID))
	    + VOW_BARGEIN_AFE_MEMIF_MAX_SIZE
		+ BARGEIN_DUMP_TOTAL_BYTE_CNT_MIC;
	vow_dump_info[DUMP_BARGEIN].size =
		BARGEIN_DUMP_TOTAL_BYTE_CNT_MIC + BARGEIN_DUMP_TOTAL_BYTE_CNT_ECHO;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_DEBUG_SUPPORT)
	VOWDRV_DEBUG("[Barge]vir: %p\n",
		     vow_dump_info[DUMP_BARGEIN].vir_addr);
#endif
	// input share same address of echo
	vow_dump_info[DUMP_INPUT].vir_addr =
	    (char *)(scp_get_reserve_mem_virt(VOW_BARGEIN_MEM_ID))
	    + VOW_BARGEIN_AFE_MEMIF_MAX_SIZE;

	vow_dump_info[DUMP_AECOUT].vir_addr =
	    (char *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
	    + VOW_AECOUTDATA_OFFSET;
	vow_dump_info[DUMP_AECOUT].size = AECOUT_DUMP_TOTAL_BYTE_CNT;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_DEBUG_SUPPORT)
	VOWDRV_DEBUG("[aecout]vir: %p\n",
		     vow_dump_info[DUMP_AECOUT].vir_addr);
#endif

	vow_dump_info[DUMP_VFFPOUT].vir_addr =
	    (char *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
	    + VOW_VFFPOUTDATA_OFFSET;
	vow_dump_info[DUMP_VFFPOUT].size = VFFPOUT_DUMP_TOTAL_BYTE_CNT;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_DEBUG_SUPPORT)
	VOWDRV_DEBUG("[vffp out]vir: %p\n",
		     vow_dump_info[DUMP_VFFPOUT].vir_addr);
#endif

	vow_dump_info[DUMP_VFFPIN].vir_addr =
	    (char *)(scp_get_reserve_mem_virt(VOW_MEM_ID))
	    + VOW_VFFPINDATA_OFFSET;
	vow_dump_info[DUMP_VFFPIN].size = VFFPIN_DUMP_TOTAL_BYTE_CNT;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_DEBUG_SUPPORT)
	VOWDRV_DEBUG("[vffp in]vir: %p\n",
		     vow_dump_info[DUMP_VFFPIN].vir_addr);
#endif
	VOWDRV_DEBUG("%s(), pcm set finish\n", __func__);

	if ((vowserv.dump_pcm_flag == false) && (enable == true)) {
		vowserv.dump_pcm_flag = true;
		vow_service_AllocKernelDumpBuffer();
		vow_pcm_dump_notify(true);
		/* Let AP will not suspend */
		__pm_stay_awake(vow_dump_suspend_lock);
	} else if ((vowserv.dump_pcm_flag == true) && (enable == false)) {
		vowserv.dump_pcm_flag = false;
		vow_pcm_dump_notify(false);
		vow_service_FreeKernelDumpBuffer();
		/* Let AP will suspend */
		__pm_relax(vow_dump_suspend_lock);
	}
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif  /* #if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT) */
	return 0;
}

/*****************************************************************************
 * VOW CONTROL FUNCTIONS
 *****************************************************************************/

static int VowDrv_SetHWStatus(int status)
{
	int ret = 0;

	VOWDRV_DEBUG("%s(): set:%x, cur:%x\n",
		     __func__, status, VowDrv_GetHWStatus());
	if ((status < NUM_OF_VOW_PWR_STATUS) && (status >= VOW_PWR_OFF)) {
		spin_lock(&vowdrv_lock);
		vowserv.pwr_status = status;
		spin_unlock(&vowdrv_lock);
	} else {
		VOWDRV_DEBUG("error input: %d\n", status);
		ret = -1;
	}
	return ret;
}

static int VowDrv_GetHWStatus(void)
{
	int ret = 0;

	spin_lock(&vowdrv_lock);
	ret = vowserv.pwr_status;
	spin_unlock(&vowdrv_lock);
	return ret;
}

int VowDrv_EnableHW(int status)
{
	int ret = 0;
	int pwr_status = 0;

	VOWDRV_DEBUG("%s(): %x\n", __func__, status);

	if (status < 0) {
		VOWDRV_DEBUG("%s(), error input: %x\n", __func__, status);
		ret = -1;
	} else {
		pwr_status = (status == 0) ? VOW_PWR_OFF : VOW_PWR_ON;

		if (pwr_status == VOW_PWR_OFF) {
			/* reset the transfer limitation to */
			/* avoid obstructing phase2.5 transferring */
			if (vowserv.tx_keyword_start == true)
				vowserv.tx_keyword_start = false;
		}
		if (pwr_status == VOW_PWR_ON) {
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
			vow_register_feature(VOW_FEATURE_ID);
#else
			VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
			/* clear enter_phase3_cnt */
			vowserv.enter_phase3_cnt = 0;
		}
		VowDrv_SetHWStatus(pwr_status);
	}
	return ret;
}

int VowDrv_ChangeStatus(void)
{
	VowDrv_Wait_Queue_flag = 1;
	wake_up_interruptible(&VowDrv_Wait_Queue);
	return 0;
}

#ifdef VOW_SMART_DEVICE_SUPPORT
void VowDrv_SetSmartDevice(bool enable)
{
	unsigned int eint_num;
	unsigned int ints[2] = {0, 0};
	//unsigned int vow_ipi_buf[2];
	bool ret;

	if (!vow_check_scp_status()) {
		VOWDRV_DEBUG("SCP is off, do not support VOW\n");
		return;
	}

	VOWDRV_DEBUG("%s():%x\n", __func__, enable);
	if (vowserv.node) {
		/* query eint number from device tree */
		ret = of_property_read_u32_array(vowserv.node,
						 "debounce",
						 ints,
						 ARRAY_SIZE(ints));
		if (ret != 0) {
			VOWDRV_DEBUG("%s(), no debounce node, ret=%d\n",
				     __func__, ret);
			return;
		}

		eint_num = ints[0];

		if (enable == false)
			eint_num = 0xFF;
	} else {
		/* no node here */
		VOWDRV_DEBUG("there is no node\n");
	}
}

void VowDrv_SetSmartDevice_GPIO(bool enable)
{
	int ret = 0;

	if (vowserv.node) {
		if (enable == false) {
			VOWDRV_DEBUG("VowDrv_SetSmartDev_gpio:OFF\n");
			ret = pinctrl_select_state(vowserv.pinctrl,
						   vowserv.pins_eint_off);
			if (ret) {
				/* pinctrl setting error */
				VOWDRV_DEBUG(
				"error, can not set gpio vow pins_eint_off\n");
			}
		} else {
			VOWDRV_DEBUG("VowDrv_SetSmartDev_gpio:ON\n");
			ret = pinctrl_select_state(vowserv.pinctrl,
						   vowserv.pins_eint_on);
			if (ret) {
				/* pinctrl setting error */
				VOWDRV_DEBUG(
				"error, can not set gpio vow pins_eint_on\n");
			}
		}
	} else {
		/* no node here */
		VOWDRV_DEBUG("there is no node\n");
	}
}
#endif

static bool VowDrv_SetFlag(int type, unsigned int set)
{
	int ipi_ret;
	unsigned int vow_ipi_buf[2];

	VOWDRV_DEBUG("%s(), type:%x, set:0x%x\n", __func__, type, set);
	vow_ipi_buf[0] = type;
	vow_ipi_buf[1] = set;

	ipi_ret = vow_ipi_send(IPIMSG_VOW_SET_FLAG,
			       2,
			       &vow_ipi_buf[0],
			       VOW_IPI_BYPASS_ACK);

	if ((ipi_ret == IPI_SCP_DIE) ||
	    (ipi_ret == IPI_SCP_SEND_PASS) ||
	    (ipi_ret == IPI_SCP_RECOVERING))
		return true;
	// IPI_SCP_SEND_FAIL, IPI_SCP_NO_SUPPORT
	return false;
}

static bool VowDrv_SetSpeakerNumber(void)
{
	bool ret = false;

	ret = VowDrv_SetFlag(VOW_FLAG_SPEAKER_NUMBER, vowserv.vow_speaker_number);

	return ret;
}

static bool VowDrv_SetProviderType(unsigned int type)
{
	bool ret = false;
	unsigned int provider_id = type & 0x0F;

	if ((provider_id != VOW_PROVIDER_NONE) && (vowserv.virtual_input == true)) {
		type &= ~0x0F;
		type |= VOW_PROVIDER_VIRTUAL;
		VOWDRV_DEBUG("set provider virtual\n\r");
	}
	vowserv.provider_type = type;
	ret = VowDrv_SetFlag(VOW_FLAG_PROVIDER_TYPE, type);

	return ret;
}

static bool VowDrv_CheckProviderType(unsigned int type, bool enable)
{
	unsigned int provider_id = type & 0x0F;
	unsigned int ch_num = (type >> 4) & 0x0F;
	unsigned int scp_dmic_ch_sel = (type >> 13) & 0x7;
	unsigned int magic = (type >> 16) & 0xFFFF;
	unsigned int ch = 0;

	if (magic != MAGIC_PROVIDER_NUMBER) {
		//check magic number from audio hal is correct
		VOWDRV_DEBUG("wrong provider setting\n\r");
		return false;
	}
	if (enable) {
		if (provider_id == VOW_PROVIDER_NONE) {
			//VOW_PROVIDER_NONE is used to disable vow
			VOWDRV_DEBUG("wrong VOW_PROVIDER_ID %d\n\r", provider_id);
			return false;
		}
		if ((vowserv.provider_type & 0x0F) != VOW_PROVIDER_NONE) {
			//current vow driver in SCP is enabled
			VOWDRV_DEBUG("VOW_PROVIDER_ID %d already construct\n\r", provider_id);
			return false;
		}
	} else {
		if (provider_id != VOW_PROVIDER_NONE) {
			//VOW_PROVIDER_NONE is used to disable vow
			VOWDRV_DEBUG("wrong VOW_PROVIDER_ID %d\n\r", provider_id);
			return false;
		}
		if ((vowserv.provider_type & 0x0F) == VOW_PROVIDER_NONE) {
			//current vow driver in SCP is disabled
			VOWDRV_DEBUG("VOW_PROVIDER_ID %d already destruct\n\r", provider_id);
			return false;
		}
	}
	if (provider_id >= VOW_PROVIDER_MAX) {
		VOWDRV_DEBUG("out of VOW_PROVIDER_ID %d\n\r", provider_id);
		return false;
	}
	if (ch_num >= VOW_CH_MAX) {
		VOWDRV_DEBUG("out of VOW_MIC_NUM %d\n\r", ch_num);
		return false;
	}
	while(scp_dmic_ch_sel){
		ch += scp_dmic_ch_sel & 0x1;
		scp_dmic_ch_sel = scp_dmic_ch_sel>>1;
	}
	if (ch >= VOW_MAX_SCP_DMIC_CH_NUM) {
		VOWDRV_DEBUG("out of VOW_MAX_SCP_DMIC_CH_NUM %d\n\r", ch);
		return false;
	}
	return true;
}

static ssize_t VowDrv_GetPhase1Debug(struct device *kobj,
				     struct device_attribute *attr,
				     char *buf)
{
	unsigned int stat;
	char cstr[35];
	int size = sizeof(cstr);

	stat = (vowserv.force_phase_stage == FORCE_PHASE1) ? 1 : 0;

	return snprintf(buf, size, "Force Phase1 Setting = %s\n",
			(stat == 0x1) ? "YES" : "NO");
}

static ssize_t VowDrv_SetPhase1Debug(struct device *kobj,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	vowserv.force_phase_stage = (enable == 1) ? FORCE_PHASE1 : NO_FORCE;

	VowDrv_SetFlag(VOW_FLAG_FORCE_PHASE1_DEBUG, enable);
	return n;
}
DEVICE_ATTR(vow_SetPhase1,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetPhase1Debug,
	    VowDrv_SetPhase1Debug);

static ssize_t VowDrv_GetPhase2Debug(struct device *kobj,
				     struct device_attribute *attr,
				     char *buf)
{
	unsigned int stat;
	char cstr[35];
	int size = sizeof(cstr);

	stat = (vowserv.force_phase_stage == FORCE_PHASE2) ? 1 : 0;

	return snprintf(buf, size, "Force Phase2 Setting = %s\n",
			(stat == 0x1) ? "YES" : "NO");
}

static ssize_t VowDrv_SetPhase2Debug(struct device *kobj,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	vowserv.force_phase_stage = (enable == 1) ? FORCE_PHASE2 : NO_FORCE;

	VowDrv_SetFlag(VOW_FLAG_FORCE_PHASE2_DEBUG, enable);
	return n;
}
DEVICE_ATTR(vow_SetPhase2,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetPhase2Debug,
	    VowDrv_SetPhase2Debug);

static ssize_t VowDrv_GetDualMicDebug(struct device *kobj,
				     struct device_attribute *attr,
				     char *buf)
{
	char cstr[35];
	int size = sizeof(cstr);

	if (vowserv.scp_dual_mic_switch == VOW_ENABLE_DUAL_MIC)
		return snprintf(buf, size, "use Daul mic\n");
	else if (vowserv.scp_dual_mic_switch == VOW_ENABLE_SINGLE_MAIN_MIC)
		return snprintf(buf, size, "use Single mic: main mic\n");
	else if (vowserv.scp_dual_mic_switch == VOW_ENABLE_SINGLE_REF_MIC)
		return snprintf(buf, size, "use Single mic: ref mic\n");
	else
		return snprintf(buf, size, "set mic error\n");
}

static ssize_t VowDrv_SetDualMicDebug(struct device *kobj,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	vowserv.scp_dual_mic_switch = enable;

	VowDrv_SetFlag(VOW_FLAG_DUAL_MIC_SWITCH, enable);
	return n;
}
DEVICE_ATTR(vow_DualMicSwitch,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetDualMicDebug,
	    VowDrv_SetDualMicDebug);

static ssize_t VowDrv_GetSplitDumpFile(struct device *kobj,
				     struct device_attribute *attr,
				     char *buf)
{
	unsigned int stat;
	char cstr[35];
	int size = sizeof(cstr);

	stat = (vowserv.split_dumpfile_flag == true) ? 1 : 0;

	return snprintf(buf, size, "Split dump file = %s\n",
			(stat == 0x1) ? "YES" : "NO");
}

static ssize_t VowDrv_SetSplitDumpFile(struct device *kobj,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	VOWDRV_DEBUG("%s(), enable = %d\n", __func__, enable);
	vowserv.split_dumpfile_flag = (enable == 1) ? true : false;

	return n;
}
DEVICE_ATTR(vow_SplitDumpFile,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetSplitDumpFile,
	    VowDrv_SetSplitDumpFile);


static ssize_t VowDrv_GetBargeInStatus(struct device *kobj,
				       struct device_attribute *attr,
				       char *buf)
{
	unsigned int stat;
	char cstr[35];
	int size = sizeof(cstr);

	stat = (vowserv.bargein_enable == true) ? 1 : 0;

	return snprintf(buf, size, "BargeIn Status = %s\n",
			(stat == 0x1) ? "YES" : "NO");
}

static ssize_t VowDrv_SetBargeInDebug(struct device *kobj,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	VowDrv_SetBargeIn(enable, 1); /* temp fix irq */
	return n;
}
DEVICE_ATTR(vow_SetBargeIn,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetBargeInStatus,
	    VowDrv_SetBargeInDebug);

static bool VowDrv_SetBargeIn(unsigned int set, unsigned int irq_id)
{
	int ipi_ret;
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	unsigned int vow_ipi_buf[1];

	if (irq_id >= VOW_BARGEIN_IRQ_MAX_NUM) {
		VOWDRV_DEBUG("out of vow bargein irq range %d", irq_id);
		return false;
	}
	vow_ipi_buf[0] = irq_id;

	VOWDRV_DEBUG("%s(), set = %d, irq = %d\n", __func__, set, irq_id);
	if (set == 1) {
		vow_register_feature(VOW_BARGEIN_FEATURE_ID);

		ipi_ret = vow_ipi_send(IPIMSG_VOW_SET_BARGEIN_ON,
				   1,
				   &vow_ipi_buf[0],
				   VOW_IPI_NEED_ACK);
		if ((ipi_ret == IPI_SCP_DIE) ||
		    (ipi_ret == IPI_SCP_SEND_PASS) ||
		    (ipi_ret == IPI_SCP_RECOVERING))
			vowserv.bargein_enable = true;
	} else if (set == 0) {
		ipi_ret = vow_ipi_send(IPIMSG_VOW_SET_BARGEIN_OFF,
				   1,
				   &vow_ipi_buf[0],
				   VOW_IPI_NEED_ACK);
		vow_deregister_feature(VOW_BARGEIN_FEATURE_ID);
		if ((ipi_ret == IPI_SCP_DIE) ||
		    (ipi_ret == IPI_SCP_SEND_PASS) ||
		    (ipi_ret == IPI_SCP_RECOVERING))
			vowserv.bargein_enable = false;
	} else {
		VOWDRV_DEBUG("Adb comment error\n\r");
	}
#else
	VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
	return true;
}

static ssize_t VowDrv_GetBypassPhase3Flag(struct device *kobj,
					  struct device_attribute *attr,
					  char *buf)
{
	unsigned int stat;
	char cstr[35];
	int size = sizeof(cstr);

	stat = (vowserv.bypass_enter_phase3 == true) ? 1 : 0;

	return snprintf(buf, size, "Enter Phase3 Setting is %s\n",
			(stat == 0x1) ? "Bypass" : "Allow");
}

static ssize_t VowDrv_SetBypassPhase3Flag(struct device *kobj,
					  struct device_attribute *attr,
					  const char *buf,
					  size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	if (enable == 0) {
		VOWDRV_DEBUG("Allow enter phase3\n");
		vowserv.bypass_enter_phase3 = false;
	} else {
		VOWDRV_DEBUG("Bypass enter phase3\n");
		vowserv.bypass_enter_phase3 = true;
	}
	return n;
}
DEVICE_ATTR(vow_SetBypassPhase3,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetBypassPhase3Flag,
	    VowDrv_SetBypassPhase3Flag);

static ssize_t VowDrv_GetEnterPhase3Counter(struct device *kobj,
					    struct device_attribute *attr,
					    char *buf)
{
	char cstr[35];
	int size = sizeof(cstr);

	return snprintf(buf, size, "Enter Phase3 Counter is %u\n",
			vowserv.enter_phase3_cnt);
}
DEVICE_ATTR(vow_GetEnterPhase3Counter,
	    0444, /*S_IRUGO*/
	    VowDrv_GetEnterPhase3Counter,
	    NULL);

static ssize_t VowDrv_GetSWIPLog(struct device *kobj,
				 struct device_attribute *attr,
				 char *buf)
{
	unsigned int stat;
	char cstr[20];
	int size = sizeof(cstr);

	stat = (vowserv.swip_log_enable == true) ? 1 : 0;
	return snprintf(buf, size, "SWIP LOG is %s\n",
			(stat == true) ? "YES" : "NO");
}

static ssize_t VowDrv_SetSWIPLog(struct device *kobj,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	VowDrv_SetFlag(VOW_FLAG_SWIP_LOG_PRINT, enable);
	vowserv.swip_log_enable = (enable == 1) ? true : false;
	VOWDRV_DEBUG("%s(),enable=%d\n", __func__, enable);

	return n;
}
DEVICE_ATTR(vow_SetLibLog,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetSWIPLog,
	    VowDrv_SetSWIPLog);

static ssize_t VowDrv_SetEnableHW(struct device *kobj,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	VowDrv_EnableHW(enable);
	VowDrv_ChangeStatus();
	return n;
}
DEVICE_ATTR(vow_SetEnableHW,
	    0200, /*S_IWUSR*/
	    NULL,
	    VowDrv_SetEnableHW);

static ssize_t VowDrv_GetMCPSflag(struct device *kobj,
				  struct device_attribute *attr,
				  char *buf)
{
	unsigned int stat;
	char cstr[35];
	int size = sizeof(cstr);

	stat = (vowserv.mcps_flag == true) ? 1 : 0;

	return snprintf(buf, size, "Enable Measure MCPS = %s\n",
			(stat == 0x1) ? "YES" : "NO");
}

static ssize_t VowDrv_SetMCPSflag(struct device *kobj,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	VowDrv_SetFlag(VOW_FLAG_MCPS, enable);
	vowserv.mcps_flag = (enable == 1) ? true : false;
	VOWDRV_DEBUG("%s(),enable=%d\n", __func__, enable);
	return n;
}
DEVICE_ATTR(vow_SetMCPS,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetMCPSflag,
	    VowDrv_SetMCPSflag);

static ssize_t VowDrv_GetPatternInput(struct device *kobj,
				      struct device_attribute *attr,
				      char *buf)
{
	unsigned int stat;
	char cstr[35];
	int size = sizeof(cstr);

	stat = (vowserv.virtual_input == true) ? 1 : 0;

	return snprintf(buf, size, "Virtual Input = %s\n",
			(stat == 0x1) ? "YES" : "NO");
}

static ssize_t VowDrv_SetPatternInput(struct device *kobj,
				      struct device_attribute *attr,
				      const char *buf,
				      size_t n)
{
	unsigned int enable = 0;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	if (enable == 1)
		vowserv.virtual_input = true;
	else if (enable == 0)
		vowserv.virtual_input = false;
	else {
		VOWDRV_DEBUG("%s(), enable need be 0 or 1\n", __func__);
		return n;
	}
	VOWDRV_DEBUG("%s(),enable=%d\n", __func__, enable);
	return n;
}
DEVICE_ATTR(vow_SetPatternInput,
	    0644, /*S_IWUSR | S_IRUGO*/
	    VowDrv_GetPatternInput,
	    VowDrv_SetPatternInput);

static bool vow_service_NotifyCHREStatus(unsigned int status)
{
	bool ret = true;
	unsigned int isCHREOpen = status & 0xF;

	VOWDRV_DEBUG("%s(),isCHREOpen=%d\n", __func__, isCHREOpen);
	if (status == 0) {
		//do nothing discard msg
		return ret;
	}
	if (isCHREOpen > CHRE_OPEN) {
		//variable overflow
		ret = false;
		return ret;
	}
	VowDrv_SetFlag(VOW_FLAG_CHRE_STATUS, status);
	return ret;
}

static int VowDrv_SetVowEINTStatus(int status)
{
	int ret = 0;

	if ((status < NUM_OF_VOW_EINT_STATUS)
	 && (status >= VOW_EINT_DISABLE)) {
		spin_lock(&vowdrv_lock);
		vowserv.eint_status = status;
		spin_unlock(&vowdrv_lock);
	} else {
		VOWDRV_DEBUG("%s() error input: %x\n",
			     __func__, status);
		ret = -1;
	}
	return ret;
}

static int VowDrv_QueryVowEINTStatus(void)
{
	int ret = 0;

	spin_lock(&vowdrv_lock);
	ret = vowserv.eint_status;
	spin_unlock(&vowdrv_lock);
	return ret;
}

static int VowDrv_open(struct inode *inode, struct file *fp)
{
	VOWDRV_DEBUG("%s() do nothing\n", __func__);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_DEBUG_SUPPORT)
	// no print in user load
	VOWDRV_DEBUG("%s() do nothing inode:%p, file:%p\n",
		    __func__, inode, fp);
#endif
	return 0;
}

static int VowDrv_release(struct inode *inode, struct file *fp)
{
	VOWDRV_DEBUG("%s(), enter\n", __func__);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_DEBUG_SUPPORT)
	VOWDRV_DEBUG("%s() inode:%p, file:%p\n", __func__, inode, fp);
#endif

	if (!(fp->f_mode & FMODE_WRITE || fp->f_mode & FMODE_READ))
		return -ENODEV;
	return 0;
}

static long VowDrv_ioctl(struct file *fp, unsigned int cmd, unsigned long arg_data)
{
	int ret = 0;
	int timeout = 0;
	char ioctl_type[50] = {0};
	unsigned long arg = 0;
	struct vow_ioctl_arg_info_t arg_info;
	unsigned long rec_queue_flags;

	timeout = 0;
	while (vowserv.vow_recovering) {
		msleep(VOW_WAITCHECK_INTERVAL_MS);
		timeout++;
		VOW_ASSERT(timeout != VOW_RECOVERY_WAIT);
	}

	if (copy_from_user((void *)(&arg_info), (const void __user *)(arg_data),
			   sizeof(struct vow_ioctl_arg_info_t))) {
		VOWDRV_DEBUG("vow check ioctl fail\n");
		return -EFAULT;
	}
	if (arg_info.magic_number != MAGIC_IOCTL_NUMBER) {
		VOWDRV_DEBUG("vow check ioctl magic fail\n");
		return -EFAULT;
	}
	arg = arg_info.return_data;
	//VOWDRV_DEBUG("%s(), cmd = %u, arg = %lu\n", __func__, cmd, arg);
	switch ((unsigned int)cmd) {
	case VOW_SET_CONTROL:
		switch (arg) {
		case VOWControlCmd_Init:
			VOWDRV_DEBUG("VOW_SET_CONTROL Init");
			vow_service_Init();
			break;
		case VOWControlCmd_Reset:
			VOWDRV_DEBUG("VOW_SET_CONTROL Reset");
			vow_service_reset();
			break;
		case VOWControlCmd_EnableHotword:
			VOWDRV_DEBUG("VOW_SET_CONTROL EnableDebug");
			mutex_lock(&vow_vmalloc_lock);
			vowserv.kernel_voicedata_idx = 0;
			mutex_unlock(&vow_vmalloc_lock);
			vowserv.recording_flag = true;
			vowserv.firstRead = true;
			/*VowDrv_SetFlag(VOW_FLAG_DEBUG, true);*/
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
			vow_register_feature(VOW_DUMP_FEATURE_ID);
#else
			VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
			if (vowserv.suspend_lock == 0) {
				vowserv.suspend_lock = 1;
				/* Let AP will not suspend */
				__pm_stay_awake(vow_suspend_lock);
				VOWDRV_DEBUG("==VOW DEBUG MODE START==\n");
			}
			break;
		case VOWControlCmd_DisableHotword:
			VOWDRV_DEBUG("VOW_SET_CONTROL DisableDebug");
			VowDrv_SetFlag(VOW_FLAG_DEBUG, false);
			vowserv.recording_flag = false;
			/* force stop vow_service_ReadVoiceData() 20180906 */
			vow_service_getVoiceData();
			spin_lock_irqsave(&vow_rec_queue_lock, rec_queue_flags);
			vow_init_rec_ipi_queue();
			spin_unlock_irqrestore(&vow_rec_queue_lock, rec_queue_flags);
			if (vowserv.suspend_lock == 1) {
				vowserv.suspend_lock = 0;
				/* Let AP will suspend */
				__pm_relax(vow_suspend_lock);
				/* lock 1sec for screen on */
				VOWDRV_DEBUG("==VOW DEBUG MODE STOP==\n");
				__pm_wakeup_event(vow_suspend_lock,
						  jiffies_to_msecs(HZ));
			}
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
			vow_deregister_feature(VOW_DUMP_FEATURE_ID);
#else
			VOWDRV_DEBUG("%s(), vow: SCP no support\n\r", __func__);
#endif
			break;
		case VOWControlCmd_EnableSeamlessRecord:
			VOWDRV_DEBUG("VOW_SET_CONTROL EnableSeamlessRecord");
			VowDrv_SetFlag(VOW_FLAG_SEAMLESS, true);
			break;
		case VOWControlCmd_EnableDump:
			vow_pcm_dump_set(true);
			break;
		case VOWControlCmd_DisableDump:
			vow_pcm_dump_set(false);
			break;
		case VOWControlCmd_Mic_Single:
			vowserv.vow_mic_number = 1;
			VOWDRV_DEBUG("VOW_SET_CONTROL Set Single Mic VOW");
			break;
		case VOWControlCmd_Mic_Dual:
			vowserv.vow_mic_number = 2;
			VOWDRV_DEBUG("VOW_SET_CONTROL Set Dual Mic VOW");
			break;
		case VOWControlCmd_Speaker_Single:
			vowserv.vow_speaker_number = 1;
			VOWDRV_DEBUG("VOW_SET_CONTROL Set Single Speaker VOW");
			break;
		case VOWControlCmd_Speaker_Dual:
			vowserv.vow_speaker_number = 2;
			VOWDRV_DEBUG("VOW_SET_CONTROL Set Dual Speaker VOW");
			break;
		case VOWControlCmd_GetDump:
			vow_service_ReadDumpData();
			break;
		default:
			VOWDRV_DEBUG("VOW_SET_CONTROL no such command = %lu",
				     arg);
			break;
		}
		break;
	case VOW_GET_SCP_RECOVER_STATUS:
		if (!vow_service_get_scp_recover(arg)) {
			strcpy(ioctl_type, "VOW_GET_SCP_RECOVER_STATUS");
			ret = -EFAULT;
		}
		break;
	case VOW_READ_VOICE_DATA:
		if (!vow_service_SetApAddr(arg)) {
			strcpy(ioctl_type, "VOW_READ_VOICE_DATA");
			ret = -EFAULT;
			break;
		}
		if ((vowserv.recording_flag == true)
		    && (vowserv.firstRead == true)) {
			vowserv.firstRead = false;
			VowDrv_SetFlag(VOW_FLAG_DEBUG, true);
		}
		vow_service_ReadVoiceData();
		break;
	case VOW_SET_SPEAKER_MODEL:
		VOWDRV_DEBUG("VOW_SET_SPEAKER_MODEL(%lu)", arg);
		if (!vow_service_SetSpeakerModel(arg)) {
			strcpy(ioctl_type, "VOW_SET_SPEAKER_MODEL");
			ret = -EFAULT;
		}
		break;
	case VOW_CLR_SPEAKER_MODEL:
		VOWDRV_DEBUG("VOW_CLR_SPEAKER_MODEL(%lu)", arg);
		if (!vow_service_ReleaseSpeakerModel((int)arg)) {
			strcpy(ioctl_type, "VOW_CLR_SPEAKER_MODEL");
			ret = -EFAULT;
		}
		break;
	case VOW_SET_DSP_AEC_PARAMETER:
		VOWDRV_DEBUG("VOW_SET_DSP_AEC_PARAMETER(%lu)", arg);
		if (!vow_service_SetCustomModel(arg)) {
			strcpy(ioctl_type, "VOW_SET_DSP_AEC_PARAMETER");
			ret = -EFAULT;
		}
		break;
	case VOW_SET_APREG_INFO:
		VOWDRV_DEBUG("VOW_SET_APREG_INFO(%lu)", arg);
		if (!vow_service_SetVBufAddr(arg)) {
			strcpy(ioctl_type, "VOW_SET_APREG_INFO");
			ret = -EFAULT;
		}
		break;
	case VOW_SET_VOW_DUMP_DATA:
		//VOWDRV_DEBUG("VOW_SET_VOW_DUMP_DATA(%lu)", arg);
		if (!vow_service_SetApDumpAddr(arg)) {
			strcpy(ioctl_type, "VOW_SET_VOW_DUMP_DATA");
			ret = -EFAULT;
		}
		break;
	case VOW_BARGEIN_ON:
		VOWDRV_DEBUG("VOW_BARGEIN_ON, irq: %d", (unsigned int)arg);
		if (!VowDrv_SetBargeIn(1, (unsigned int)arg)) {
			strcpy(ioctl_type, "VOW_BARGEIN_ON");
			ret = -EFAULT;
		}
		break;
	case VOW_BARGEIN_OFF:
		VOWDRV_DEBUG("VOW_BARGEIN_OFF, irq: %d", (unsigned int)arg);
		if (!VowDrv_SetBargeIn(0, (unsigned int)arg)) {
			strcpy(ioctl_type, "VOW_BARGEIN_OFF");
			ret = -EFAULT;
		}
		break;
	case VOW_CHECK_STATUS:
		if (arg != 0) {
			VOWDRV_DEBUG("VOW_CHECK_STATUS(%lu) para err, break", arg);
			break;
		}
		/* VOW disable already, then bypass second one */
		VowDrv_ChangeStatus();
		VOWDRV_DEBUG("VOW_CHECK_STATUS(%lu)", arg);
		break;
	case VOW_RECOG_ENABLE:
		pr_debug("+VOW_RECOG_ENABLE(0x%lx)+", arg);
		pr_debug("KERNEL_VOW_DRV_VER %s", KERNEL_VOW_DRV_VER);
		if (!VowDrv_CheckProviderType((unsigned int)arg, true)) {
			pr_debug("VOW_RECOG_ENABLE fail");
			break;
		}
		VowDrv_SetProviderType((unsigned int)arg);
		VowDrv_SetSpeakerNumber();
		VowDrv_EnableHW(1);
		VowDrv_ChangeStatus();
		vow_service_Enable();
		pr_debug("-VOW_RECOG_ENABLE(0x%lx)-", arg);
		break;
	case VOW_RECOG_DISABLE:
		pr_debug("+VOW_RECOG_DISABLE(0x%lx)+", arg);
		if (!VowDrv_CheckProviderType((unsigned int)arg, false)) {
			pr_debug("VOW_RECOG_DISABLE fail");
			break;
		}
		VowDrv_EnableHW(0);
		VowDrv_ChangeStatus();
		vow_service_Disable();
		VowDrv_SetProviderType((unsigned int)arg);
		vow_service_getScpRecover();
		pr_debug("-VOW_RECOG_DISABLE(0x%lx)-", arg);
		break;
	case VOW_MODEL_START:
		vow_service_SetModelStatus(VOW_MODEL_STATUS_START, arg);
		break;
	case VOW_MODEL_STOP:
		vow_service_SetModelStatus(VOW_MODEL_STATUS_STOP, arg);
		break;
	case VOW_GET_GOOGLE_ENGINE_VER: {
		if (vowserv.google_engine_version == DEFAULT_GOOGLE_ENGINE_VER ||
			vowserv.google_engine_version == 0) {
			VOWDRV_DEBUG("%s(), VOW_GET_GOOGLE_ENGINE_VER %d fail",
				__func__, vowserv.google_engine_version);
		} else if (copy_to_user((void __user *)arg,
						 &vowserv.google_engine_version,
						 sizeof(vowserv.google_engine_version))) {
			VOWDRV_DEBUG("%s(), copy_to_user fail in VOW_GET_GOOGLE_ENGINE_VER",
						__func__);
		}
	}
		break;
	case VOW_GET_GOOGLE_ARCH:
	case VOW_GET_ALEXA_ENGINE_VER: {
		struct vow_engine_info_t engine_ver_temp;
		unsigned int length = VOW_ENGINE_INFO_LENGTH_BYTE;

		if (copy_from_user((void *)&engine_ver_temp,
				 (const void __user *)arg,
				 sizeof(struct vow_engine_info_t))) {
			VOWDRV_DEBUG("%s(), copy_from_user fail", __func__);
		}
		if (engine_ver_temp.data_addr == 0 ||
			engine_ver_temp.return_size_addr == 0) {
			VOWDRV_DEBUG("%s(), copy_from_user fail 2", __func__);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_DEBUG_SUPPORT)
			VOWDRV_DEBUG("%s(), data_addr %lu return_size_addr %lu",
			__func__,
			engine_ver_temp.data_addr,
			engine_ver_temp.return_size_addr);
#endif
		} else if ((unsigned int)cmd == VOW_GET_ALEXA_ENGINE_VER) {
			pr_debug("VOW_GET_ALEXA_ENGINE_VER = %s, %lu, %lu, %d",
					 vowserv.alexa_engine_version,
					 engine_ver_temp.data_addr,
					 engine_ver_temp.return_size_addr,
					 length);
			if (copy_to_user((void __user *)engine_ver_temp.data_addr,
					 vowserv.alexa_engine_version,
					 length)) {
				VOWDRV_DEBUG("%s(), copy_to_user fail", __func__);
			}
		} else if ((unsigned int)cmd == VOW_GET_GOOGLE_ARCH) {
			pr_debug("VOW_GET_GOOGLE_ARCH = %s, %lu, %lu, %d",
					 vowserv.google_engine_arch,
					 engine_ver_temp.data_addr,
					 engine_ver_temp.return_size_addr,
					 length);
			if (copy_to_user((void __user *)engine_ver_temp.data_addr,
					 vowserv.google_engine_arch,
					 length)) {
				VOWDRV_DEBUG("%s(), copy_to_user fail", __func__);
			}
		}
		if (copy_to_user((void __user *)engine_ver_temp.return_size_addr,
				 &length,
				 sizeof(unsigned int))) {
			VOWDRV_DEBUG("%s(), copy_to_user fail", __func__);
		}
	}
		break;
	/* for payload dump feature(data from scp) */
	case VOW_SET_PAYLOADDUMP_INFO: {
		struct vow_payloaddump_info_t payload = {};
		unsigned int buffer_bound = 0;

		if (copy_from_user((void *)&payload,
				 (const void __user *)arg,
				 sizeof(struct vow_payloaddump_info_t))) {
			VOWDRV_DEBUG("vow copy payloaddump_info fail\n");
			return false;
		}
		/* add return condition */
		if (dual_ch_transfer == true) {
			buffer_bound = VOW_MAX_MIC_NUM * VOW_VOICEDATA_SIZE;
		} else {
			buffer_bound = VOW_VOICEDATA_SIZE;
		}
		if ((payload.return_payloaddump_addr == 0) ||
		    (payload.max_payloaddump_size != buffer_bound)) {
			VOWDRV_DEBUG("vow check payload fail: addr_0x%x, size_0x%x\n",
			     (unsigned int)payload.return_payloaddump_addr,
			     (unsigned int)payload.max_payloaddump_size);
			return false;
		}
		if (payload.return_payloaddump_size_addr != 0) {
			VOWDRV_DEBUG("vow size_addr_%x\n",
			     (unsigned int)payload.return_payloaddump_size_addr);
		}
		vowserv.payloaddump_user_addr =
		    payload.return_payloaddump_addr;
		vowserv.payloaddump_user_max_size =
		    payload.max_payloaddump_size;
		vowserv.payloaddump_user_return_size_addr =
		    payload.return_payloaddump_size_addr;
		pr_debug("-VOW_SET_PAYLOADDUMP_INFO(addr=%lu, sz=%lu)",
			 vowserv.payloaddump_user_addr,
			 vowserv.payloaddump_user_max_size);
		mutex_lock(&vow_payloaddump_mutex);
		if (vowserv.payloaddump_kernel_ptr != NULL) {
			vfree(vowserv.payloaddump_kernel_ptr);
			vowserv.payloaddump_kernel_ptr = NULL;
		}
		if (vowserv.payloaddump_user_max_size > 0) {
			vowserv.payloaddump_kernel_ptr =
			    vmalloc(vowserv.payloaddump_user_max_size);
		} else {
			strcpy(ioctl_type, "VOW_SET_PAYLOADDUMP_INFO");
			ret = -EFAULT;
		}
		mutex_unlock(&vow_payloaddump_mutex);
		vowserv.payloaddump_enable = true;
	}
		break;
	case VOW_SET_VOW_DUAL_CH_TRANSFER:
		VOWDRV_DEBUG("VOW_SET_VOW_DUAL_CH_TRANSFER(%lu)", arg);
		vow_service_SetDualChannelTransfer(arg);
		break;
	case VOW_NOTIFY_CHRE_STATUS:
		VOWDRV_DEBUG("VOW_NOTIFY_CHRE_STATUS(%lu)", arg);
		vowserv.chre_status = (unsigned int)arg;
		if (vow_service_NotifyCHREStatus((unsigned int)arg) == false)
			ret = -EFAULT;
		break;
	case VOW_SET_VOW_DELAY_WAKEUP:
		VOWDRV_DEBUG("VOW_SET_VOW_DELAY_WAKEUP(%lu)", arg);
		vowserv.delay_wakeup_time = (unsigned int)arg;
		if (vow_service_SetDelayWakeupTime((unsigned int)arg) == false)
			ret = -EFAULT;
		break;
	case VOW_SET_VOW_PAYLOAD_CALLBACK:
		VOWDRV_DEBUG("VOW_SET_VOW_PAYLOAD_CALLBACK(%lu)", arg);
		vowserv.payloaddump_cb_type = (unsigned int)arg;
		if (vow_service_SetPayloaddump_callback((unsigned int)arg) == false)
			ret = -EFAULT;
		break;
	default:
		VOWDRV_DEBUG("vow WrongParameter(%lu)", arg);
		break;
	}
	if (ret != 0)
		VOWDRV_DEBUG("vow handle ioctl: %s with arg: %lu error", ioctl_type, arg);
	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long VowDrv_compat_ioctl(struct file *fp,
				unsigned int cmd,
				unsigned long arg)
{
	long ret = 0, arg_err = 0;
	struct vow_ioctl_arg_info_kernel_t arg_info32 = {};
	struct vow_ioctl_arg_info_t arg_info = {};
	/* VOWDRV_DEBUG("++VowDrv_compat_ioctl cmd = %u, arg = %lu\n" */
	/*		, cmd, arg); */
	if (!fp->f_op || !fp->f_op->unlocked_ioctl) {
		(void)ret;
		return -ENOTTY;
	}

	arg_err = (long)copy_from_user(&arg_info32, compat_ptr(arg),
		(unsigned long)sizeof(struct vow_ioctl_arg_info_kernel_t));
	if (arg_err != 0L)
		VOWDRV_DEBUG("%s(), copy data from user fail", __func__);
	arg_info.magic_number    = arg_info32.magic_number;
	arg_info.return_data     = arg_info32.return_data;

	switch (cmd) {
	case VOW_CLR_SPEAKER_MODEL:
	case VOW_SET_CONTROL:
	case VOW_CHECK_STATUS:
	case VOW_RECOG_ENABLE:
	case VOW_RECOG_DISABLE:
	case VOW_BARGEIN_ON:
	case VOW_BARGEIN_OFF:
	case VOW_GET_GOOGLE_ENGINE_VER:
	case VOW_SET_VOW_DUAL_CH_TRANSFER:
	case VOW_NOTIFY_CHRE_STATUS:
	case VOW_SET_VOW_DELAY_WAKEUP:
	case VOW_SET_VOW_PAYLOAD_CALLBACK:
		ret = fp->f_op->unlocked_ioctl(fp, cmd, (unsigned long)&arg_info);
		break;
	case VOW_MODEL_START:
	case VOW_MODEL_STOP: {
		struct vow_model_start_kernel_t data32 = {};
		struct vow_model_start_t data = {};
		long err = (long)copy_from_user(&data32, compat_ptr(arg_info.return_data),
			(unsigned long)sizeof(struct vow_model_start_kernel_t));

		if (err != 0L)
			VOWDRV_DEBUG("%s(), copy data from user fail", __func__);

		data.handle               = data32.handle;
		data.confidence_level     = data32.confidence_level;
		data.dsp_inform_addr      = data32.dsp_inform_addr;
		data.dsp_inform_size_addr = data32.dsp_inform_size_addr;
		arg_info.return_data = (unsigned long)&data;

		ret = fp->f_op->unlocked_ioctl(fp, cmd, (unsigned long)&arg_info);
	}
		break;
	case VOW_READ_VOICE_DATA:
	case VOW_SET_SPEAKER_MODEL:
	case VOW_SET_VOW_DUMP_DATA:
	case VOW_SET_APREG_INFO: {
		struct vow_model_info_kernel_t data32 = {};
		struct vow_model_info_t data = {};
		long err = (long)copy_from_user(&data32, compat_ptr(arg_info.return_data),
			(unsigned long)sizeof(struct vow_model_info_kernel_t));

		if (err != 0L)
			VOWDRV_DEBUG("%s(), copy data from user fail", __func__);

		data.id               = data32.id;
		data.keyword          = data32.keyword;
		data.addr             = data32.addr;
		data.size             = data32.size;
		data.return_size_addr = data32.return_size_addr;
		data.uuid             = data32.uuid;
		data.data             = (uint32_t *)data32.data;
		arg_info.return_data = (unsigned long)&data;

		ret = fp->f_op->unlocked_ioctl(fp, cmd, (unsigned long)&arg_info);
	}
		break;
	case VOW_SET_DSP_AEC_PARAMETER:
	case VOW_GET_GOOGLE_ARCH:
	case VOW_GET_ALEXA_ENGINE_VER: {
		struct vow_engine_info_kernel_t data32 = {};
		struct vow_engine_info_t data = {};
		long err = (long)copy_from_user(&data32, compat_ptr(arg_info.return_data),
			(unsigned long)sizeof(struct vow_engine_info_kernel_t));

		if (err != 0L)
			VOWDRV_DEBUG("%s(), copy data from user fail", __func__);

		data.return_size_addr = data32.return_size_addr;
		data.data_addr        = data32.data_addr;
		arg_info.return_data = (unsigned long)&data;

		ret = fp->f_op->unlocked_ioctl(fp, cmd, (unsigned long)&arg_info);
	}
		break;
	case VOW_SET_PAYLOADDUMP_INFO: {
		struct vow_payloaddump_info_kernel_t data32 = {};
		struct vow_payloaddump_info_t data = {};
		long err = (long)copy_from_user(&data32, compat_ptr(arg_info.return_data),
			(unsigned long)sizeof(struct vow_payloaddump_info_kernel_t));

		if (err != 0L)
			VOWDRV_DEBUG("%s(), copy data from user fail", __func__);

		data.return_payloaddump_addr      = data32.return_payloaddump_addr;
		data.return_payloaddump_size_addr = data32.return_payloaddump_size_addr;
		data.max_payloaddump_size         = data32.max_payloaddump_size;
		arg_info.return_data = (unsigned long)&data;

		ret = fp->f_op->unlocked_ioctl(fp, cmd, (unsigned long)&arg_info);
	}
		break;
	case VOW_GET_SCP_RECOVER_STATUS: {
		struct vow_scp_recover_info_kernel_t data32 = {};
		struct vow_scp_recover_info_t data = {};
		long err = (long)copy_from_user(&data32, compat_ptr(arg_info.return_data),
			(unsigned long)sizeof(struct vow_scp_recover_info_kernel_t));
		if (err != 0L)
			VOWDRV_DEBUG("%s(), copy data from user fail", __func__);

		data.return_event_addr = data32.return_event_addr;
		arg_info.return_data = (unsigned long)&data;

		ret = fp->f_op->unlocked_ioctl(fp, cmd, (unsigned long)&arg_info);
	}
		break;
	default:
		break;
	}
	/* VOWDRV_DEBUG("--VowDrv_compat_ioctl\n"); */
	return ret;
}
#endif

static ssize_t VowDrv_write(struct file *fp,
			    const char __user *data,
			    size_t count,
			    loff_t *offset)
{
	/* VOWDRV_DEBUG("+VowDrv_write = %p count = %d\n",fp ,count); */
	return 0;
}

static ssize_t VowDrv_read(struct file *fp,
			   char __user *data,
			   size_t count,
			   loff_t *offset)
{
	unsigned int read_count = 0;
	unsigned int time_diff_scp_ipi = 0;
	unsigned int time_diff_ipi_read = 0;
	unsigned long long vow_read_cycle = 0;
	int ret = 0;
	unsigned int ret_data = 0;
	int slot = 0;
	int eint_status = 0;
	bool dsp_inform_tx_flag = false;

	VOWDRV_DEBUG("+%s()+\n", __func__);
	if (fp == NULL || data == NULL || offset == NULL) {
		VOWDRV_DEBUG("%s(), open vow fail\n", __func__);
		goto exit;
	}
	if (count != sizeof(struct vow_eint_data_struct_t)) {
		VOWDRV_DEBUG(
			"%s(), cpy incorrect size to user, size=%ld, correct size=%ld, exit\n",
			__func__,
			count,
			sizeof(struct vow_eint_data_struct_t));
		goto exit;
	}
	VowDrv_SetVowEINTStatus(VOW_EINT_RETRY);

	if (VowDrv_Wait_Queue_flag == 0) {
		ret = wait_event_interruptible(VowDrv_Wait_Queue,
					       VowDrv_Wait_Queue_flag);
		if (ret < 0)
			VOWDRV_DEBUG("wait %s fail, ret=%d\n", __func__, ret);
	}
	if (VowDrv_Wait_Queue_flag == 1) {
		VowDrv_Wait_Queue_flag = 0;
		if (VowDrv_GetHWStatus() == VOW_PWR_OFF) {
			VOWDRV_DEBUG("vow Enter_phase3_cnt = %d\n",
				      vowserv.enter_phase3_cnt);
			vowserv.scp_command_flag = false;
			VowDrv_SetVowEINTStatus(VOW_EINT_DISABLE);
		} else {
			if (vowserv.scp_command_flag) {
				VowDrv_SetVowEINTStatus(VOW_EINT_PASS);
				vow_read_cycle = get_cycles();
				time_diff_scp_ipi =
				    (unsigned int)CYCLE_TO_NS *
				    (unsigned int)(vowserv.ap_received_ipi_cycle
				    - vowserv.scp_recognize_ok_cycle);
				time_diff_ipi_read =
				    (unsigned int)CYCLE_TO_NS *
				    (unsigned int)(vow_read_cycle
				    - vowserv.ap_received_ipi_cycle);
				VOWDRV_DEBUG("vow Wakeup by SCP\n");
				VOWDRV_DEBUG("SCP->IPI:%d(ns),IPI->AP:%d(ns)\n",
					     time_diff_scp_ipi,
					     time_diff_ipi_read);
				if (vowserv.suspend_lock == 0) {
					/* lock 1sec for screen on */
					__pm_wakeup_event(vow_suspend_lock,
							  jiffies_to_msecs(HZ));
				}
				vowserv.scp_command_flag = false;
				if (vowserv.extradata_bytelen > 0)
					dsp_inform_tx_flag = true;
			} else {
				VOWDRV_DEBUG("vow Wakeup by other(%d,%d)\n",
					     VowDrv_Wait_Queue_flag,
					     VowDrv_GetHWStatus());
			}
		}
	}
	slot = vow_service_SearchSpeakerModelWithKeyword(
		  vowserv.scp_command_keywordid);
	eint_status = VowDrv_QueryVowEINTStatus();
	if (slot < 0) {
		/* there is no pair id */
		VOWDRV_DEBUG("%s(), search ID fail, not keyword event, eint=%d, exit\n",
						__func__, eint_status);
		vowserv.scp_command_id =  0;
		vowserv.confidence_level = 0;
		goto exit;
	} else {
		vowserv.scp_command_id = vowserv.vow_speaker_model[slot].id;
	}

	memset((void *)&vowserv.vow_eint_data_struct, 0,
					sizeof(vowserv.vow_eint_data_struct));
	vowserv.vow_eint_data_struct.id = vowserv.scp_command_id;
	vowserv.vow_eint_data_struct.eint_status = eint_status;
	vowserv.vow_eint_data_struct.data[0] = (char)vowserv.confidence_level;
	vowserv.confidence_level = 0;
	read_count = copy_to_user((void __user *)data,
				  &vowserv.vow_eint_data_struct,
				  sizeof(struct vow_eint_data_struct_t));
	if (dsp_inform_tx_flag) {
		/* int i; */
		mutex_lock(&vow_extradata_mutex);
		if (vowserv.extradata_mem_ptr == NULL) {
			mutex_unlock(&vow_extradata_mutex);
			goto exit;
		}
		if (vowserv.extradata_ptr == NULL) {
			mutex_unlock(&vow_extradata_mutex);
			goto exit;
		}
		mutex_unlock(&vow_extradata_mutex);
		if (vowserv.vow_speaker_model[slot].rx_inform_size_addr == 0)
			goto exit;
		if (vowserv.vow_speaker_model[slot].rx_inform_addr == 0)
			goto exit;
		VOWDRV_DEBUG("%s(), copy to user, extra data len=%d\n",
		     __func__, vowserv.extradata_bytelen);

		/* for payload dump feature(data from scp) */
		vow_service_ReadPayloadDumpData(vowserv.payloaddump_length);

		/* copy extra data from DRAM */
		mutex_lock(&vow_extradata_mutex);
		memcpy(vowserv.extradata_mem_ptr,
		       vowserv.extradata_ptr,
		       vowserv.extradata_bytelen);
		mutex_unlock(&vow_extradata_mutex);
		/* copy extra data into user space */
		ret_data = copy_to_user(
		    (void __user *)
		    vowserv.vow_speaker_model[slot].rx_inform_size_addr,
		    &vowserv.extradata_bytelen,
		    sizeof(unsigned int));
		if (ret_data != 0) {
			/* fail, print the fail size */
			VOWDRV_DEBUG("[vow dsp inform1]CopytoUser fail sz:%d\n",
				     ret_data);
		}
		mutex_lock(&vow_extradata_mutex);
		ret_data = copy_to_user(
		    (void __user *)
		    vowserv.vow_speaker_model[slot].rx_inform_addr,
		    vowserv.extradata_mem_ptr,
		    vowserv.extradata_bytelen);
		mutex_unlock(&vow_extradata_mutex);
		if (ret_data != 0) {
			/* fail, print the fail size */
			VOWDRV_DEBUG("[vow dsp inform2]CopytoUser fail sz:%d\n",
				     ret_data);
		}
	}

exit:
	VOWDRV_DEBUG("+%s()-, recog id: %d, confidence_lv=%d, eint=%d\n",
		     __func__,
		     vowserv.scp_command_id,
		     vowserv.confidence_level,
		     eint_status);
	return read_count;
}

static int VowDrv_flush(struct file *flip, fl_owner_t id)
{
	VOWDRV_DEBUG("%s(), enter\n", __func__);

	return 0;
}

static int VowDrv_fasync(int fd, struct file *flip, int mode)
{
	VOWDRV_DEBUG("%s()\n", __func__);
	/*return fasync_helper(fd, flip, mode, &VowDrv_fasync);*/
	return 0;
}

static int VowDrv_remap_mmap(struct file *flip, struct vm_area_struct *vma)
{
	VOWDRV_DEBUG("%s()\n", __func__);
	return -1;
}

#ifdef VOW_SMART_DEVICE_SUPPORT
int VowDrv_setup_smartdev_eint(struct platform_device *pdev)
{
	int ret;
	unsigned int ints[2] = {0, 0};

	/* gpio setting */
	vowserv.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(vowserv.pinctrl)) {
		ret = PTR_ERR(vowserv.pinctrl);
		VOWDRV_DEBUG("Cannot find Vow pinctrl!\n");
		return ret;
	}
	vowserv.pins_eint_on = pinctrl_lookup_state(vowserv.pinctrl,
						    "vow_smartdev_eint_on");
	if (IS_ERR(vowserv.pins_eint_on)) {
		ret = PTR_ERR(vowserv.pins_eint_on);
		VOWDRV_DEBUG("Cannot find vow pinctrl eint_on!\n");
		return ret;
	}

	vowserv.pins_eint_off = pinctrl_lookup_state(vowserv.pinctrl,
						     "vow_smartdev_eint_off");
	if (IS_ERR(vowserv.pins_eint_off)) {
		ret = PTR_ERR(vowserv.pins_eint_off);
		VOWDRV_DEBUG("Cannot find vow pinctrl eint_off!\n");
		return ret;
	}
	/* eint setting */
	vowserv.node = pdev->dev.of_node;
	if (vowserv.node) {
		ret = of_property_read_u32_array(vowserv.node,
					   "debounce",
					   ints,
					   ARRAY_SIZE(ints));
		if (ret != 0) {
			VOWDRV_DEBUG("%s(), no debounce node, ret=%d\n",
				      __func__, ret);
			return ret;
		}

		VOWDRV_DEBUG("VOW EINT ID: %x, %x\n", ints[0], ints[1]);
	} else {
		/* no node here */
		VOWDRV_DEBUG("%s(), there is no this node\n", __func__);
	}
	return 0;
}
#endif

bool vow_service_GetScpRecoverStatus(void)
{
	return vowserv.scp_recovering;
}

bool vow_service_GetVowRecoverStatus(void)
{
	return vowserv.vow_recovering;
}

/*****************************************************************************
 * SCP Recovery Register
 *****************************************************************************/
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
static int vow_scp_recover_event(struct notifier_block *this,
				 unsigned long event,
				 void *ptr)
{
	switch (event) {
	case SCP_EVENT_READY: {
		int I;
		int ipi_ret;
		unsigned int vow_ipi_buf[2];

		vowserv.vow_recovering = true;
		vowserv.scp_recovering = false;
		VOWDRV_DEBUG("%s(), SCP_EVENT_READY\n", __func__);

		vowserv.scp_recover_data = VOW_SCP_EVENT_READY;
		vow_service_getScpRecover();

		if (!vow_check_scp_status()) {
			VOWDRV_DEBUG("SCP is Off, don't recover VOW\n");
			return NOTIFY_DONE;
		}
		if (vowserv.scp_recovering) {
			vowserv.vow_recovering = false;
			VOWDRV_DEBUG("fail: vow recover1\n");
			break;
		}
		vow_service_Init();
		if (vowserv.scp_recovering) {
			vowserv.vow_recovering = false;
			VOWDRV_DEBUG("fail: vow recover2\n");
			break;
		}
		for (I = 0; I < MAX_VOW_SPEAKER_MODEL; I++) {
			if (!vow_service_SendSpeakerModel(I, VOW_SET_MODEL))
				VOWDRV_DEBUG("fail: SendSpeakerModel\n");
		}
		/* send AEC custom model */
		vow_ipi_buf[0] = vowserv.custom_model_size;
		ipi_ret = vow_ipi_send(IPIMSG_VOW_SET_CUSTOM_MODEL,
				       1, &vow_ipi_buf[0], VOW_IPI_BYPASS_ACK);
		if (ipi_ret != IPI_SCP_SEND_PASS)
			VOWDRV_DEBUG("%s(), IPIMSG_SET_CUSTOM_MODEL send error %d\n", __func__, ipi_ret);

		/* send VOW model status */
		for (I = 0; I < MAX_VOW_SPEAKER_MODEL; I++) {
			if (vowserv.vow_speaker_model[I].enabled) {
				vow_service_SendModelStatus(
					I, VOW_MODEL_STATUS_START);
				VOWDRV_DEBUG("send Model start, slot%d\n", I);
			}
		}

		/* vow setting */
		if (vow_service_SetDelayWakeupTime(vowserv.delay_wakeup_time) == false)
			VOWDRV_DEBUG("fail: vow_SetDelayWakeupTime\n");
		if (vow_service_SetPayloaddump_callback(vowserv.payloaddump_cb_type) == false)
			VOWDRV_DEBUG("fail: vow_SetPayloaddump\n");

		/* if vow is not enable, then return */
		if (VowDrv_GetHWStatus() != VOW_PWR_ON) {
			vowserv.vow_recovering = false;
			VOWDRV_DEBUG("fail: vow not enable\n");
			break;
		}
		if (vowserv.scp_recovering) {
			vowserv.vow_recovering = false;
			VOWDRV_DEBUG("fail: vow recover3\n");
			break;
		}

		/* pcm dump recover */
		VOWDRV_DEBUG("recording_flag = %d, dump_pcm_flag = %d\n",
				vowserv.recording_flag, vowserv.dump_pcm_flag);
		if (vowserv.recording_flag == true)
			VowDrv_SetFlag(VOW_FLAG_DEBUG, true);

		if (vowserv.dump_pcm_flag == true)
			vow_pcm_dump_notify(true);

		if (vowserv.scp_recovering) {
			vowserv.vow_recovering = false;
			VOWDRV_DEBUG("fail: vow recover4\n");
			break;
		}
		if (vow_service_NotifyCHREStatus(vowserv.chre_status) == false)
			VOWDRV_DEBUG("fail: vow_NotifyCHREStatus\n");

		if (!VowDrv_SetProviderType(vowserv.provider_type))
			VOWDRV_DEBUG("fail: vow_SetMtkifType\n");

		if (!VowDrv_SetSpeakerNumber())
			VOWDRV_DEBUG("fail: VowDrv_SetSpeakerNumber\n");

		if (!vow_service_Enable())
			VOWDRV_DEBUG("fail: vow_service_Enable\n");

		vowserv.vow_recovering = false;
		break;
	}
	case SCP_EVENT_STOP:
		vowserv.scp_recovering = true;
		VOWDRV_DEBUG("%s(), SCP_EVENT_STOP\n", __func__);

		vowserv.scp_recover_data = VOW_SCP_EVENT_STOP;
		vow_service_getScpRecover();
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block vow_scp_recover_notifier = {
	.notifier_call = vow_scp_recover_event,
};
#endif  /* #if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT) */

/*****************************************************************************
 * VOW platform driver Registration
 *****************************************************************************/
static int VowDrv_probe(struct platform_device *dev)
{
	VOWDRV_DEBUG("%s()\n", __func__);

#ifdef VOW_SMART_DEVICE_SUPPORT
	VowDrv_setup_smartdev_eint(dev);
#endif

	return 0;
}

static int VowDrv_remove(struct platform_device *dev)
{
	VOWDRV_DEBUG("%s()\n", __func__);
	return 0;
}

static void VowDrv_shutdown(struct platform_device *dev)
{
	VOWDRV_DEBUG("%s()\n", __func__);
}

static int VowDrv_suspend(struct platform_device *dev, pm_message_t state)
{
	/* only one suspend mode */
	VOWDRV_DEBUG("%s()\n", __func__);
	return 0;
}

static int VowDrv_resume(struct platform_device *dev) /* wake up */
{
	VOWDRV_DEBUG("%s()\n", __func__);
	return 0;
}

static const struct file_operations VOW_fops = {
	.owner   = THIS_MODULE,
	.open    = VowDrv_open,
	.release = VowDrv_release,
	.unlocked_ioctl   = VowDrv_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = VowDrv_compat_ioctl,
#endif
	.write   = VowDrv_write,
	.read    = VowDrv_read,
	.flush   = VowDrv_flush,
	.fasync  = VowDrv_fasync,
	.mmap    = VowDrv_remap_mmap
};

static struct miscdevice VowDrv_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = VOW_DEVNAME,
	.fops = &VOW_fops,
};

const struct dev_pm_ops VowDrv_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
	.freeze = NULL,
	.thaw = NULL,
	.poweroff = NULL,
	.restore = NULL,
	.restore_noirq = NULL,
};

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id vow_of_match[] = {
	{.compatible = "mediatek,vow"},
	{},
};
#endif

static struct platform_driver VowDrv_driver = {
	.probe    = VowDrv_probe,
	.remove   = VowDrv_remove,
	.shutdown = VowDrv_shutdown,
	.suspend  = VowDrv_suspend,
	.resume   = VowDrv_resume,
	.driver   = {
#if IS_ENABLED(CONFIG_PM)
	.pm       = &VowDrv_pm_ops,
#endif
	.name     = "VOW_driver_device",
#if IS_ENABLED(CONFIG_OF)
	.of_match_table = vow_of_match,
#endif
	},
};

static int __init VowDrv_mod_init(void)
{
	int ret = 0;

	VOWDRV_DEBUG("+%s()\n", __func__);

	/* Register platform DRIVER */
	ret = platform_driver_register(&VowDrv_driver);
	if (ret != 0) {
		VOWDRV_DEBUG("VowDrv Fail:%d - Register DRIVER\n", ret);
		return ret;
	}

	/* register MISC device */
	ret = misc_register(&VowDrv_misc_device);
	if (ret != 0) {
		VOWDRV_DEBUG("VowDrv_probe misc_register Fail:%d\n", ret);
		return ret;
	}

	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetPhase1);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetPhase2);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetBypassPhase3);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_GetEnterPhase3Counter);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetLibLog);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetEnableHW);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetBargeIn);
	if (unlikely(ret != 0))
		return ret;
	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_DualMicSwitch);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SplitDumpFile);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetMCPS);
	if (unlikely(ret != 0))
		return ret;

	ret = device_create_file(VowDrv_misc_device.this_device,
				 &dev_attr_vow_SetPatternInput);
	if (unlikely(ret != 0))
		return ret;
	/* ipi register */
	vow_ipi_register(vow_ipi_rx_internal, vow_ipi_rceive_ack);

	vow_service_Init();
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	scp_A_register_notify(&vow_scp_recover_notifier);
#endif
	vow_suspend_lock = wakeup_source_register(NULL, "vow wakelock");
	vow_ipi_suspend_lock = wakeup_source_register(NULL, "vow ipi wakelock");
	vow_dump_suspend_lock = wakeup_source_register(NULL, "vow dump wakelock");

	if (!vow_suspend_lock)
		pr_debug("vow wakeup source init failed.\n");

	if (!vow_ipi_suspend_lock)
		pr_debug("vow ipi wakeup source init failed.\n");

	if (!vow_dump_suspend_lock)
		pr_debug("vow dump wakeup source init failed.\n");

	VOWDRV_DEBUG("-%s(): Init Audio WakeLock\n", __func__);

	return 0;
}

static void __exit VowDrv_mod_exit(void)
{
	VOWDRV_DEBUG("+%s()\n", __func__);
	wakeup_source_unregister(vow_suspend_lock);
	wakeup_source_unregister(vow_ipi_suspend_lock);
	wakeup_source_unregister(vow_dump_suspend_lock);
	/* extra data memory release */
	mutex_lock(&vow_extradata_mutex);
	if (vowserv.extradata_mem_ptr != NULL) {
		vfree(vowserv.extradata_mem_ptr);
		vowserv.extradata_mem_ptr = NULL;
	}
	mutex_unlock(&vow_extradata_mutex);
	VOWDRV_DEBUG("-%s()\n", __func__);
}
module_init(VowDrv_mod_init);
module_exit(VowDrv_mod_exit);

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

/*****************************************************************************
 * License
 *****************************************************************************/
MODULE_AUTHOR("Michael Hsiao <michael.hsiao@mediatek.com>");
MODULE_DESCRIPTION("MediaTek VoW Driver");
MODULE_LICENSE("GPL v2");

