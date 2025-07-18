// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_dump.h"
#include "mtk_rect.h"
#include "mtk_drm_drv.h"

#define DISP_REG_OVL_DL_IN_RELAY0_SIZE 0x260
#define DISP_REG_OVL_DL_IN_RELAY1_SIZE 0x264

/**
 * struct mtk_disp_dli_async - DISP_RSZ driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_dli_async {
	struct mtk_ddp_comp ddp_comp;
};

static inline struct mtk_disp_dli_async *comp_to_dli_async(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_dli_async, ddp_comp);
}

static void mtk_dli_async_addon_config(struct mtk_ddp_comp *comp,
				 enum mtk_ddp_comp_id prev,
				 enum mtk_ddp_comp_id next,
				 union mtk_addon_config *addon_config,
				 struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv = NULL;
	u32 dli_in_relay_size = 0;

	DDPINFO("%s+\n", __func__);

	if (!addon_config)
		return;

	priv = mtk_crtc->base.dev->dev_private;
	if (priv->data->mmsys_id == MMSYS_MT6991)
		dli_in_relay_size = DISP_REG_OVL_DL_IN_RELAY1_SIZE;
	else
		dli_in_relay_size = DISP_REG_OVL_DL_IN_RELAY0_SIZE;

	if ((addon_config->config_type.module == DISP_MML_IR_PQ_v2) ||
	    (addon_config->config_type.module == DISP_MML_IR_PQ_v2_1) ||
	    (addon_config->config_type.module == DISP_MML_DL) ||
	    (addon_config->config_type.module == DISP_MML_DL_1) ||
	    (addon_config->config_type.module == DISP_MML_DL_EXDMA)) {
		u8 pipe = addon_config->addon_mml_config.pipe;
		u32 width = addon_config->addon_mml_config.mml_dst_roi[pipe].width;
		u32 height = addon_config->addon_mml_config.mml_dst_roi[pipe].height;

		DDPINFO("%s,module:%d,dli:0x%x,p:%d,w:%d,h:%d\n", __func__, addon_config->config_type.module,
			dli_in_relay_size, pipe, width, height);

		cmdq_pkt_write(handle, comp->cmdq_base,
			       comp->regs_pa + dli_in_relay_size,
			       (width | height << 16), ~0);
	}
}

void mtk_dli_async_dump_mt6991(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}
	DDPINFO("%s\n", __func__);
	DDPDUMP("== DISP %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
	DDPDUMP("0x28C: 0x%08x\n", readl(baddr + 0x28C));
	DDPDUMP("0x308: 0x%08x 0x%08x\n", readl(baddr + 0x308),
		readl(baddr + 0x30C));
}

void mtk_dli_async_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv = NULL;

	priv = mtk_crtc->base.dev->dev_private;

	if (!baddr) {
		DDPDUMP("%s, %s is NULL!\n", __func__, mtk_dump_comp_str(comp));
		return;
	}
	if (priv->data->mmsys_id == MMSYS_MT6991)
		mtk_dli_async_dump_mt6991(comp);
	else {
		DDPDUMP("== DISP %s REGS:0x%pa ==\n", mtk_dump_comp_str(comp), &comp->regs_pa);
		DDPDUMP("0x26C: 0x%08x\n", readl(baddr + 0x26C));
		DDPDUMP("0x2C8: 0x%08x 0x%08x\n", readl(baddr + 0x2C8),
			readl(baddr + 0x2CC));
	}
}

int mtk_dli_async_analysis(struct mtk_ddp_comp *comp)
{
	return 0;
}

static void mtk_dli_async_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPDBG("%s+\n", __func__);
	// nothig to do
}

static void mtk_dli_async_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	DDPDBG("%s+\n", __func__);
	// nothig to do
}

static void mtk_dli_async_prepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_clk_prepare(comp);

	writel(0, comp->regs + DISP_REG_OVL_DL_IN_RELAY1_SIZE);
	writel(0, comp->regs + DISP_REG_OVL_DL_IN_RELAY0_SIZE);
}

static void mtk_dli_async_unprepare(struct mtk_ddp_comp *comp)
{
	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_clk_unprepare(comp);
}

static const struct mtk_ddp_comp_funcs mtk_disp_dli_async_funcs = {
	.start = mtk_dli_async_start,
	.stop = mtk_dli_async_stop,
	.addon_config = mtk_dli_async_addon_config,
	.prepare = mtk_dli_async_prepare,
	.unprepare = mtk_dli_async_unprepare,
};

static int mtk_disp_dli_async_bind(struct device *dev, struct device *master,
			     void *data)
{
	struct mtk_disp_dli_async *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	int ret;

	DDPINFO("%s &priv->ddp_comp:0x%lx\n", __func__, (unsigned long)&priv->ddp_comp);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
			dev->of_node->full_name, ret);
		return ret;
	}

	return 0;
}

static void mtk_disp_dli_async_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct mtk_disp_dli_async *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	DDPINFO("%s\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_dli_async_component_ops = {
	.bind = mtk_disp_dli_async_bind, .unbind = mtk_disp_dli_async_unbind,
};

static int mtk_disp_dli_async_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_dli_async *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;

	DDPINFO("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_DLI_ASYNC);
	DDPINFO("comp_id:%d", comp_id);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		return comp_id;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
				&mtk_disp_dli_async_funcs);

	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		return ret;
	}

	//priv->data = of_device_get_match_data(dev);

	platform_set_drvdata(pdev, priv);

	DDPINFO("&priv->ddp_comp:0x%lx", (unsigned long)&priv->ddp_comp);

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_dli_async_component_ops);

	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
	}

	DDPINFO("%s-\n", __func__);

	return ret;
}

static int mtk_disp_dli_async_remove(struct platform_device *pdev)
{
	struct mtk_disp_dli_async *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_dli_async_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}

static const struct of_device_id mtk_disp_dli_async_driver_dt_match[] = {
	{.compatible = "mediatek,mt6983-disp-dli-async3",},
	{.compatible = "mediatek,mt6895-disp-dli-async3",},
	{.compatible = "mediatek,mt6985-disp-dli-async",},
	{.compatible = "mediatek,mt6989-disp-dli-async",},
	{.compatible = "mediatek,mt6899-disp-dli-async",},
	{.compatible = "mediatek,mt6991-disp-dli-async",},
	{.compatible = "mediatek,mt6897-disp-dli-async",},
	{.compatible = "mediatek,mt6886-disp-dli-async3",},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_disp_dli_async_driver_dt_match);

struct platform_driver mtk_disp_dli_async_driver = {
	.probe = mtk_disp_dli_async_probe,
	.remove = mtk_disp_dli_async_remove,
	.driver = {
		.name = "mediatek-disp-dli-async",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_dli_async_driver_dt_match,
	},
};
