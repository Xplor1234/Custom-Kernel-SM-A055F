// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>

#include "../audio_dsp/mtk-dsp-platform-driver.h"
#include "../audio_dsp/mtk-base-dsp.h"

#include "mtk-dsp-common.h"
#include "mtk-dsp-mem-control.h"

#if IS_ENABLED(CONFIG_MTK_SLBC) && !IS_ENABLED(CONFIG_ADSP_SLB_LEGACY)
#include "slbc_ops.h"
#define SND_DSP_SLC_DTS_SIZE (9)
#endif

#define SND_DSP_DTS_SIZE (4)
#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_pcm_hardware audio_dsp_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = (SNDRV_PCM_FMTBIT_S16_LE |
		    SNDRV_PCM_FMTBIT_S24_LE |
		    SNDRV_PCM_FMTBIT_S32_LE),
	.rates = MTK_PCM_RATES,
	.period_bytes_min = 256,
	.period_bytes_max = 4 * 48 * 1024,
	.periods_min = 2,
	.periods_max = 4096,
	.buffer_bytes_max = 4 * 48 * 1024,
	.fifo_size = 0,
};

static char *dsp_task_dsp_name[AUDIO_TASK_DAI_NUM] = {
	[AUDIO_TASK_VOIP_ID]        = "mtk-dsp-voip",
	[AUDIO_TASK_PRIMARY_ID]     = "mtk-dsp-primary",
	[AUDIO_TASK_OFFLOAD_ID]     = "mtk-dsp-offload",
	[AUDIO_TASK_DEEPBUFFER_ID]  = "mtk-dsp-deep",
	[AUDIO_TASK_DYNAMIC_ID]     = "mtk-dsp-dynamic",
	[AUDIO_TASK_PLAYBACK_ID]    = "mtk-dsp-playback",
	[AUDIO_TASK_MUSIC_ID]       = "mtk-dsp-music",
	[AUDIO_TASK_CAPTURE_UL1_ID] = "mtk-dsp-capture1",
	[AUDIO_TASK_A2DP_ID]        = "mtk-dsp-a2dp",
	[AUDIO_TASK_BLEDL_ID]       = "mtk-dsp-bledl",
	[AUDIO_TASK_BLEUL_ID]       = "mtk-dsp-bleul",
	[AUDIO_TASK_BTDL_ID]        = "mtk-dsp-btdl",
	[AUDIO_TASK_BTUL_ID]        = "mtk-dsp-btul",
	[AUDIO_TASK_DATAPROVIDER_ID] = "mtk-dsp-dataprovider",
	[AUDIO_TASK_CALL_FINAL_ID]  = "mtk-dsp-call-final",
	[AUDIO_TASK_FAST_ID]        = "mtk-dsp-fast",
	[AUDIO_TASK_SPATIALIZER_ID] = "mtk-dsp-spatializer",
	[AUDIO_TASK_KTV_ID]         = "mtk-dsp-ktv",
	[AUDIO_TASK_CAPTURE_RAW_ID] = "mtk-dsp-capture-raw",
	[AUDIO_TASK_FM_ADSP_ID]     = "mtk-dsp-fm",
	[AUDIO_TASK_UL_PROCESS_ID]  = "mtk-dsp-ulproc",
	[AUDIO_TASK_ECHO_REF_ID]    = "mtk-dsp-echoref",
	[AUDIO_TASK_ECHO_REF_DL_ID] = "mtk-dsp-echodl",
	[AUDIO_TASK_USBDL_ID]       = "mtk-dsp-usbdl",
	[AUDIO_TASK_USBUL_ID]       = "mtk-dsp-usbul",
	[AUDIO_TASK_MDDL_ID]        = "mtk-dsp-mddl",
	[AUDIO_TASK_MDUL_ID]        = "mtk-dsp-mdul",
	[AUDIO_TASK_CALLDL_ID]      = "mtk-dsp-calldl",
	[AUDIO_TASK_CALLUL_ID]      = "mtk-dsp-callul",
	[AUDIO_TASK_FAST_MEDIA_ID]  = "mtk-dsp-fast-media",
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_HFP_CLIENT_SUPPORT)
	[AUDIO_TASK_HFP_CLIENT_RX_ADSP_ID]      = "mtk-dsp-hfp-client-rx",
	[AUDIO_TASK_HFP_CLIENT_TX_ADSP_ID]      = "mtk-dsp-hfp-client-tx",
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_ANC_SUPPORT)
	[AUDIO_TASK_ANC_ADSP_ID]    = "mtk-dsp-anc",
#endif
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_EXTSTREAM_SUPPORT)
	[AUDIO_TASK_EXTSTREAM1_ADSP_ID]   = "mtk-dsp-extstream1",
	[AUDIO_TASK_EXTSTREAM2_ADSP_ID]   = "mtk-dsp-extstream2",
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MTK_AUTO_AUDIO_DSP)
#if IS_ENABLED(CONFIG_MTK_ADSP_AUTO_MULTI_PLAYBACK_SUPPORT)
	[AUDIO_TASK_SUB_PLAYBACK_ID]   = "mtk-dsp-sub-playback",
#endif
	[AUDIO_TASK_PLAYBACK0_ID]   = "mtk-dsp-playback0",
	[AUDIO_TASK_PLAYBACK1_ID]   = "mtk-dsp-playback1",
	[AUDIO_TASK_PLAYBACK2_ID]   = "mtk-dsp-playback2",
	[AUDIO_TASK_PLAYBACK3_ID]   = "mtk-dsp-playback3",
	[AUDIO_TASK_PLAYBACK4_ID]   = "mtk-dsp-playback4",
	[AUDIO_TASK_PLAYBACK5_ID]   = "mtk-dsp-playback5",
	[AUDIO_TASK_PLAYBACK6_ID]   = "mtk-dsp-playback6",
	[AUDIO_TASK_PLAYBACK7_ID]   = "mtk-dsp-playback7",
	[AUDIO_TASK_PLAYBACK8_ID]   = "mtk-dsp-playback8",
	[AUDIO_TASK_PLAYBACK9_ID]   = "mtk-dsp-playback9",
	[AUDIO_TASK_PLAYBACK10_ID]   = "mtk-dsp-playback10",
	[AUDIO_TASK_PLAYBACK11_ID]   = "mtk-dsp-playback11",
	[AUDIO_TASK_PLAYBACK12_ID]   = "mtk-dsp-playback12",
	[AUDIO_TASK_PLAYBACK13_ID]   = "mtk-dsp-playback13",
	[AUDIO_TASK_PLAYBACK14_ID]   = "mtk-dsp-playback14",
	[AUDIO_TASK_PLAYBACK15_ID]   = "mtk-dsp-playback15",
	[AUDIO_TASK_CAPTURE_MCH_ID] = "mtk-dsp-capture-mch",
#endif
};

static int dsp_pcm_taskattr_init(struct platform_device *pdev)
{
	struct mtk_adsp_task_attr task_attr;
#if IS_ENABLED(CONFIG_MTK_SLBC) && !IS_ENABLED(CONFIG_ADSP_SLB_LEGACY)
	struct slbc_gid_data slbc_data_tmp = {0};
#endif
	struct mtk_base_dsp *dsp = platform_get_drvdata(pdev);
	int ret = 0;
	int dsp_id = 0;

	pr_info("%s\n", __func__);
	if (!dsp)
		return -1;

	if (pdev->dev.of_node) {
		for (dsp_id = 0; dsp_id < AUDIO_TASK_DAI_NUM; dsp_id++) {
			ret = of_property_read_u32_array(pdev->dev.of_node,
						 dsp_task_dsp_name[dsp_id],
						 (unsigned int *)&task_attr,
						 SND_DSP_DTS_SIZE);
			if (ret != 0) {
				pr_info("%s error dsp_id[%d]\n",
					__func__, dsp_id);
				continue;
			}
			set_task_attr(dsp_id,
				      ADSP_TASK_ATTR_DEFAULT,
				      task_attr.default_enable);
			set_task_attr(dsp_id,
				      ADSP_TASK_ATTR_MEMDL,
				      task_attr.afe_memif_dl);
			set_task_attr(dsp_id,
				      ADSP_TASK_ATTR_MEMUL,
				      task_attr.afe_memif_ul);
			set_task_attr(dsp_id,
				      ADSP_TASK_ATTR_MEMREF,
				      task_attr.afe_memif_ref);
		}
		dump_all_task_attr();

		/* get dsp version */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "mtk-dsp-ver",
					   &dsp->dsp_ver);
		if (ret != 0)
			pr_info("%s mtk-dsp-ver error\n", __func__);

		ret = of_property_read_u32(pdev->dev.of_node,
			"swdsp-smartpa-process-enable",
			&(task_attr.task_property));
		if (ret)
			task_attr.task_property = 0;
		set_task_attr(AUDIO_TASK_PLAYBACK_ID, ADSP_TASK_ATTR_PROPERTY,
			      task_attr.task_property);

		/* a2dp irq clear in adsp */
		ret = of_property_read_u32(pdev->dev.of_node,
			"mtk-dsp-a2dp-irq",
			&(task_attr.task_property));
		if (ret)
			task_attr.task_property = 0;
		set_task_attr(AUDIO_TASK_A2DP_ID, ADSP_TASK_ATTR_PROPERTY,
			      task_attr.task_property);

		ret = of_property_read_u32(pdev->dev.of_node,
			"adsp-dynamic-buffer",
			&(task_attr.kernel_dynamic_config));
		if (ret)
			task_attr.kernel_dynamic_config = 0;
		set_task_attr(AUDIO_TASK_PLAYBACK_ID, ADSP_TASK_ATTR_KERNEL_LATENCY_SUPPORT,
			      task_attr.kernel_dynamic_config);
#if IS_ENABLED(CONFIG_MTK_SLBC) && !IS_ENABLED(CONFIG_ADSP_SLB_LEGACY)
		ret = of_property_read_u32_array(pdev->dev.of_node,
						 "adsp-slc",
						 (unsigned int *)&slbc_data_tmp,
						 SND_DSP_SLC_DTS_SIZE);
		if (ret != 0)
			pr_info("%s adsp-slc read error\n", __func__);
		for (dsp_id = 0; dsp_id < AUDIO_TASK_DAI_NUM; dsp_id++) {
			if (dsp_id == AUDIO_TASK_VOIP_ID || dsp_id == AUDIO_TASK_OFFLOAD_ID ||
			    dsp_id == AUDIO_TASK_DEEPBUFFER_ID) {
				set_task_attr(dsp_id, ADSP_TASK_ATTR_ADSP_SLC_SIGN, slbc_data_tmp.sign);
				set_task_attr(dsp_id, ADSP_TASK_ATTR_ADSP_SLC_DMASIZE, slbc_data_tmp.dma_size);
				set_task_attr(dsp_id, ADSP_TASK_ATTR_ADSP_SLC_BW, slbc_data_tmp.bw);
			}
		}
#endif
	}
	return 0;
}

static int dsp_pcm_dev_probe(struct platform_device *pdev)
{
	struct mtk_base_dsp *dsp;
	int ret = 0;

	dsp = devm_kzalloc(&pdev->dev, sizeof(struct mtk_base_dsp), GFP_KERNEL);
	if (!dsp)
		return -ENOMEM;

	/*  register dsp dai driver*/
	dai_dsp_register(pdev, dsp);
	dsp->mtk_dsp_hardware = &audio_dsp_hardware;

	dsp->dev = &pdev->dev;

	dsp->request_dram_resource = dsp_dram_request;
	dsp->release_dram_resource = dsp_dram_release;

	dsp->dsp_ipi_ops.ipi_message_callback = mtk_dsp_pcm_ipi_recv;
	dsp->dsp_ipi_ops.ipi_handler = mtk_dsp_handler;

	platform_set_drvdata(pdev, dsp);
	pm_runtime_enable(&pdev->dev);

	if (pdev->dev.of_node) {
		pr_info("%s of_node->name:%s fullname:%s\n", __func__,
			pdev->dev.of_node->name, pdev->dev.of_node->full_name);
	}

	ret = snd_soc_register_component(&pdev->dev,
					 &mtk_dsp_pcm_platform,
					 dsp->dai_drivers,
					 dsp->num_dai_drivers);
	if (ret) {
		dev_info(&pdev->dev, "%s() err_platform: %d\n", __func__, ret);
		goto err_platform;
	}

	set_ipi_recv_private((void *)dsp);
	set_dsp_base((void *)dsp);
	dsp_pcm_taskattr_init(pdev);

	ret = init_mtk_adsp_dram_segment();
	if (ret) {
		pr_info("%s(), init_mtk_adsp_dram_segment fail: %d\n",
			__func__, ret);
		goto err_platform;
	}

	ret = mtk_adsp_init_gen_pool(dsp);
	if (ret) {
		pr_info("init_gen_pool fail\n");
		goto err_platform;
	}

	/* set adsp shared memory cacheable by using ADSP MPU */
	if (dsp->dsp_ver)
		ret = set_mtk_adsp_mpu_sharedram(AUDIO_DSP_AFE_SHARE_MEM_ID);
	if (ret)
		pr_info("set_mtk_adsp_mpu_sharedram fail\n");

	ret = mtk_init_adsp_audio_share_mem(dsp);
	if (ret) {
		pr_info("init share mem fail\n");
		goto err_platform;
	}

	mtk_audio_register_notify();

err_platform:
	return 0;
}

static const struct of_device_id dsp_pcm_dt_match[] = {
	{ .compatible = "mediatek,snd-audio-dsp", },
	{},
};
MODULE_DEVICE_TABLE(of, dsp_pcm_dt_match);

static struct platform_driver dsp_pcm_driver = {
	.driver = {
		   .name = "snd-audio-dsp",
		   .owner = THIS_MODULE,
		   .of_match_table = dsp_pcm_dt_match,
	},
	.probe = dsp_pcm_dev_probe,
};

module_platform_driver(dsp_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC dsp platform driver for audio dsp");
MODULE_AUTHOR("Chipeng Chang <Chipeng.Chang@mediatek.com>");
MODULE_LICENSE("GPL v2");
