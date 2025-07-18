/* SPDX-License-Identifier: GPL-2.0 */
/*
 * vow.h  --  VoW platform driver definition
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

#ifndef __VOW_H__
#define __VOW_H__

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "vow_scp.h"

/*****************************************************************************
 * VOW Type Define
 *****************************************************************************/
#define DEBUG_VOWDRV 1

#if DEBUG_VOWDRV
#define VOWDRV_DEBUG(format, args...) pr_info(format, ##args)
#else
#define VOWDRV_DEBUG(format, args...)
#endif

#define DEBUG_IPI_RX 0

#if IS_ENABLED(CONFIG_MTK_VOW_GVA_SUPPORT)
#define VOW_GOOGLE_MODEL 1
#else
#define VOW_GOOGLE_MODEL 0
#endif

#if IS_ENABLED(CONFIG_MTK_VOW_AMAZON_SUPPORT)
#define VOW_AMAZON_MODEL 1
#else
#define VOW_AMAZON_MODEL 0
#endif

#define VOW_DEVNAME                    "vow"
#define VOW_IOC_MAGIC                  'V'
#define VOW_PRE_LEARN_MODE             (1)

#define MAX_VOW_SPEAKER_MODEL          (2)

#define VOW_WAITCHECK_INTERVAL_MS      (2)
#define MAX_VOW_INFO_LEN               (7)
#define VOW_VOICE_RECORD_THRESHOLD     (1280) /* 40ms */
#define VOW_VOICE_RECORD_BIG_THRESHOLD (8320) /* 260ms */
#define VOW_IPI_SEND_CNT_TIMEOUT       (50) /* 50 loop */
/* UBM_V1:0xA000, UBM_V2:0xDC00, UBM_V3: 2*0x11000, UBM_V4: 2*0x16800  */
#define VOW_MODEL_SIZE_THRES           (0x2800)
#define VOW_MODEL_SIZE                 (0x16800)
#define VOW_VOICEDATA_OFFSET           (VOW_MODEL_SIZE * MAX_VOW_SPEAKER_MODEL)
#define VOW_VOICEDATA_SIZE             (0x17F00) /* 96000 + 6*320, need over 3sec */
#define VOW_NORMAL_REC_SIZE            (0x17C00) /* 3sec(96000B) can be divided by 320byte */
/* IPI return value definition */
#define WORD_H                         (16)
#define WORD_L                         (0)
#define WORD_H_MASK                    (0xFFFF0000)
#define WORD_L_MASK                    (0x0000FFFF)
/* multiplier of cycle to ns in 13m clock */
#define CYCLE_TO_NS                    (77)
#define VOW_STOP_DUMP_WAIT             (50)
#define FRAME_BUF_SIZE                 (8192)
#define RESERVED_DATA                  (4)
#define VOW_RECOVERY_WAIT              (100)

#define VOW_MAX_MIC_NUM                (2)
#define VOW_MAX_ECHO_NUM               (2)
#define VOW_MAX_VFFP_OUTPUT_CH         (2)
#define VOW_DEFAULT_SPEAKER_NUM        (1)

/* length limitation sync by audio hal */
#define VOW_VBUF_LENGTH                (0x18100)  /* 0x17700 + 0x0A00 */

#define VOW_FRM_LEN                    (160)
#define AECOUT_DUMP_SMPL_CNT           (VOW_FRM_LEN * 16)
#define AECOUT_DUMP_BYTE_CNT           (AECOUT_DUMP_SMPL_CNT * sizeof(short))
#define AECOUT_DUMP_TOTAL_BYTE_CNT     (AECOUT_DUMP_BYTE_CNT * VOW_MAX_MIC_NUM)
#define VFFPIN_DUMP_SMPL_CNT           (VOW_FRM_LEN * 50)
#define VFFPIN_DUMP_BYTE_CNT           (VFFPIN_DUMP_SMPL_CNT * sizeof(short))
#define VFFPIN_DUMP_TOTAL_BYTE_CNT     (VFFPIN_DUMP_BYTE_CNT * VOW_MAX_MIC_NUM)
#define VFFPOUT_DUMP_SMPL_CNT          (VOW_FRM_LEN * 50)
#define VFFPOUT_DUMP_BYTE_CNT          (VFFPOUT_DUMP_SMPL_CNT * sizeof(short))
#define VFFPOUT_DUMP_TOTAL_BYTE_CNT    (VFFPOUT_DUMP_BYTE_CNT * VOW_MAX_VFFP_OUTPUT_CH)
#define BARGEIN_DUMP_SMPL_CNT_MIC      (VOW_FRM_LEN * 16)
#define BARGEIN_DUMP_BYTE_CNT_MIC      (BARGEIN_DUMP_SMPL_CNT_MIC * sizeof(short))
#define BARGEIN_DUMP_TOTAL_BYTE_CNT_MIC (BARGEIN_DUMP_BYTE_CNT_MIC * VOW_MAX_MIC_NUM)
#define BARGEIN_DUMP_SMPL_CNT_ECHO     (VOW_FRM_LEN * 16)
#define BARGEIN_DUMP_BYTE_CNT_ECHO     (BARGEIN_DUMP_SMPL_CNT_ECHO * sizeof(short))
#define BARGEIN_DUMP_TOTAL_BYTE_CNT_ECHO (BARGEIN_DUMP_BYTE_CNT_ECHO * VOW_MAX_ECHO_NUM)
#define VOW_MAX_CH_NUM                 (2)   /* used in dump interleaving */
#define VOW_MAX_SCP_DMIC_CH_NUM        (3)

#define VOW_EXTRA_DATA_SIZE            0x100 /* 256 */
#define VOW_CUSTOM_MODEL_SIZE          0x2800 // 10KB = 0x2800
#define VOW_MAX_CUST_MODEL_SIZE        (3000)

#define VOW_ENGINE_INFO_LENGTH_BYTE    64

#define VOW_AECOUTDATA_OFFSET          (VOW_VOICEDATA_OFFSET + VOW_MAX_MIC_NUM * VOW_VOICEDATA_SIZE)
#define VOW_VFFPOUTDATA_OFFSET         (VOW_AECOUTDATA_OFFSET + AECOUT_DUMP_TOTAL_BYTE_CNT)
#define VOW_VFFPINDATA_OFFSET          (VOW_VFFPOUTDATA_OFFSET + VFFPOUT_DUMP_TOTAL_BYTE_CNT)
#define VOW_EXTRA_DATA_OFFSET          (VOW_VFFPINDATA_OFFSET + VFFPIN_DUMP_TOTAL_BYTE_CNT)
#define VOW_CUSTOM_MODEL_OFFSET        (VOW_EXTRA_DATA_OFFSET + VOW_EXTRA_DATA_SIZE)
#define VOW_PAYLOADDUMP_OFFSET         (VOW_CUSTOM_MODEL_OFFSET + (((VOW_MAX_CUST_MODEL_SIZE >> 8) + 1) << 8))
#define VOW_PAYLOADDUMP_R_OFFSET       (VOW_PAYLOADDUMP_OFFSET + VOW_VOICEDATA_SIZE)
#define VOW_DRAM_REGION_END_OFFSET     (VOW_PAYLOADDUMP_R_OFFSET + VOW_VOICEDATA_SIZE)

/* below is control message */
#define VOW_SET_CONTROL               _IOW(VOW_IOC_MAGIC, 0x03, unsigned int)
#define VOW_SET_SPEAKER_MODEL         _IOW(VOW_IOC_MAGIC, 0x04, unsigned int)
#define VOW_CLR_SPEAKER_MODEL         _IOW(VOW_IOC_MAGIC, 0x05, unsigned int)
#define VOW_SET_APREG_INFO            _IOW(VOW_IOC_MAGIC, 0x09, unsigned int)
#define VOW_BARGEIN_ON                _IOW(VOW_IOC_MAGIC, 0x0A, unsigned int)
#define VOW_BARGEIN_OFF               _IOW(VOW_IOC_MAGIC, 0x0B, unsigned int)
#define VOW_CHECK_STATUS              _IOW(VOW_IOC_MAGIC, 0x0C, unsigned int)
#define VOW_RECOG_ENABLE              _IOW(VOW_IOC_MAGIC, 0x0D, unsigned int)
#define VOW_RECOG_DISABLE             _IOW(VOW_IOC_MAGIC, 0x0E, unsigned int)
#define VOW_MODEL_START               _IOW(VOW_IOC_MAGIC, 0x0F, unsigned int)
#define VOW_MODEL_STOP                _IOW(VOW_IOC_MAGIC, 0x10, unsigned int)
#define VOW_GET_ALEXA_ENGINE_VER      _IOW(VOW_IOC_MAGIC, 0x11, unsigned int)
#define VOW_GET_GOOGLE_ENGINE_VER     _IOW(VOW_IOC_MAGIC, 0x12, unsigned int)
#define VOW_GET_GOOGLE_ARCH           _IOW(VOW_IOC_MAGIC, 0x13, unsigned int)
#define VOW_SET_DSP_AEC_PARAMETER     _IOW(VOW_IOC_MAGIC, 0x14, unsigned int)
#define VOW_SET_PAYLOADDUMP_INFO      _IOW(VOW_IOC_MAGIC, 0x16, unsigned int)
#define VOW_READ_VOICE_DATA           _IOW(VOW_IOC_MAGIC, 0x17, unsigned int)
#define VOW_SET_VOW_DUMP_DATA         _IOW(VOW_IOC_MAGIC, 0x18, unsigned int)
#define VOW_GET_SCP_RECOVER_STATUS    _IOW(VOW_IOC_MAGIC, 0x19, unsigned int)
#define VOW_SET_VOW_DUAL_CH_TRANSFER  _IOW(VOW_IOC_MAGIC, 0x1A, unsigned int)
#define VOW_NOTIFY_CHRE_STATUS        _IOW(VOW_IOC_MAGIC, 0x1B, unsigned int)
#define VOW_SET_VOW_DELAY_WAKEUP      _IOW(VOW_IOC_MAGIC, 0x1C, unsigned int)
#define VOW_SET_VOW_PAYLOAD_CALLBACK  _IOW(VOW_IOC_MAGIC, 0x1D, unsigned int)

#ifdef VOW_ECHO_SW_SRC
#define VOW_BARGEIN_AFE_MEMIF_SIZE        (0x1E00)
#else
#define VOW_BARGEIN_AFE_MEMIF_SIZE        (0xA00)
#endif
#define VOW_BARGEIN_AFE_MEMIF_MAX_SIZE    (0x2000)

#define VOW_BARGEIN_IRQ_MAX_NUM           (32)

#define KERNEL_VOW_DRV_VER                "3.0.0"
#define DEFAULT_GOOGLE_ENGINE_VER         (1235201314)  /* set meaningless default value */
#define MAGIC_PROVIDER_NUMBER             (0xABCD)      /* set meaningless default value */
#define MAGIC_IOCTL_NUMBER                (0xDEADBEEF)  /* set meaningless default value */

#define REC_QUEUE_NUM                     (10)

enum { /* dump_data_t */
	DUMP_AECOUT = 0,
	DUMP_VFFPOUT,
	DUMP_BARGEIN,
	DUMP_INPUT,
	DUMP_VFFPIN,
	NUM_DUMP_DATA,
};


/*****************************************************************************
 * VOW Enum
 *****************************************************************************/
enum vow_control_cmd_t {
	VOWControlCmd_Init = 0,
	VOWControlCmd_EnableHotword,
	VOWControlCmd_DisableHotword,
	VOWControlCmd_EnableSeamlessRecord,
	VOWControlCmd_EnableDump,
	VOWControlCmd_DisableDump,
	VOWControlCmd_GetDump,
	VOWControlCmd_Reset,
	VOWControlCmd_Mic_Single,
	VOWControlCmd_Mic_Dual,
	VOWControlCmd_Speaker_Single,
	VOWControlCmd_Speaker_Dual
};

enum vow_ipi_msgid_t {
	IPIMSG_VOW_ENABLE = 0,
	IPIMSG_VOW_DISABLE = 1,
	IPIMSG_VOW_SETMODE = 2,
	IPIMSG_VOW_APINIT = 3,
	IPIMSG_VOW_SET_MODEL = 4,
	IPIMSG_VOW_SET_FLAG = 5,
	//IPIMSG_VOW_SET_SMART_DEVICE = 6,
	IPIMSG_VOW_SET_BARGEIN_ON = 10,
	IPIMSG_VOW_SET_BARGEIN_OFF = 11,
	IPIMSG_VOW_PCM_DUMP_ON = 12,
	IPIMSG_VOW_PCM_DUMP_OFF = 13,
	IPIMSG_VOW_COMBINED_INFO = 17,
	IPIMSG_VOW_MODEL_START = 18,
	IPIMSG_VOW_MODEL_STOP = 19,
	IPIMSG_VOW_RETURN_VALUE = 20,
	IPIMSG_VOW_SET_SKIP_SAMPLE_COUNT = 21,
	IPIMSG_VOW_GET_ALEXA_ENGINE_VER = 22,
	IPIMSG_VOW_GET_GOOGLE_ENGINE_VER = 23,
	IPIMSG_VOW_GET_GOOGLE_ARCH = 24,
	IPIMSG_VOW_ALEXA_ENGINE_VER = 25,
	IPIMSG_VOW_GOOGLE_ENGINE_VER = 26,
	IPIMSG_VOW_GOOGLE_ARCH = 27,
	IPIMSG_VOW_SET_CUSTOM_MODEL = 28,
	IPIMSG_VOW_HAL_REBOOT = 29,
	//IPIMSG_VOW_FLUSH = 30,
	IPIMSG_VOW_SCP_BARGE_IN_RESUME = 31,
	/*------ sound_soc-vow-kernel ------*/
	IPIMSG_VOW_PCM_HWFREE = 100
};

enum vow_eint_status_t {
	VOW_EINT_DISABLE = -2,
	VOW_EINT_FAIL = -1,
	VOW_EINT_PASS = 0,
	VOW_EINT_RETRY = 1,
	NUM_OF_VOW_EINT_STATUS = 4
};

enum vow_flag_type_t {
	VOW_FLAG_DEBUG = 0,
	VOW_FLAG_PRE_LEARN,
	VOW_FLAG_PERIODIC_ENABLE,
	VOW_FLAG_FORCE_PHASE1_DEBUG,
	VOW_FLAG_FORCE_PHASE2_DEBUG,
	VOW_FLAG_SWIP_LOG_PRINT,
	VOW_FLAG_PROVIDER_TYPE,
	VOW_FLAG_SEAMLESS,
	VOW_FLAG_DUAL_MIC_SWITCH,
	VOW_FLAG_MCPS,
	VOW_FLAG_SPEAKER_NUMBER,
	VOW_FLAG_CHRE_STATUS,
	VOW_FLAG_WAKEUP_DELAY_TIME,
	VOW_FLAG_PAYLOADDUMP_CB_TYPE,
	NUM_OF_VOW_FLAG_TYPE
};

enum vow_pwr_status_t {
	VOW_PWR_OFF = 0,
	VOW_PWR_ON = 1,
	NUM_OF_VOW_PWR_STATUS
};

enum vow_ipi_result_t {
	VOW_IPI_SUCCESS = 0,
	VOW_IPI_CLR_SMODEL_ID_NOTMATCH,
	VOW_IPI_SET_SMODEL_NO_FREE_SLOT,
};

enum vow_force_phase_t {
	NO_FORCE = 0,
	FORCE_PHASE1,
	FORCE_PHASE2,
};

enum vow_model_type_t {
	VOW_MODEL_INIT = 0,    /* no use */
	VOW_MODEL_SPEAKER = 1,
	VOW_MODEL_NOISE = 2,   /* no use */
	VOW_MODEL_FIR = 3,      /* no use */
	VOW_MODEL_CLEAR = 4
};

enum vow_provider_t {
	VOW_PROVIDER_NONE = 0,
	VOW_PROVIDER_SCP_FIFO = 1,
	VOW_PROVIDER_STANDALONE_CODEC = 2,
	VOW_PROVIDER_VIRTUAL = 3,
	VOW_PROVIDER_SCP_DMIC_FIFO = 4,
	VOW_PROVIDER_MAX
};

enum vow_channel_t {
	VOW_MONO = 0,
	VOW_STEREO = 1,
	VOW_CH_MAX
};

enum vow_model_status_t {
	VOW_MODEL_STATUS_START = 1,
	VOW_MODEL_STATUS_STOP = 0
};

enum vow_model_control_t {
	VOW_SET_MODEL = 0,
	VOW_CLEAN_MODEL = 1
};

enum {
	VENDOR_ID_MTK = 77,     //'M'
	VENDOR_ID_AMAZON = 65,  //'A'
	VENDOR_ID_OTHERS = 71,
	VENDOR_ID_NONE = 0
};

enum {
	VOW_ENABLE_DUAL_MIC = 2,
	VOW_ENABLE_SINGLE_MAIN_MIC = 1,
	VOW_ENABLE_SINGLE_REF_MIC = 0
};

enum {
	VOW_SCP_EVENT_NONE = 0,
	VOW_SCP_EVENT_STOP = 5,
	VOW_SCP_EVENT_READY = 6
};

enum chre_status_t {
	CHRE_DO_NOTHING = 0,
	CHRE_CLOSE = 1,
	CHRE_OPEN = 2
};

enum vow_payloaddump_setting_t {
	PAYLOADDUMP_OFF = 0,
	PAYLOADDUMP_1ST_STAGE = 1,
	PAYLOADDUMP_2ND_STAGE = 2,
	PAYLOADDUMP_MAX_NUM
};

/*****************************************************************************
 * VOW Structure Define
 *****************************************************************************/
struct vow_ipi_msg_t {
	short id;
	short size;
	short *buf;
};

struct vow_eint_data_struct_t {
	int size;        /* size of data section */
	int eint_status; /* eint status */
	int id;          /* user id */
	char data[RESERVED_DATA];    /* reserved for future extension */
};

struct vow_rec_queue_info_t {
	unsigned int rec_buf_offset;
	unsigned int rec_buf_length;
};

#if IS_ENABLED(CONFIG_COMPAT)

struct vow_speaker_model_t {
	void *model_ptr;
	int  id;
	int  keyword;
	int  uuid;
	int  flag;
	int  enabled;
	unsigned int model_size;
	unsigned int confidence_lv;
	unsigned long rx_inform_addr;
	unsigned long rx_inform_size_addr;
};

struct vow_model_info_t {
	long  id;
	long  keyword;
	long  addr;
	long  size;
	long  return_size_addr;
	long  uuid;
	void *data;
};

struct vow_model_start_t {
	long handle;
	long confidence_level;
	long dsp_inform_addr;
	long dsp_inform_size_addr;
};
struct vow_model_info_kernel_t {
	compat_size_t  id;
	compat_size_t  keyword;
	compat_size_t  addr;
	compat_size_t  size;
	compat_size_t  return_size_addr;
	compat_size_t  uuid;
	compat_uptr_t *data;
};

struct vow_model_start_kernel_t {
	compat_size_t handle;
	compat_size_t confidence_level;
	compat_size_t dsp_inform_addr;
	compat_size_t dsp_inform_size_addr;
};

struct vow_engine_info_t {
	long return_size_addr;
	long data_addr;
};

struct vow_engine_info_kernel_t {
	compat_size_t return_size_addr;
	compat_size_t data_addr;
};

struct vow_payloaddump_info_t {
	long return_payloaddump_addr;
	long return_payloaddump_size_addr;
	long max_payloaddump_size;
};

struct vow_payloaddump_info_kernel_t {
	compat_size_t return_payloaddump_addr;
	compat_size_t return_payloaddump_size_addr;
	compat_size_t max_payloaddump_size;
};

struct vow_scp_recover_info_t {
	long return_event_addr;
};

struct vow_scp_recover_info_kernel_t {
	compat_size_t return_event_addr;
};

struct vow_ioctl_arg_info_t {
  long magic_number;
  long return_data;
};

struct vow_ioctl_arg_info_kernel_t {
  compat_size_t magic_number;
  compat_size_t return_data;
};

#else  /* #if IS_ENABLED(CONFIG_COMPAT) */

struct vow_speaker_model_t {
	void *model_ptr;
	int  id;
	int  uuid;
	int  flag;
	int  enabled;
	unsigned int model_size;
	unsigned int confidence_lv;
	unsigned long rx_inform_addr;
	unsigned long rx_inform_size_addr;
};

struct vow_model_info_t {
	long  id;
	long  keyword;
	long  addr;
	long  size;
	long  return_size_addr;
	long  uuid;
	void *data;
};

struct vow_model_start_t {
	long handle;
	long confidence_level;
	long dsp_inform_addr;
	long dsp_inform_size_addr;
};

struct vow_engine_info_t {
	long return_size_addr;
	long data_addr;
};

struct vow_payloaddump_info_t {
	long return_payloaddump_addr;
	long return_payloaddump_size_addr;
	long max_payloaddump_size;
};

struct vow_scp_recover_info_t {
	long return_event_addr;
};

struct vow_ioctl_arg_info_t {
  long magic_number;
  long return_data;
};

#endif  /* #if IS_ENABLED(CONFIG_COMPAT) */

enum ipi_type_flag_t {
	RECOG_OK_IDX = 0,
	DEBUG_DUMP_IDX = 1,
	AECOUT_DUMP_IDX = 2,
	VFFPIN_DUMP_IDX = 3,
	BARGEIN_DUMP_IDX = 4,
	INPUT_DUMP_IDX = 5,
	VFFPOUT_DUMP_IDX = 6
};

#define RECOG_OK_IDX_MASK           (0x01 << RECOG_OK_IDX)
#define DEBUG_DUMP_IDX_MASK         (0x01 << DEBUG_DUMP_IDX)
#define AECOUT_DUMP_IDX_MASK        (0x01 << AECOUT_DUMP_IDX)
#define VFFPIN_DUMP_IDX_MASK        (0x01 << VFFPIN_DUMP_IDX)
#define BARGEIN_DUMP_IDX_MASK       (0x01 << BARGEIN_DUMP_IDX)
#define INPUT_DUMP_IDX_MASK         (0x01 << INPUT_DUMP_IDX)
#define VFFPOUT_DUMP_IDX_MASK       (0x01 << VFFPOUT_DUMP_IDX)
#define SCP_DUMP_DATA_MASK	    (AECOUT_DUMP_IDX_MASK + \
				     VFFPIN_DUMP_IDX_MASK + \
				     BARGEIN_DUMP_IDX_MASK + \
				     INPUT_DUMP_IDX_MASK + \
				     VFFPOUT_DUMP_IDX_MASK)

struct vow_ipi_combined_info_t {
	unsigned short ipi_type_flag;
	/* IPIMSG_VOW_RECOGNIZE_OK */
	unsigned short recog_ok_keywordid;
	/* unsigned int recog_ret_info; */
	unsigned int confidence_lv;
	unsigned long long recog_ok_os_timer;
	unsigned int extra_data_len;
	/* IPIMSG_VOW_DATAREADY */
	unsigned int voice_buf_offset;
	unsigned int voice_length;
	/* IPIMSG_VOW_BARGEIN_DUMP_INFO */
	unsigned int vffpin_dump_size;
	unsigned int vffpin_dump_offset;
	unsigned int vffpin_dump_offset_R;
	/* IPIMSG_VOW_BARGEIN_PCMDUMP_OK */
	unsigned int mic_dump_size;
	unsigned int mic_offset;
//	unsigned int mic_dump_size_R;
	unsigned int mic_offset_R;
	unsigned int echo_dump_size;
	unsigned int echo_offset;
	unsigned int echo_offset_R;
	unsigned int aecout_dump_size;
	unsigned int aecout_dump_offset;
//	unsigned int aecout_dump_size_R;
	unsigned int aecout_dump_offset_R;
	unsigned int payloaddump_len;
	unsigned int vffpout_dump_size;
	unsigned int vffpout_dump_offset;
	unsigned int vffpout_dump_offset_2nd_ch;
};


/*****************************************************************************
 * VOW Function Declaration
 *****************************************************************************/
bool vow_service_GetScpRecoverStatus(void);
bool vow_service_GetVowRecoverStatus(void);
void vow_ipi_rx_internal(unsigned int msg_id, void *msg_data);
bool vow_ipi_rceive_ack(unsigned int msg_id, unsigned int msg_data);

#endif /*__VOW_H__ */
