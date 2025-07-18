// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.

#include <audio_ipi_platform.h>

#include <linux/printk.h>
#include <linux/bug.h>

#include <audio_task.h>

#include <adsp_helper.h>
#include <scp.h>

/*
 * =============================================================================
 *                     log
 * =============================================================================
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[IPI][CHIP] %s(), " fmt "\n", __func__


/*
 * =============================================================================
 *                     by chip
 * =============================================================================
 */

uint32_t audio_get_dsp_id(const uint8_t task)
{
	uint32_t dsp_id = AUDIO_OPENDSP_ID_INVALID;

	switch (task) {
	case TASK_SCENE_VOICE_ULTRASOUND:
	case TASK_SCENE_SPEAKER_PROTECTION:
	case TASK_SCENE_VOW:
	case TASK_SCENE_AUDIO_CONTROLLER_CM4:
		dsp_id = AUDIO_OPENDSP_USE_CM4_A;
		break;
	case TASK_SCENE_PLAYBACK_MP3:
	case TASK_SCENE_PRIMARY:
	case TASK_SCENE_DEEPBUFFER:
	case TASK_SCENE_AUDPLAYBACK:
	case TASK_SCENE_A2DP:
	case TASK_SCENE_BLEDL:
	case TASK_SCENE_BLEENC:
	case TASK_SCENE_BTDL:
	case TASK_SCENE_DATAPROVIDER:
	case TASK_SCENE_AUD_DAEMON_A:
	case TASK_SCENE_AUDIO_CONTROLLER_HIFI3_A:
	case TASK_SCENE_CALL_FINAL:
	case TASK_SCENE_MUSIC:
	case TASK_SCENE_FAST:
	case TASK_SCENE_FAST_MEDIA:
	case TASK_SCENE_SPATIALIZER:
	case TASK_SCENE_DYNAMIC:
#if !IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
	case TASK_SCENE_FM_ADSP:
#endif
	case TASK_SCENE_BLECALLDL:
		dsp_id = AUDIO_OPENDSP_USE_HIFI3_A;
		break;
	case TASK_SCENE_VOIP:
#if IS_ENABLED(CONFIG_ADSP_VOIP_LEGACY)
		dsp_id = AUDIO_OPENDSP_USE_HIFI3_B;
#else
		dsp_id = AUDIO_OPENDSP_USE_HIFI3_A;
#endif
		break;
	case TASK_SCENE_ECHO_REF_DL:
	case TASK_SCENE_USB_DL:
	case TASK_SCENE_USB_UL:
	case TASK_SCENE_MD_DL:
	case TASK_SCENE_MD_UL:
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	case TASK_SCENE_EXTSTREAM1:
	case TASK_SCENE_EXTSTREAM2:
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT)
	case TASK_SCENE_SUB_PLAYBACK:
#endif
	case TASK_SCENE_PLAYBACK0:
	case TASK_SCENE_PLAYBACK1:
	case TASK_SCENE_PLAYBACK2:
	case TASK_SCENE_PLAYBACK3:
	case TASK_SCENE_PLAYBACK4:
	case TASK_SCENE_PLAYBACK5:
	case TASK_SCENE_PLAYBACK6:
	case TASK_SCENE_PLAYBACK7:
	case TASK_SCENE_PLAYBACK8:
	case TASK_SCENE_PLAYBACK9:
	case TASK_SCENE_PLAYBACK10:
	case TASK_SCENE_PLAYBACK11:
	case TASK_SCENE_PLAYBACK12:
	case TASK_SCENE_PLAYBACK13:
	case TASK_SCENE_PLAYBACK14:
	case TASK_SCENE_PLAYBACK15:
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	case TASK_SCENE_HFP_CLIENT_TX:
#endif
#endif
		dsp_id = AUDIO_OPENDSP_USE_HIFI3_A;
		break;
	case TASK_SCENE_RECORD:
	case TASK_SCENE_CAPTURE_UL1:
	case TASK_SCENE_AUD_DAEMON_B:
	case TASK_SCENE_KTV:
	case TASK_SCENE_CAPTURE_RAW:
	case TASK_SCENE_BLEUL:
	case TASK_SCENE_BLEDEC:
	case TASK_SCENE_BLECALLUL:
	case TASK_SCENE_BTUL:
	case TASK_SCENE_UL_PROCESS:
	case TASK_SCENE_ECHO_REF_UL:
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	case TASK_SCENE_HFP_CLIENT_RX:
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	case TASK_SCENE_ANC:
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
	case TASK_SCENE_FM_ADSP:
	case TASK_SCENE_CAPTURE_MCH:
#endif
		if (get_adsp_core_total() > 1)
			dsp_id = AUDIO_OPENDSP_USE_HIFI3_B;
		else
			dsp_id = AUDIO_OPENDSP_USE_HIFI3_A;
		break;
	case TASK_SCENE_AUDIO_CONTROLLER_HIFI3_B:
		if (get_adsp_core_total() > 1)
			dsp_id = AUDIO_OPENDSP_USE_HIFI3_B;
		else {
			pr_notice("task %d not support!!", task);
			dsp_id = AUDIO_OPENDSP_ID_INVALID;
		}
		break;
	case TASK_SCENE_AUDIO_CONTROLLER_RV:
	case TASK_SCENE_RV_SPK_PROCESS:
		dsp_id = AUDIO_OPENDSP_USE_RV_A;
		break;
	case TASK_SCENE_PHONE_CALL:
		if (get_adsp_core_total() > 1)
			dsp_id = AUDIO_OPENDSP_USE_HIFI3_B;
		else if (get_adsp_core_total() > 0)
			dsp_id = AUDIO_OPENDSP_USE_HIFI3_A;
		else if (SCP_CORE_TOTAL > 0)
			dsp_id = AUDIO_OPENDSP_USE_RV_A;
		break;
	case TASK_SCENE_PHONE_CALL_SUB:
		if (get_adsp_core_total() > 0)
			dsp_id = AUDIO_OPENDSP_USE_HIFI3_A;
		else if (SCP_CORE_TOTAL > 0)
			dsp_id = AUDIO_OPENDSP_USE_RV_A;
		break;
	default:
		pr_notice("task %d not support!!", task);
		dsp_id = AUDIO_OPENDSP_ID_INVALID;
	}

	return dsp_id;
}
EXPORT_SYMBOL_GPL(audio_get_dsp_id);
