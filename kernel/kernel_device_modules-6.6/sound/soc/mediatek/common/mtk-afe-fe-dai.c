// SPDX-License-Identifier: GPL-2.0
/*
 * mtk-afe-fe-dais.c  --  Mediatek afe fe dai operator
 *
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Garlic Tseng <garlic.tseng@mediatek.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include "mtk-afe-platform-driver.h"
#include <sound/pcm_params.h>
#include "mtk-afe-fe-dai.h"
#include "mtk-base-afe.h"
#include "mtk-afe-external.h"

#if IS_ENABLED(CONFIG_SND_SOC_MTK_SRAM)
#include "mtk-sram-manager.h"
#endif
/* dsp relate */
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
#include "../audio_dsp/mtk-dsp-common_define.h"
#include "../audio_dsp/mtk-dsp-common.h"
#include "../audio_dsp/mtk-dsp-mem-control.h"
#endif

#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
#include "adsp_helper.h"
#endif

/* scp relate */
#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
#include "../audio_scp/mtk-scp-audio-pcm.h"
#include "../audio_scp/mtk-scp-audio-mem-control.h"
#endif

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

/* ultra relate */
#if IS_ENABLED(CONFIG_MTK_ULTRASND_PROXIMITY)
#include "../ultrasound/ultra_common/mtk-scp-ultra.h"
#endif

#if !IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
#include "mtk-mmap-ion.h"
#endif

#define AFE_BASE_END_OFFSET 8
#define AFE_AGENT_SET_OFFSET 4
#define AFE_AGENT_CLR_OFFSET 8

static unsigned int g_channel;

int mtk_get_channel_value(void)
{
	return g_channel;
}
EXPORT_SYMBOL(mtk_get_channel_value);

static bool (*afe_is_vow_memif)(int);

void register_is_vow_bargein_memif_callback(bool (*callback)(int id))
{
	afe_is_vow_memif = callback;
}
EXPORT_SYMBOL(register_is_vow_bargein_memif_callback);

static bool is_semaphore_control_need(bool is_scp_sema_support)
{
	bool is_adsp_active = false;

#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	is_adsp_active = is_adsp_feature_in_active();
#endif

	/* If is_scp_sema_support is true,
	 * scp semaphore is to ensure AP/SCP/ADSP synchronization.
	 * Otherwise, using adsp semaphore for synchronization
	 * if adsp feature is active.
	 */
	return is_scp_sema_support | is_adsp_active;
}

int mtk_regmap_update_bits(struct regmap *map, int reg,
			   unsigned int mask,
			   unsigned int val, int shift)
{
	if (reg < 0 || WARN_ON_ONCE(shift < 0))
		return 0;
	return regmap_update_bits(map, reg, mask << shift, val << shift);
}
EXPORT_SYMBOL(mtk_regmap_update_bits);

int mtk_regmap_write(struct regmap *map, int reg, unsigned int val)

{
	if (reg < 0)
		return 0;
	return regmap_write(map, reg, val);
}
EXPORT_SYMBOL(mtk_regmap_write);

#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
static void vow_barge_in_disable_memif(struct mtk_base_afe *afe,
				       struct mtk_base_afe_memif *memif)
{
	int irq_id = memif->irq_usage;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	unsigned int value = 0;
	unsigned int TargetBitField;
	unsigned int irq_enable;

	/* get memif irq status */
	regmap_read(afe->regmap, irq_data->irq_en_reg, &value);
	TargetBitField = ((0x1 << 1) - 1) << irq_data->irq_en_shift;
	irq_enable = (value & TargetBitField) >> irq_data->irq_en_shift;
	dev_info(afe->dev, "%s(), memif irq status = 0x%x, irq_enable = %d\n",
		 __func__, value, irq_enable);
	if (irq_enable) {
		mtk_regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
				       1, 0, irq_data->irq_en_shift);
		dev_info(afe->dev, "%s(), disable barge-in memif irq\n", __func__);

		/* after clear irq */
		regmap_read(afe->regmap, irq_data->irq_en_reg, &value);
		TargetBitField = ((0x1 << 1) - 1) << irq_data->irq_en_shift;
		irq_enable = (value & TargetBitField) >> irq_data->irq_en_shift;
		dev_dbg(afe->dev, "%s(), after clear irq, memif irq status= 0x%x, irq_enable= %d\n",
			__func__, value, irq_enable);
	}
}

static void vow_barge_in_hw_free(struct mtk_base_afe *afe, int id)
{
	struct vow_sound_soc_ipi_send_info vow_ipi_info;
	int ret = 0;

	vow_ipi_info.msg_id = SOUND_SOC_IPIMSG_VOW_PCM_HWFREE;
	vow_ipi_info.payload_len = 0;
	vow_ipi_info.payload = NULL;
	vow_ipi_info.need_ack = SOUND_SOC_VOW_IPI_BYPASS_ACK;
	ret = notify_vow_ipi_send(NOTIFIER_VOW_IPI_SEND, &vow_ipi_info);
	if (ret != NOTIFY_STOP) {
		dev_info(afe->dev, "%s(), IPIMSG_VOW_PCM_HWFREE ipi send error: %d\n",
			 __func__, ret);
		ret = mtk_memif_set_disable(afe, id);
		if (ret) {
			dev_info(afe->dev, "%s(), error, id %d, memif disable, ret %d\n",
				__func__, id, ret);
		}
	}
}
#endif

#if IS_ENABLED(CONFIG_MTK_ULTRASND_PROXIMITY)
static void ultra_stop_memif (struct mtk_base_afe *afe)
{
	int ret = 0;

	pr_info("%s, notify_ultra_afe_hw_free\n", __func__);
	ret = notify_ultra_afe_hw_free(NOTIFIER_ULTRA_AFE_HW_FREE, NULL);
	if (ret != NOTIFY_STOP)
		dev_info(afe->dev, "%s(),NOTIFIER_ULTRA_AFE_HW_FREE ipi send ret: %d\n",
			 __func__, ret);
}
#endif


int mtk_afe_fe_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int memif_num = asoc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_base_afe_memif *memif = &afe->memif[memif_num];
	const struct snd_pcm_hardware *mtk_afe_hardware = afe->mtk_afe_hardware;
	int ret;

	memif->substream = substream;

	snd_pcm_hw_constraint_step(substream->runtime, 0,
				   SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 16);
	/* enable agent */
	mtk_regmap_update_bits(afe->regmap, memif->data->agent_disable_reg,
			       1, 0, memif->data->agent_disable_shift);

	snd_soc_set_runtime_hwparams(substream, mtk_afe_hardware);

	/*
	 * Capture cannot use ping-pong buffer since hw_ptr at IRQ may be
	 * smaller than period_size due to AFE's internal buffer.
	 * This easily leads to overrun when avail_min is period_size.
	 * One more period can hold the possible unread buffer.
	 */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		int periods_max = mtk_afe_hardware->periods_max;

		ret = snd_pcm_hw_constraint_minmax(runtime,
						   SNDRV_PCM_HW_PARAM_PERIODS,
						   3, periods_max);
		if (ret < 0) {
			dev_err(afe->dev, "hw_constraint_minmax failed\n");
			return ret;
		}
	}

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		dev_err(afe->dev, "snd_pcm_hw_constraint_integer failed\n");

	/* dynamic allocate irq to memif */
	if (memif->irq_usage < 0) {
		int irq_id = mtk_dynamic_irq_acquire(afe);

		if (irq_id != afe->irqs_size) {
			/* link */
			memif->irq_usage = irq_id;
		} else {
			dev_err(afe->dev, "%s() error: no more asys irq\n",
				__func__);
			ret = -EBUSY;
		}
	}
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_afe_fe_startup);

void mtk_afe_fe_shutdown(struct snd_pcm_substream *substream,
			 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mtk_base_afe_memif *memif = &afe->memif[asoc_rtd_to_cpu(rtd, 0)->id];
	int irq_id;

	irq_id = memif->irq_usage;

	mtk_regmap_update_bits(afe->regmap, memif->data->agent_disable_reg,
			       1, 1, memif->data->agent_disable_shift);

	if (!memif->const_irq) {
		mtk_dynamic_irq_release(afe, irq_id);
		memif->irq_usage = -1;
		memif->substream = NULL;
	}
}
EXPORT_SYMBOL_GPL(mtk_afe_fe_shutdown);

int mtk_afe_fe_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int id = asoc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int ret = 0;
	unsigned int channels = params_channels(params);
	unsigned int rate = params_rate(params);
	snd_pcm_format_t format = params_format(params);
	struct snd_dma_buffer *dmab = NULL;
	bool using_dram = false;

#if !IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
	// mmap don't alloc buffer
	if (memif->use_mmap_share_mem != 0) {
		unsigned long phy_addr;
		void *vir_addr;

		substream->runtime->dma_bytes = params_buffer_bytes(params);
		if (substream->runtime->dma_bytes > MMAP_BUFFER_SIZE) {
			substream->runtime->dma_bytes = MMAP_BUFFER_SIZE;
			dev_warn(afe->dev, "%s(), mmap error buffer size\n",
				 __func__);
		}
		if (memif->use_mmap_share_mem == 1) {
			mtk_get_mmap_dl_buffer(&phy_addr, &vir_addr);
			dev_info(afe->dev, "%s, DL assign area %p, addr %ld\n",
				 __func__, vir_addr, phy_addr);
			substream->runtime->dma_area = vir_addr;
			substream->runtime->dma_addr = phy_addr;
		} else if (memif->use_mmap_share_mem == 2) {
			mtk_get_mmap_ul_buffer(&phy_addr, &vir_addr);
			dev_info(afe->dev, "%s, UL assign area %p, addr %ld\n",
				 __func__, vir_addr, phy_addr);
			substream->runtime->dma_area = vir_addr;
			substream->runtime->dma_addr = phy_addr;
		} else {
			dev_warn(afe->dev, "mmap share mem %d not support\n",
				 memif->use_mmap_share_mem);
		}
		goto MEM_ALLOCATE_DONE;
	}
#endif

#if IS_ENABLED(CONFIG_SND_SOC_MTK_SRAM)
#if IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
		// if (memif->using_passthrough) {
		if (memif_has_sram_passthrough_shm(memif)
			|| memif_has_dram_passthrough_shm(memif)) {
			substream->runtime->dma_addr = 0;
			substream->runtime->dma_area = NULL;
			substream->runtime->dma_bytes = 0;
			memif->using_passthrough = 0;
		} else {
			snd_pcm_lib_free_pages(substream);
		}

		substream->runtime->dma_bytes = params_buffer_bytes(params);

		if (memif->use_dram_only == 0) {
			if (memif_has_sram_passthrough_shm(memif)) {
				// Handle sram passthrough.
				substream->runtime->dma_addr = memif->sram_dma_addr;
				substream->runtime->dma_area = memif->sram_dma_area;
				// TODO check nbl_vmm set_hw_params function
				substream->runtime->dma_bytes = memif->sram_dma_bytes;
				memif->using_sram = 1;
				memif->using_passthrough = 1;
			}
			if (memif_has_dram_passthrough_shm(memif)) {
				// Handle dram passthrough.
				substream->runtime->dma_addr = memif->dram_dma_addr;
				substream->runtime->dma_area = memif->dram_dma_area;
				substream->runtime->dma_bytes = memif->dram_dma_bytes;
				memif->using_sram = 0;
				memif->using_passthrough = 1;
		} else	{
#if	IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
			if (memif->use_adsp_share_mem == true)
				ret = mtk_adsp_allocate_mem(substream,
							params_buffer_bytes(params));
			else
				ret = snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(params));
#else
				ret = snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(params));
#endif
			if (ret < 0)
				return ret;
			memif->using_sram = 0;
			memif->using_passthrough = 0;
			using_dram = true;
			}
		} else {
			if (memif_has_dram_passthrough_shm(memif)) { // Handle dram passthrough.
				substream->runtime->dma_addr = memif->dram_dma_addr;
				substream->runtime->dma_area = memif->dram_dma_area;
				// TODO check nbl_vmm set_hw_params function
				substream->runtime->dma_bytes = memif->dram_dma_bytes;
				memif->using_passthrough = 1;
		} else {
#if	IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
			if (memif->use_adsp_share_mem == true)
				ret = mtk_adsp_allocate_mem(substream,
						params_buffer_bytes(params));
			else
				ret = snd_pcm_lib_malloc_pages(substream,
						params_buffer_bytes(params));
#else
				ret = snd_pcm_lib_malloc_pages(substream,
						params_buffer_bytes(params));
#endif
			if (ret < 0)
				return ret;
			memif->using_passthrough = 0;
			using_dram = true;
			}
			memif->using_sram = 0;
		}
		goto MEM_ALLOCATE_DONE;
#else //CONFIG_NEBULA_SND_PASSTHROUGH
	/*
	 * hw_params may be called several time,
	 * free sram of this substream first
	 */
	mtk_audio_sram_free(afe->sram, substream);

	substream->runtime->dma_bytes = params_buffer_bytes(params);

	if (memif->use_dram_only == 0 &&
	    mtk_audio_sram_allocate(afe->sram,
				    &substream->runtime->dma_addr,
				    &substream->runtime->dma_area,
				    substream->runtime->dma_bytes,
				    substream,
				    params_format(params), false, false) == 0) {
		memif->using_sram = 1;
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
		if (memif->vow_barge_in_enable) {
			memif->vow_barge_in_using_dram = false;
		}
#endif
	} else
#endif //CONFIG_NEBULA_SND_PASSTHROUGH
#endif //CONFIG_SND_SOC_MTK_SRAM
	{
		memif->using_sram = 0;
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
		if (memif->vow_barge_in_enable) {
			ret = notify_allocate_mem(NOTIFIER_VOW_ALLOCATE_MEM, substream);
			if (ret != NOTIFY_STOP) {
				dev_err(afe->dev,
					"%s(), vow_barge_in allocate mem err: %d\n",
					__func__, ret);
				return -EINVAL;
			}
			memif->vow_barge_in_using_dram = true;
			goto MEM_ALLOCATE_DONE;
		}
#endif

#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
		if (memif->use_adsp_share_mem) {
			ret = mtk_adsp_allocate_mem(substream,
						    params_buffer_bytes(params));
			using_dram = true;
			if (ret < 0) {
				dev_err(afe->dev, "%s(), adsp_share_mem: %d, err: %d\n",
					__func__, memif->use_adsp_share_mem, ret);
				return ret;
			}

			goto MEM_ALLOCATE_DONE;
		}
#endif

#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
		if (memif->use_scp_share_mem) {
			ret = mtk_scp_allocate_mem(substream,
						    params_buffer_bytes(params));
			if (ret < 0) {
				dev_info(afe->dev, "%s(), scp_share_mem: %d, err: %d\n",
					__func__, memif->use_scp_share_mem, ret);
				return ret;
			}

			goto MEM_ALLOCATE_DONE;
		}
#endif

#if IS_ENABLED(CONFIG_MTK_ULTRASND_PROXIMITY)
	if (memif->scp_ultra_enable) {
		ret = notify_allocate_mem(NOTIFIER_ULTRASOUND_ALLOCATE_MEM, substream);
		if (ret != NOTIFY_STOP) {
			dev_err(afe->dev, "%s(), scp_ultra_enable: %d, err: %d\n",
				__func__, memif->scp_ultra_enable, ret);
			return -EINVAL;
		}

		goto MEM_ALLOCATE_DONE;
	}
#endif
		ret = snd_pcm_lib_malloc_pages(substream,
					       params_buffer_bytes(params));
		using_dram = true;
		if (ret < 0) {
			dev_err(afe->dev,
				"%s(), snd_pcm_lib_malloc_pages err: %d\n",
				__func__, ret);
			return ret;
		}
	}
MEM_ALLOCATE_DONE:
	dev_info(afe->dev,
		 "%s(), %s, use_adsp_share_mem %d, using_sram %d, use_dram_only %d, ch %d, rate %d, fmt %d, dma_addr %pad, dma_area %p, dma_bytes 0x%zx\n",
		 __func__, memif->data->name,
		 memif->use_adsp_share_mem,
		 memif->using_sram, memif->use_dram_only,
		 channels, rate, format,
		 &substream->runtime->dma_addr,
		 substream->runtime->dma_area,
		 substream->runtime->dma_bytes);

	memset_io(substream->runtime->dma_area, 0,
		  substream->runtime->dma_bytes);

	if (memif->using_sram == 0 && afe->request_dram_resource)
		afe->request_dram_resource(afe->dev);

	/* set addr */
	ret = mtk_memif_set_addr(afe, id,
				 substream->runtime->dma_area,
				 substream->runtime->dma_addr,
				 substream->runtime->dma_bytes);
	if (ret) {
		dev_err(afe->dev, "%s(), error, id %d, set addr, ret %d\n",
			__func__, id, ret);
		return ret;
	}

	/* set channel */
	ret = mtk_memif_set_channel(afe, id, channels);
	if (ret) {
		dev_err(afe->dev, "%s(), error, id %d, set channel %d, ret %d\n",
			__func__, id, channels, ret);
		return ret;
	}

	/* set rate */
	ret = mtk_memif_set_rate_substream(substream, id, rate);
	if (ret) {
		dev_err(afe->dev, "%s(), error, id %d, set rate %d, ret %d\n",
			__func__, id, rate, ret);
		return ret;
	}

	/* set format */
	ret = mtk_memif_set_format(afe, id, format);
	if (ret) {
		dev_err(afe->dev, "%s(), error, id %d, set format %d, ret %d\n",
			__func__, id, format, ret);
		return ret;
	}
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	afe_pcm_ipi_to_dsp(AUDIO_DSP_TASK_PCM_HWPARAM,
			   substream, params, dai, afe);
#endif
	if (!using_dram) {
		dmab = kzalloc(sizeof(*dmab), GFP_KERNEL);
		if (!dmab)
			return -ENOMEM;
		dmab->area = substream->runtime->dma_area;
		dmab->addr = substream->runtime->dma_addr;
		dmab->bytes = substream->runtime->dma_bytes;
		dmab->dev = substream->dma_buffer.dev;
		snd_pcm_set_runtime_buffer(substream, dmab);
	}

#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
	afe_pcm_ipi_to_scp(AUDIO_DSP_TASK_PCM_HWPARAM,
			   substream, params, dai, afe);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_afe_fe_hw_params);

#if IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
void unreg_dram_passthrough_shm(struct mtk_base_afe_memif *memif)
{
	if (memif->using_passthrough
			&& memif->dram_dma_addr
			&& memif->substream->runtime->dma_addr == memif->dram_dma_addr) {
		memif->substream->runtime->dma_area = NULL;
		memif->substream->runtime->dma_addr = 0;
		memif->substream->runtime->dma_bytes = 0;
		memif->using_passthrough = 0;
	}

	memif->dram_dma_area = NULL;
	memif->dram_dma_addr = 0;
	memif->dram_dma_bytes = 0;
}
EXPORT_SYMBOL_GPL(unreg_dram_passthrough_shm);

void unreg_sram_passthrough_shm(struct mtk_base_afe_memif *memif)
{
	if (memif->using_passthrough
			&& memif->sram_dma_addr
			&& memif->substream->runtime->dma_addr == memif->sram_dma_addr) {
		memif->substream->runtime->dma_area = NULL;
		memif->substream->runtime->dma_addr = 0;
		memif->substream->runtime->dma_bytes = 0;
		memif->using_passthrough = 0;
	}

	if (memif->sram_dma_area)
		iounmap(memif->sram_dma_area);

	memif->sram_dma_area = NULL;
	memif->sram_dma_addr = 0;
	memif->sram_dma_bytes = 0;
}
EXPORT_SYMBOL_GPL(unreg_sram_passthrough_shm);
#endif

int mtk_afe_fe_hw_free(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct mtk_base_afe_memif *memif = &afe->memif[cpu_dai->id];
	int pid = current->pid;
	int tid = current->tgid;

	dev_info(afe->dev,
		 "%s(), %s, use_adsp_share_mem %d, using_sram %d, use_dram_only %d, dma_addr %pad, dma_area %p, dma_bytes 0x%zx, vow_barge_in_enable %d, trigger close memif pid %d, tid %d, name %s and hw free pid %d, tid %d, name %s\n",
		 __func__, memif->data->name,
		 memif->use_adsp_share_mem,
		 memif->using_sram, memif->use_dram_only,
		 &substream->runtime->dma_addr,
		 substream->runtime->dma_area,
		 substream->runtime->dma_bytes,
		 memif->vow_barge_in_enable,
		 memif->pid, memif->tid, memif->process_name,
		 pid, tid, current->comm);

	if (tid != memif->tid && memif->pid > 0) {
		dev_info(afe->dev, "%s(), tid is not match\n", __func__);
		dump_stack();
	}

	if (memif->err_close_order)
		dump_stack();

#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	afe_pcm_ipi_to_dsp(AUDIO_DSP_TASK_PCM_HWFREE,
			   substream, NULL, dai, afe);
#endif

#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
	afe_pcm_ipi_to_scp(AUDIO_DSP_TASK_PCM_HWFREE,
			   substream, NULL, dai, afe);
#endif

#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
	if (memif->vow_barge_in_enable) {
		// audio irq stop
		vow_barge_in_disable_memif(afe, memif);
		// send ipi to SCP
		vow_barge_in_hw_free(afe, cpu_dai->id);
	}
#endif

#if IS_ENABLED(CONFIG_MTK_ULTRASND_PROXIMITY)
	if (memif->scp_ultra_enable)
		ultra_stop_memif(afe);
#endif

	if (memif->using_sram == 0 && afe->release_dram_resource)
		afe->release_dram_resource(afe->dev);

#if !IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
	// mmap do not free buffer
	if (memif->use_mmap_share_mem) {
		kfree(substream->runtime->dma_buffer_p);
		snd_pcm_set_runtime_buffer(substream, NULL);
		mtk_clean_mmap_dl_buffer(afe->dev);
		return 0;
	}
#endif

#if IS_ENABLED(CONFIG_SND_SOC_MTK_SRAM)
	if (memif->using_sram) {
		memif->using_sram = 0;
		kfree(substream->runtime->dma_buffer_p);
		snd_pcm_set_runtime_buffer(substream, NULL);
#if IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
		return 0;
#else
		return mtk_audio_sram_free(afe->sram, substream);
#endif
	}
	else
#endif
	{
#if IS_ENABLED(CONFIG_NEBULA_SND_PASSTHROUGH)
		if (memif->using_passthrough)
			return 0;
#endif

#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
		if (is_adsp_genpool_addr_valid(substream))
			return mtk_adsp_free_mem(substream);
#endif
#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
		if (is_scp_genpool_addr_valid(substream))
			return mtk_scp_free_mem(substream);
#endif
#if IS_ENABLED(CONFIG_MTK_ULTRASND_PROXIMITY)
		// ultrasound uses reserve dram, ignore free
		if (is_ultra_reserved_dram_addr_valid(substream)) {
			kfree(substream->runtime->dma_buffer_p);
			snd_pcm_set_runtime_buffer(substream, NULL);
			return 0;
		}
#endif
#if IS_ENABLED(CONFIG_MTK_VOW_SUPPORT)
		// vow uses reserve dram, ignore free
		if (memif->vow_barge_in_using_dram) {
			memif->vow_barge_in_using_dram = false;
			return 0;
		}
#endif

		return snd_pcm_lib_free_pages(substream);
	}
}
EXPORT_SYMBOL_GPL(mtk_afe_fe_hw_free);

int mtk_afe_fe_trigger(struct snd_pcm_substream *substream, int cmd,
		       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int id = asoc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	struct mtk_base_afe_irq *irqs = &afe->irqs[memif->irq_usage];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	unsigned int counter = runtime->period_size;
	int fs;
	int ret;

	dev_dbg(afe->dev, "%s %s cmd=%d\n", __func__, memif->data->name, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		ret = mtk_memif_set_enable(afe, id);
		if (ret) {
			dev_err(afe->dev, "%s(), error, id %d, memif enable, ret %d\n",
				__func__, id, ret);
			return ret;
		}

		/* set irq counter */
		mtk_regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				       irq_data->irq_cnt_maskbit, counter,
				       irq_data->irq_cnt_shift);

		/* set irq fs */
		fs = afe->irq_fs(substream, runtime->rate);

		if (fs < 0)
			return -EINVAL;

		mtk_regmap_update_bits(afe->regmap, irq_data->irq_fs_reg,
				       irq_data->irq_fs_maskbit, fs,
				       irq_data->irq_fs_shift);

		/* enable interrupt */
		mtk_regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
				       1, 1, irq_data->irq_en_shift);

		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		ret = mtk_memif_set_disable(afe, id);
		if (ret) {
			dev_err(afe->dev, "%s(), error, id %d, memif enable, ret %d\n",
				__func__, id, ret);
		}

		/* disable interrupt */
		mtk_regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
				       1, 0, irq_data->irq_en_shift);
		/* and clear pending IRQ */
		mtk_regmap_write(afe->regmap, irq_data->irq_clr_reg,
				 1 << irq_data->irq_clr_shift);
		return ret;
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL_GPL(mtk_afe_fe_trigger);

int mtk_afe_fe_prepare(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd  = asoc_substream_to_rtd(substream);
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int id = asoc_rtd_to_cpu(rtd, 0)->id;
	int pbuf_size;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (afe->get_memif_pbuf_size) {
			pbuf_size = afe->get_memif_pbuf_size(substream);
			mtk_memif_set_pbuf_size(afe, id, pbuf_size);
		}
	}

	/* The data type of stop_threshold in userspace is unsigned int.
	 * However its data type in kernel space is unsigned long.
	 * It needs to convert to ULONG_MAX in kernel space
	 */
	if (substream->runtime->stop_threshold == ~(0U))
		substream->runtime->stop_threshold = ULONG_MAX;
	if (substream->runtime->stop_threshold == S32_MAX)
		substream->runtime->stop_threshold = LONG_MAX;

#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUDIO_DSP)
	afe_pcm_ipi_to_dsp(AUDIO_DSP_TASK_PCM_PREPARE,
			   substream, NULL, dai, afe);
#endif

#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
	afe_pcm_ipi_to_scp(AUDIO_DSP_TASK_PCM_PREPARE,
			   substream, NULL, dai, afe);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_afe_fe_prepare);

const struct snd_soc_dai_ops mtk_afe_fe_ops = {
	.startup	= mtk_afe_fe_startup,
	.shutdown	= mtk_afe_fe_shutdown,
	.hw_params	= mtk_afe_fe_hw_params,
	.hw_free	= mtk_afe_fe_hw_free,
	.prepare	= mtk_afe_fe_prepare,
	.trigger	= mtk_afe_fe_trigger,
};
EXPORT_SYMBOL_GPL(mtk_afe_fe_ops);

static DEFINE_MUTEX(irqs_lock);
int mtk_dynamic_irq_acquire(struct mtk_base_afe *afe)
{
	int i;

	mutex_lock(&afe->irq_alloc_lock);
	for (i = 0; i < afe->irqs_size; ++i) {
		if (afe->irqs[i].irq_occupyed == 0) {
			afe->irqs[i].irq_occupyed = 1;
			mutex_unlock(&afe->irq_alloc_lock);
			return i;
		}
	}
	mutex_unlock(&afe->irq_alloc_lock);
	return afe->irqs_size;
}
EXPORT_SYMBOL_GPL(mtk_dynamic_irq_acquire);

int mtk_dynamic_irq_release(struct mtk_base_afe *afe, int irq_id)
{
	mutex_lock(&afe->irq_alloc_lock);
	if (irq_id >= 0 && irq_id < afe->irqs_size) {
		afe->irqs[irq_id].irq_occupyed = 0;
		mutex_unlock(&afe->irq_alloc_lock);
		return 0;
	}
	mutex_unlock(&afe->irq_alloc_lock);
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(mtk_dynamic_irq_release);

int mtk_afe_suspend(struct snd_soc_component *component)
{
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct device *dev = afe->dev;
	struct regmap *regmap = afe->regmap;
	int i;

	if (pm_runtime_status_suspended(dev) || afe->suspended)
		return 0;

	if (!afe->reg_back_up)
		afe->reg_back_up =
			devm_kcalloc(dev, afe->reg_back_up_list_num,
				     sizeof(unsigned int), GFP_KERNEL);

	for (i = 0; i < afe->reg_back_up_list_num; i++)
		regmap_read(regmap, afe->reg_back_up_list[i],
			    &afe->reg_back_up[i]);

	afe->suspended = true;
	afe->runtime_suspend(dev);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_afe_suspend);

int mtk_afe_resume(struct snd_soc_component *component)
{
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct device *dev = afe->dev;
	struct regmap *regmap = afe->regmap;
	int i = 0;

	if (pm_runtime_status_suspended(dev) || !afe->suspended)
		return 0;

	afe->runtime_resume(dev);

	if (!afe->reg_back_up)
		dev_dbg(dev, "%s no reg_backup\n", __func__);

	for (i = 0; i < afe->reg_back_up_list_num; i++)
		mtk_regmap_write(regmap, afe->reg_back_up_list[i],
				 afe->reg_back_up[i]);

	afe->suspended = false;
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_afe_resume);

unsigned int is_afe_need_triggered(struct mtk_base_afe_memif *memif)
{
	/* memif and irq enable control of SCP and ADSP
	 * features will be set in ADSP and SCP side.
	 */

	if (memif->use_adsp_share_mem ||
	    memif->vow_barge_in_enable ||
#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
	    memif->use_scp_share_mem ||
#endif
	    memif->scp_ultra_enable)
		return false;

	return true;
}
EXPORT_SYMBOL_GPL(is_afe_need_triggered);

int raw_mtk_memif_set_enable(struct mtk_base_afe *afe, int id)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int reg = 0;

	if (memif->data->enable_shift < 0) {
		dev_warn(afe->dev, "%s(), error, id %d, enable_shift < 0\n",
			 __func__, id);
		return 0;
	}

	if (afe->is_memif_bit_banding)
		reg = memif->data->enable_reg + AFE_AGENT_SET_OFFSET;
	else
		reg = memif->data->enable_reg;

	return mtk_regmap_update_bits(afe->regmap, reg,
				      1, 1, memif->data->enable_shift);
}

int raw_mtk_memif_set_disable(struct mtk_base_afe *afe, int id)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int reg = 0;

	if (memif->data->enable_shift < 0) {
		dev_warn(afe->dev, "%s(), error, id %d, enable_shift < 0\n",
			 __func__, id);
		return 0;
	}

	if (afe->is_memif_bit_banding)
		reg = memif->data->enable_reg + AFE_AGENT_CLR_OFFSET;
	else
		reg = memif->data->enable_reg;

	/* In bit banding mechanism,
	 * it sets 1 to corresponding bit for disabling memif.
	 */
	return mtk_regmap_update_bits(afe->regmap, reg,
				      1, afe->is_memif_bit_banding,
				      memif->data->enable_shift);
}

int mtk_memif_set_enable(struct mtk_base_afe *afe, int afe_id)
{
	int ret = 0;
	int adsp_sem_ret = NOTIFY_STOP;
	int get_sema_type;
	int release_sema_type;

	if (!afe)
		return -ENODEV;

	get_sema_type = afe->is_scp_sema_support ?
			    NOTIFIER_SCP_3WAY_SEMAPHORE_GET :
			    NOTIFIER_ADSP_3WAY_SEMAPHORE_GET;
	release_sema_type = afe->is_scp_sema_support ?
				NOTIFIER_SCP_3WAY_SEMAPHORE_RELEASE :
				NOTIFIER_ADSP_3WAY_SEMAPHORE_RELEASE;

	if (!is_semaphore_control_need(afe->is_scp_sema_support))
		return raw_mtk_memif_set_enable(afe, afe_id);

	if (afe->is_memif_bit_banding == 0) {
		adsp_sem_ret =
			notify_3way_semaphore_control(get_sema_type, NULL);
		if (adsp_sem_ret != NOTIFY_STOP) {
			pr_info("%s error, adsp_sem_ret[%d]\n", __func__, ret);
			return -EBUSY;
		}
	}

	ret = raw_mtk_memif_set_enable(afe, afe_id);

	if (afe->is_memif_bit_banding == 0)
		notify_3way_semaphore_control(release_sema_type, NULL);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_memif_set_enable);

int mtk_memif_set_disable(struct mtk_base_afe *afe, int afe_id)
{
	int ret = 0;
	int adsp_sem_ret = NOTIFY_STOP;
	int get_sema_type;
	int release_sema_type;

	if (!afe)
		return -ENODEV;

	get_sema_type = afe->is_scp_sema_support ?
			    NOTIFIER_SCP_3WAY_SEMAPHORE_GET :
			    NOTIFIER_ADSP_3WAY_SEMAPHORE_GET;
	release_sema_type = afe->is_scp_sema_support ?
				NOTIFIER_SCP_3WAY_SEMAPHORE_RELEASE :
				NOTIFIER_ADSP_3WAY_SEMAPHORE_RELEASE;

	if (!is_semaphore_control_need(afe->is_scp_sema_support))
		return raw_mtk_memif_set_disable(afe, afe_id);

	if (afe->is_memif_bit_banding == 0) {
		adsp_sem_ret =
			notify_3way_semaphore_control(get_sema_type, NULL);
		if (adsp_sem_ret != NOTIFY_STOP) {
			pr_info("%s error, adsp_sem_ret[%d]\n", __func__, ret);
			return -EBUSY;
		}
	}

	ret = raw_mtk_memif_set_disable(afe, afe_id);

	if (afe->is_memif_bit_banding == 0)
		notify_3way_semaphore_control(release_sema_type, NULL);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_memif_set_disable);

int mtk_irq_set_enable(struct mtk_base_afe *afe,
		       const struct mtk_base_irq_data *irq_data,
		       int afe_id)
{
	int ret = 0;
	int adsp_sem_ret = NOTIFY_STOP;
	int get_sema_type;
	int release_sema_type;

	if (!afe)
		return -ENODEV;
	if (!irq_data)
		return -ENODEV;

	get_sema_type = afe->is_scp_sema_support ?
			    NOTIFIER_SCP_3WAY_SEMAPHORE_GET :
			    NOTIFIER_ADSP_3WAY_SEMAPHORE_GET;
	release_sema_type = afe->is_scp_sema_support ?
				NOTIFIER_SCP_3WAY_SEMAPHORE_RELEASE :
				NOTIFIER_ADSP_3WAY_SEMAPHORE_RELEASE;

	if (!is_semaphore_control_need(afe->is_scp_sema_support))
		return regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
					  1 << irq_data->irq_en_shift,
					  1 << irq_data->irq_en_shift);

	adsp_sem_ret =
		notify_3way_semaphore_control(get_sema_type, NULL);
	if (adsp_sem_ret != NOTIFY_STOP) {
		pr_info("%s error, adsp_sem_ret[%d]\n", __func__, ret);
		return -EBUSY;
	}

	regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
			   1 << irq_data->irq_en_shift,
			   1 << irq_data->irq_en_shift);
	notify_3way_semaphore_control(release_sema_type, NULL);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_irq_set_enable);

int mtk_irq_set_disable(struct mtk_base_afe *afe,
			const struct mtk_base_irq_data *irq_data,
			int afe_id)
{
	int ret = 0;
	int adsp_sem_ret = NOTIFY_STOP;
	int get_sema_type;
	int release_sema_type;

	if (!afe)
		return -EPERM;
	if (!irq_data)
		return -EPERM;

	get_sema_type = afe->is_scp_sema_support ?
			    NOTIFIER_SCP_3WAY_SEMAPHORE_GET :
			    NOTIFIER_ADSP_3WAY_SEMAPHORE_GET;
	release_sema_type = afe->is_scp_sema_support ?
				NOTIFIER_SCP_3WAY_SEMAPHORE_RELEASE :
				NOTIFIER_ADSP_3WAY_SEMAPHORE_RELEASE;

	if (!is_semaphore_control_need(afe->is_scp_sema_support))
		return regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
					  1 << irq_data->irq_en_shift,
					  0 << irq_data->irq_en_shift);

	adsp_sem_ret =
		notify_3way_semaphore_control(get_sema_type, NULL);
	if (adsp_sem_ret != NOTIFY_STOP) {
		pr_info("%s error, adsp_sem_ret[%d]\n", __func__, ret);
		return -EBUSY;
	}

	regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
			   1 << irq_data->irq_en_shift,
			   0 << irq_data->irq_en_shift);
	notify_3way_semaphore_control(release_sema_type, NULL);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_irq_set_disable);

int mtk_memif_set_addr(struct mtk_base_afe *afe, int id,
		       unsigned char *dma_area,
		       dma_addr_t dma_addr,
		       size_t dma_bytes)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int msb_at_bit33 = upper_32_bits(dma_addr) ? 1 : 0;
	unsigned int phys_buf_addr = lower_32_bits(dma_addr);
	unsigned int phys_buf_addr_upper_32 = upper_32_bits(dma_addr);
	unsigned int value = 0;

	/* check the memif already disable */
	regmap_read(afe->regmap, memif->data->enable_reg, &value);
	if (value & 0x1 << memif->data->enable_shift) {
		mtk_memif_set_disable(afe, id);
		pr_info("%s memif[%d] is enabled before set_addr, en:0x%x",
			__func__, id, value);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		// For Liber hardcode, ignore VOW CM0 aee
		if ((void *)afe_is_vow_memif == NULL || afe_is_vow_memif(id) == false)
			aee_kernel_exception("[Audio]", "Error: AFE memif enable before set_addr");
#endif
	}

	memif->dma_area = dma_area;
	memif->dma_addr = dma_addr;
	memif->dma_bytes = dma_bytes;

	/* start */
	mtk_regmap_write(afe->regmap, memif->data->reg_ofs_base,
			 phys_buf_addr);
	/* end */
	if (memif->data->reg_ofs_end)
		mtk_regmap_write(afe->regmap,
				 memif->data->reg_ofs_end,
				 phys_buf_addr + dma_bytes - 1);
	else
		mtk_regmap_write(afe->regmap,
				 memif->data->reg_ofs_base +
				 AFE_BASE_END_OFFSET,
				 phys_buf_addr + dma_bytes - 1);

	/* set start, end, upper 32 bits */
	if (memif->data->reg_ofs_base_msb) {
		mtk_regmap_write(afe->regmap, memif->data->reg_ofs_base_msb,
				 phys_buf_addr_upper_32);
		mtk_regmap_write(afe->regmap,
				 memif->data->reg_ofs_end_msb,
				 phys_buf_addr_upper_32);
	}

	/*
	 * set MSB to 33-bit, for memif address
	 * only for memif base address, if msb_end_reg exists
	 */
	if (memif->data->msb_reg)
		mtk_regmap_update_bits(afe->regmap, memif->data->msb_reg,
				       1, msb_at_bit33, memif->data->msb_shift);

	/* set MSB to 33-bit, for memif end address */
	if (memif->data->msb_end_reg)
		mtk_regmap_update_bits(afe->regmap, memif->data->msb_end_reg,
				       1, msb_at_bit33,
				       memif->data->msb_end_shift);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_memif_set_addr);

int mtk_memif_set_channel(struct mtk_base_afe *afe,
			  int id, unsigned int channel)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	unsigned int mono;


	if (memif->data->mono_invert)
		mono = (channel == 1) ? 0 : 1;
	else
		mono = (channel == 1) ? 1 : 0;

	if (memif->data->mono_shift > 0)
		mtk_regmap_update_bits(afe->regmap, memif->data->mono_reg,
				       0x1, mono, memif->data->mono_shift);

	if (memif->data->quad_ch_mask) {
		unsigned int quad_ch = (channel == 4) ? 1 : 0;

		mtk_regmap_update_bits(afe->regmap, memif->data->quad_ch_reg,
				       memif->data->quad_ch_mask,
				       quad_ch, memif->data->quad_ch_shift);
	}

	/* for specific configuration of memif mono mode */
	if (memif->data->int_odd_flag_reg)
		mtk_regmap_update_bits(afe->regmap,
				       memif->data->int_odd_flag_reg,
				       1, mono,
				       memif->data->int_odd_flag_shift);

	if (memif->data->ch_num_maskbit) {
		mtk_regmap_update_bits(afe->regmap, memif->data->ch_num_reg,
				       memif->data->ch_num_maskbit,
				       channel, memif->data->ch_num_shift);
	}

	/* save channel value for cm get*/
	g_channel = channel;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_memif_set_channel);

static int mtk_memif_set_rate_fs(struct mtk_base_afe *afe,
				 int id, int fs)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];

	if (memif->data->fs_shift >= 0)
		mtk_regmap_update_bits(afe->regmap, memif->data->fs_reg,
				       memif->data->fs_maskbit,
				       fs, memif->data->fs_shift);

	return 0;
}

int mtk_memif_set_rate(struct mtk_base_afe *afe,
		       int id, unsigned int rate)
{
	int fs = 0;

	if (!afe->get_dai_fs) {
		dev_err(afe->dev, "%s(), error, afe->get_dai_fs == NULL\n",
			__func__);
		return -EINVAL;
	}

	fs = afe->get_dai_fs(afe, id, rate);

	if (fs < 0)
		return -EINVAL;

	return mtk_memif_set_rate_fs(afe, id, fs);

}
EXPORT_SYMBOL_GPL(mtk_memif_set_rate);

int mtk_memif_set_rate_substream(struct snd_pcm_substream *substream,
				 int id, unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	int fs = 0;

	if (!afe->memif_fs) {
		dev_err(afe->dev, "%s(), error, afe->memif_fs == NULL\n",
			__func__);
		return -EINVAL;
	}

	fs = afe->memif_fs(substream, rate);

	if (fs < 0)
		return -EINVAL;

	return mtk_memif_set_rate_fs(afe, id, fs);
}
EXPORT_SYMBOL_GPL(mtk_memif_set_rate_substream);

int mtk_memif_set_format(struct mtk_base_afe *afe,
			 int id, snd_pcm_format_t format)
{
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	int hd_audio = 0;
	int hd_align = 0;
	int ret = 0;

	/* set hd mode */
	switch (format) {
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_U16_LE:
		hd_audio = 0;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_U32_LE:
		if (afe->memif_32bit_supported) {
			if (memif->data->hd_msb_shift > 0) {
				hd_audio = 1;
				hd_align = 0;
			} else {
				hd_audio = 2;
				hd_align = 0;
			}
		} else {
			hd_audio = 1;
			hd_align = 1;
		}
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_U24_LE:
		if (memif->data->hd_msb_shift > 0) {
			hd_audio = 1;
			hd_align = 1;
		} else {
			hd_audio = 1;
		}
		break;
	default:
		dev_err(afe->dev, "%s() error: unsupported format %d\n",
			__func__, format);
		break;
	}
	if (memif->data->hd_mask <= 0)
		ret = mtk_regmap_update_bits(afe->regmap, memif->data->hd_reg,
					     0x3, hd_audio,
					     memif->data->hd_shift);
	else
		ret = mtk_regmap_update_bits(afe->regmap, memif->data->hd_reg,
					     memif->data->hd_mask, hd_audio,
					     memif->data->hd_shift);
	if (ret)
		dev_err(afe->dev, "%s() error set memif->data->hd_reg %d\n",
			__func__, ret);

	ret = mtk_regmap_update_bits(afe->regmap, memif->data->hd_align_reg,
				     0x1, hd_align,
				     memif->data->hd_align_mshift);
	if (ret)
		dev_err(afe->dev, "%s() error set memif->data->hd_align_mshift %d\n",
			__func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_memif_set_format);

int mtk_memif_set_pbuf_size(struct mtk_base_afe *afe,
			    int id, int pbuf_size)
{
	const struct mtk_base_memif_data *memif_data = afe->memif[id].data;

	if (memif_data->pbuf_mask == 0 || memif_data->minlen_mask == 0)
		return 0;

	mtk_regmap_update_bits(afe->regmap, memif_data->pbuf_reg,
			       memif_data->pbuf_mask,
			       pbuf_size, memif_data->pbuf_shift);

	mtk_regmap_update_bits(afe->regmap, memif_data->minlen_reg,
			       memif_data->minlen_mask,
			       pbuf_size, memif_data->minlen_shift);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_memif_set_pbuf_size);

int mtk_memif_set_min_max_len(struct mtk_base_afe *afe, int id, int min_l, int max_l)
{
	const struct mtk_base_memif_data *memif_data = afe->memif[id].data;

	if (memif_data->minlen_mask == 0 || memif_data->maxlen_mask == 0)
		return 0;

	mtk_regmap_update_bits(afe->regmap, memif_data->minlen_reg,
			       memif_data->minlen_mask,
			       min_l, memif_data->minlen_shift);
	mtk_regmap_update_bits(afe->regmap, memif_data->maxlen_reg,
			       memif_data->maxlen_mask,
			       max_l, memif_data->maxlen_shift);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_memif_set_min_max_len);

MODULE_DESCRIPTION("Mediatek simple fe dai operator");
MODULE_AUTHOR("Garlic Tseng <garlic.tseng@mediatek.com>");
MODULE_LICENSE("GPL v2");
