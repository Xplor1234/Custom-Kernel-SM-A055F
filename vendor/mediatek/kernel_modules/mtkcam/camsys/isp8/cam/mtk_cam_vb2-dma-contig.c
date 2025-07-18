// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/refcount.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>

#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-memops.h>

#include "mtk_cam_vb2-dma-contig.h"

#include "mtk_cam.h"
#include "mtk_cam-debug_option.h"
#include "mtk_cam-trace.h"
#include "mtk_cam-video.h"

struct mtk_cam_vb2_buf {
	struct device			*dev;
	void				*vaddr;
	unsigned long			size;
	void				*cookie;
	dma_addr_t			dma_addr;
	unsigned long			attrs;
	enum dma_data_direction		dma_dir;
	struct sg_table			*dma_sgt;
	struct frame_vector		*vec;

	/* MMAP related */
	struct vb2_vmarea_handler	handler;
	refcount_t			refcount;
	struct sg_table			*sgt_base;
	unsigned int                    sync;

	/* DMABUF related */
	struct dma_buf_attachment	*db_attach;
	struct iosys_map map;
	struct vb2_buffer *vb;
};

/*********************************************/
/*        scatterlist table functions        */
/*********************************************/

static unsigned long mtk_cam_vb2_get_contiguous_size(struct sg_table *sgt)
{
	struct scatterlist *s;
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	unsigned int i;
	unsigned long size = 0;

	for_each_sgtable_dma_sg(sgt, s, i) {
		if (sg_dma_address(s) != expected)
			break;
		expected += sg_dma_len(s);
		size += sg_dma_len(s);
	}
	return size;
}

/*********************************************/
/*         callbacks for all buffers         */
/*********************************************/

static void *mtk_cam_vb2_cookie(struct vb2_buffer *vb, void *buf_priv)
{
	struct mtk_cam_vb2_buf *buf = buf_priv;

	return &buf->dma_addr;
}
static void *mtk_cam_vb2_vaddr(struct vb2_buffer *vb, void *buf_priv)
{
	struct mtk_cam_vb2_buf *buf = buf_priv;
	int ret = 0;

	MTK_CAM_TRACE_FUNC_BEGIN(BUFFER);
	if (!buf->vaddr && buf->db_attach) {
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
		ret = dma_buf_vmap_unlocked(buf->db_attach->dmabuf, &buf->map);
#else
		ret = dma_buf_vmap(buf->db_attach->dmabuf, &buf->map);
#endif
	}
	if (buf->sync)
		buf->vaddr = WARN_ON(ret) ? NULL : buf->map.vaddr;
	else
		buf->vaddr = buf->map.vaddr;
	MTK_CAM_TRACE_END(BUFFER);

	if (ret)
		pr_info("%s warning ret=0x%x", __func__, ret);

	return buf->vaddr;
}


static unsigned int mtk_cam_vb2_num_users(void *buf_priv)
{
	struct mtk_cam_vb2_buf *buf = buf_priv;

	return refcount_read(&buf->refcount);
}

static void mtk_cam_vb2_prepare(void *buf_priv)
{
	struct mtk_cam_vb2_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(buf->vb->vb2_queue);

	if (buf->vb->skip_cache_sync_on_prepare)
		return;
	if (!sgt)
		return;
	if (CAM_DEBUG_ENABLED(V4L2))
		pr_info("%s: %s\n", __func__, node->desc.name);
	if (buf->sync)
		dma_sync_sgtable_for_device(buf->dev, sgt, buf->dma_dir);
}

static void mtk_cam_vb2_finish(void *buf_priv)
{
	struct mtk_cam_vb2_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(buf->vb->vb2_queue);

	if (buf->vb->skip_cache_sync_on_finish)
		return;
	if (!sgt)
		return;
	if (CAM_DEBUG_ENABLED(V4L2))
		pr_info("%s: %s\n", __func__, node->desc.name);
	if (buf->sync)
		dma_sync_sgtable_for_cpu(buf->dev, sgt, buf->dma_dir);
}

/*********************************************/
/*       callbacks for DMABUF buffers        */
/*********************************************/

static int mtk_cam_vb2_map_dmabuf(void *mem_priv)
{
	struct mtk_cam_vb2_buf *buf = mem_priv;
	struct sg_table *sgt;
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(buf->vb->vb2_queue);
	unsigned long contig_size;

	if (WARN_ON(!buf->db_attach)) {
		pr_info("trying to pin a non attached buffer\n");
		return -EINVAL;
	}

	if (WARN_ON(buf->dma_sgt)) {
		pr_info("dmabuf buffer is already pinned\n");
		return 0;
	}

	MTK_CAM_TRACE_FUNC_BEGIN(BUFFER);

	/* get the associated scatterlist for this buffer */
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
	sgt = dma_buf_map_attachment_unlocked(buf->db_attach, buf->dma_dir);
#else
	sgt = dma_buf_map_attachment(buf->db_attach, buf->dma_dir);
#endif
	if (IS_ERR(sgt)) {
		pr_info("Error getting dmabuf scatterlist\n");
		return -EINVAL;
	}

	/* checking if dmabuf is big enough to store contiguous chunk */
	contig_size = mtk_cam_vb2_get_contiguous_size(sgt);
	if (contig_size < buf->size) {
		pr_info("contiguous chunk is too small %lu/%lu\n",
		       contig_size, buf->size);
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
				dma_buf_unmap_attachment_unlocked(buf->db_attach, sgt, buf->dma_dir);
#else
				dma_buf_unmap_attachment(buf->db_attach, sgt, buf->dma_dir);
#endif
		return -EFAULT;
	}

	buf->dma_addr = sg_dma_address(sgt->sgl);
	buf->dma_sgt = sgt;
	buf->vaddr = NULL;
	if (node->desc.image)
		node->image_info.remap = true;
	else
		node->meta_info.remap = true;

	if (CAM_DEBUG_ENABLED(V4L2))
		pr_info("%s: %s\n", __func__, node->desc.name);
	MTK_CAM_TRACE_END(BUFFER);
	return 0;
}

static void mtk_cam_vb2_unmap_dmabuf(void *mem_priv)
{
	struct mtk_cam_vb2_buf *buf = mem_priv;
	struct sg_table *sgt = buf->dma_sgt;
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(buf->vb->vb2_queue);

	if (WARN_ON(!buf->db_attach)) {
		pr_info("trying to unpin a not attached buffer\n");
		return;
	}

	if (WARN_ON(!sgt)) {
		pr_info("dmabuf buffer is already unpinned\n");
		return;
	}

	MTK_CAM_TRACE_FUNC_BEGIN(BUFFER);

	if (buf->vaddr) {
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
		dma_buf_vunmap_unlocked(buf->db_attach->dmabuf, &buf->map);
#else
		dma_buf_vunmap(buf->db_attach->dmabuf, &buf->map);
#endif
		buf->vaddr = NULL;
	}
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
		dma_buf_unmap_attachment_unlocked(buf->db_attach, sgt, buf->dma_dir);
#else
		dma_buf_unmap_attachment(buf->db_attach, sgt, buf->dma_dir);
#endif
	if (CAM_DEBUG_ENABLED(V4L2))
		pr_info("%s: %s\n", __func__, node->desc.name);
	buf->dma_addr = 0;
	buf->dma_sgt = NULL;

	MTK_CAM_TRACE_END(BUFFER);
}

static void mtk_cam_vb2_detach_dmabuf(void *mem_priv)
{
	struct mtk_cam_vb2_buf *buf = mem_priv;

	/* if vb2 works correctly you should never detach mapped buffer */
	if (WARN_ON(buf->dma_addr))
		mtk_cam_vb2_unmap_dmabuf(buf);

	/* detach this attachment */
	dma_buf_detach(buf->db_attach->dmabuf, buf->db_attach);
	kfree(buf);
}

static bool region_heap_is_prot(struct dma_buf *dbuf)
{
	if (strstr(dbuf->exp_name, "prot"))
		return true;

	return false;
}

static void *mtk_cam_vb2_attach_dmabuf(
	struct vb2_buffer *vb, struct device *dev, struct dma_buf *dbuf,
	unsigned long size)
{
	struct mtk_cam_vb2_buf *buf;
	struct dma_buf_attachment *dba;
	struct mtk_cam_buffer *mtk_buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(mtk_buf->vbb.vb2_buf.vb2_queue);
	struct mtk_cam_device *cam = vb2_get_drv_priv(mtk_buf->vbb.vb2_buf.vb2_queue);

	if (dbuf->size < size)
		return ERR_PTR(-EFAULT);
	if (WARN_ON(!dev))
		return ERR_PTR(-EINVAL);
	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);
	/* acp - io coherence buffer */
	if (cam->smmu_dev_acp &&
		(mtk_buf->flags & FLAG_NO_CACHE_CLEAN ||
		mtk_buf->flags & FLAG_NO_CACHE_INVALIDATE) &&
		(node->desc.dma_port == MTKCAM_IPI_RAW_META_STATS_CFG ||
		node->desc.dma_port == MTKCAM_IPI_RAW_META_STATS_0 ||
		node->desc.dma_port == MTKCAM_IPI_RAW_META_STATS_1) &&
		(!region_heap_is_prot(dbuf))) {
		buf->dev = cam->smmu_dev_acp;
		dev_info(buf->dev, "%s node:%s flags:0x%x index:%d", __func__,
			node->desc.name, mtk_buf->flags, mtk_buf->v4l2_buffer_idx);
		mtk_buf->is_acp = 1;
	} else {
		buf->dev = dev;
		mtk_buf->is_acp = 0;
	}
	/* create attachment for the dmabuf with the user device */
	dba = dma_buf_attach(dbuf, buf->dev);
	if (IS_ERR(dba)) {
		pr_info("failed to attach dmabuf\n");
		kfree(buf);
		return dba;
	}
	/* always skip cache operations, we handle it manually */
	dba->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
	buf->dma_dir = vb->vb2_queue->dma_dir;
	buf->size = size;
	buf->db_attach = dba;
	buf->vb = vb;

	if (region_heap_is_prot(dbuf))
		buf->sync = 0;
	else
		buf->sync = 1;

	return buf;
}

/*********************************************/
/*       DMA CONTIG exported functions       */
/*********************************************/

const struct vb2_mem_ops mtk_cam_dma_contig_memops = {
	/* .alloc = */
	/* .put = */
	/* .get_dmabuf = */
	.cookie		= mtk_cam_vb2_cookie,
	.vaddr		= mtk_cam_vb2_vaddr,
	/* .mmap = */
	/* .get_userptr = */
	/* .put_userptr	= */
	.prepare	= mtk_cam_vb2_prepare,
	.finish		= mtk_cam_vb2_finish,
	.map_dmabuf	= mtk_cam_vb2_map_dmabuf,
	.unmap_dmabuf	= mtk_cam_vb2_unmap_dmabuf,
	.attach_dmabuf	= mtk_cam_vb2_attach_dmabuf,
	.detach_dmabuf	= mtk_cam_vb2_detach_dmabuf,
	.num_users	= mtk_cam_vb2_num_users,
};

void mtk_cam_vb2_sync_for_device(struct vb2_buffer *vb)
{
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct mtk_cam_buffer *mtk_buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_vb2_buf *buf;
	struct sg_table *sgt;
	unsigned int plane;

	if (CAM_DEBUG_ENABLED(V4L2))
		pr_info("%s: %s\n", __func__, node->desc.name);

	for (plane = 0; plane < vb->num_planes; ++plane) {
		buf = vb->planes[plane].mem_priv;
		sgt = buf->dma_sgt;

		if (!sgt)
			continue;

		if (buf->sync) {
			dma_sync_sgtable_for_device(
				mtk_buf->is_acp ? buf->dev :
				vb->vb2_queue->alloc_devs[plane] ? : vb->vb2_queue->dev,
				sgt, buf->dma_dir);
		}
	}
}

void mtk_cam_vb2_sync_for_cpu(struct vb2_buffer *vb)
{
	struct mtk_cam_video_device *node = mtk_cam_vbq_to_vdev(vb->vb2_queue);
	struct mtk_cam_buffer *mtk_buf = mtk_cam_vb2_buf_to_dev_buf(vb);
	struct mtk_cam_vb2_buf *buf;
	struct sg_table *sgt;
	unsigned int plane;

	if (CAM_DEBUG_ENABLED(V4L2))
		pr_info("%s: %s\n", __func__, node->desc.name);

	for (plane = 0; plane < vb->num_planes; ++plane) {
		buf = vb->planes[plane].mem_priv;
		sgt = buf->dma_sgt;

		if (!sgt)
			continue;

		if (buf->sync) {
			dma_sync_sgtable_for_cpu(
				mtk_buf->is_acp ? buf->dev :
				vb->vb2_queue->alloc_devs[plane] ? : vb->vb2_queue->dev,
				sgt, buf->dma_dir);
		}
	}
}

MODULE_DESCRIPTION("DMA-contig memory handling routines for mtk-cam videobuf2");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL");
