// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include <linux/dma-fence.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <soc/mediatek/smi.h>
#include <cmdq-util.h>
#include <linux/sched/clock.h>
#include <linux/delay.h>
#include <linux/ratelimit.h>

#include "mtk-mml-core.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-tile.h"
#include "mtk-mml-pq-core.h"
#include "mtk-mml-mmp.h"
#include "mtk-mml-dpc.h"

#if IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT)
#define MML_TRACE_MSG_LEN 1024
#else
#define MML_TRACE_MSG_LEN 896
#endif

#ifndef MML_FPGA
int mtk_mml_msg;
#else
int mtk_mml_msg = 1;
#endif
EXPORT_SYMBOL(mtk_mml_msg);
module_param(mtk_mml_msg, int, 0644);

/* see mtk-mml-core.c enum mml_hrt_mode for more detail */
int mtk_mml_hrt_mode;
EXPORT_SYMBOL(mtk_mml_hrt_mode);
module_param(mtk_mml_hrt_mode, int, 0644);

int mml_hrt_bound = 2500;
module_param(mml_hrt_bound, int, 0644);

int mml_cmdq_err;
EXPORT_SYMBOL(mml_cmdq_err);
module_param(mml_cmdq_err, int, 0644);

int mml_pkt_dump;
module_param(mml_pkt_dump, int, 0644);

int mml_pkt_dump_p;
module_param(mml_pkt_dump_p, int, 0644);

int mml_pkt_dump_m = 1;
module_param(mml_pkt_dump_m, int, 0644);

int mml_comp_dump;
module_param(mml_comp_dump, int, 0644);

int mml_trace = 1;
module_param(mml_trace, int, 0644);

/* MML DVFS/QoS debug option, set following value by bit:
 * bit[0:0]	0: disable mml dvfs/qos feature
 *		1: default value, enable and calculate by frame data and end time
 * bit[1:1]	0: do not use force throughput
 *		1: use force throughput in bit[15:2]
 * bit[13:2]	force throghput (clock), only work if bit[1]=1
 * bit[16:16]	reserve
 * bit[17:17]	0: do not use force bandwidth
 *		1: use force bandwidth in bit[31:18]
 * bit[29:18]	force bandwidth, only work if bit[17]=1
 */
int mml_qos = 1;
module_param(mml_qos, int, 0644);

int mml_qos_log;
module_param(mml_qos_log, int, 0644);

int mml_stash_bw;
module_param(mml_stash_bw, int, 0644);

int mml_dpc_log;
module_param(mml_dpc_log, int, 0644);

int mml_pq_disable;
module_param(mml_pq_disable, int, 0644);

int mml_slt;
module_param(mml_slt, int, 0644);

int mml_racing_ut;
module_param(mml_racing_ut, int, 0644);

int mml_rdma_urgent;
module_param(mml_rdma_urgent, int, 0644);

/* loop eoc and wdone mmp debug mark
 * 0: disable/per-task
 * 1: per-tile
 * 2: only tile 0 and tile 1
 * 3: per-loop/per-frame eoc
 */
int mml_racing_eoc = 2;
module_param(mml_racing_eoc, int, 0644);

int mml_hw_perf;
module_param(mml_hw_perf, int, 0644);

/* stash function for mml dma hw
 * 0: disable all stash function
 * bit 0: enable DL mode stash
 * bit 1: enable DC mode stash
 */
#if defined(MML_STASH_SUPPORT)
int mml_stash = 0x3;
#else
int mml_stash;
#endif
module_param(mml_stash, int, 0644);
EXPORT_SYMBOL(mml_stash);

int mml_urate = 110;
module_param(mml_urate, int, 0644);

/* dc mode reserve time in us */
int dc_sw_reserve = 300;
module_param(dc_sw_reserve, int, 0644);

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
static bool mml_timeout_dump = true;
static DEFINE_MUTEX(mml_dump_mutex);

static char *mml_inst_dump;
u32 mml_inst_dump_sz;
static void *mml_inst_raw;
u32 mml_inst_raw_sz;
#endif	/* CONFIG_MTK_MML_DEBUG */

struct topology_ip_node {
	struct list_head entry;
	const struct mml_topology_ops *op;
	const char *ip;
};

/* list of topology ip nodes for different platform,
 * which contains operation for specific platform.
 */
static LIST_HEAD(tp_ips);
static DEFINE_MUTEX(tp_mutex);

/* error counter */
int mml_err_cnt;
module_param(mml_err_cnt, int, 0644);

/* control bits see mtk-mml-core.h enum mml_log_buf_setting */
#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
int mml_log_rec = mml_logbuf_msg | mml_logbuf_log | mml_logbuf_krn;
#else
int mml_log_rec = mml_logbuf_krn;
#endif
EXPORT_SYMBOL(mml_log_rec);
module_param(mml_log_rec, int, 0644);

static DEFINE_SPINLOCK(mml_log_lock);
static char mml_log_record[MML_LOG_SIZE];
static u32 mml_log_idx;
static u32 mml_log_end;

static atomic_t mml_task_ref = ATOMIC_INIT(0);
int mml_task_check_cnt = 16;
module_param(mml_task_check_cnt, int, 0644);

void mml_save_log_record(const char *fmt, ...)
{
	va_list args;
	int ret;
	unsigned long flags = 0;

	va_start(args, fmt);

	spin_lock_irqsave(&mml_log_lock, flags);
	ret = vsnprintf(mml_log_record + mml_log_idx, MML_LOG_SIZE - mml_log_idx, fmt, args);
	if (ret >= MML_LOG_SIZE - mml_log_idx) {
		ret = vsnprintf(mml_log_record, sizeof(mml_log_record), fmt, args);
		mml_log_end = mml_log_idx;
		mml_log_idx = ret >= sizeof(mml_log_record) ? 0 : ret;
	} else {
		mml_log_idx += ret;
		if (mml_log_idx >= MML_LOG_SIZE)
			mml_log_idx = 0;
	}
	spin_unlock_irqrestore(&mml_log_lock, flags);

	va_end(args);
}
EXPORT_SYMBOL_GPL(mml_save_log_record);

void mml_print_log_record(struct seq_file *seq)
{
	int ret = 0;
	u32 idx, end, size = seq->size;
	unsigned long flags = 0;

	seq_puts(seq, "\nMML log buffer begin:\n");

	spin_lock_irqsave(&mml_log_lock, flags);
	idx = mml_log_idx + 1;
	end = mml_log_end;

	if (idx > 0 && end > idx) {
		ret = seq_write(seq, mml_log_record + idx,
			min_t(u32, end - idx - 1, seq->size));
		if (!ret)
			seq_puts(seq, "\n");
	}
	if (!ret) {
		ret = seq_write(seq, mml_log_record, min_t(u32, idx - 1, seq->size));
		if (!ret)
			seq_puts(seq, "\n");
	}
	spin_unlock_irqrestore(&mml_log_lock, flags);
	mml_log("%s print log index %u end %u sz %u ret %d", __func__, idx, end, size, ret);
}

int mml_topology_register_ip(const char *ip, const struct mml_topology_ops *op)
{
	struct topology_ip_node *tp_node = NULL;
	if (!ip) {
		mml_err("fail to register ip %s", ip);
		return -ENOMEM;
	}

	tp_node = kzalloc(sizeof(*tp_node), GFP_KERNEL);
	if (unlikely(!tp_node))
		return -ENOMEM;

	mml_log("%s ip %s", __func__, ip);

	INIT_LIST_HEAD(&tp_node->entry);
	tp_node->ip = ip;
	tp_node->op = op;

	mutex_lock(&tp_mutex);
	list_add_tail(&tp_node->entry, &tp_ips);
	mutex_unlock(&tp_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(mml_topology_register_ip);

void mml_topology_unregister_ip(const char *ip)
{
	struct topology_ip_node *tp_node, *tmp;

	mutex_lock(&tp_mutex);
	list_for_each_entry_safe(tp_node, tmp, &tp_ips, entry) {
		if (strcmp(tp_node->ip, ip) == 0) {
			list_del(&tp_node->entry);
			kfree(tp_node);
			break;
		}
	}
	mutex_unlock(&tp_mutex);
}
EXPORT_SYMBOL_GPL(mml_topology_unregister_ip);

u32 mml_topology_get_mode_caps(void)
{
	struct topology_ip_node *tp_node;
	u32 modes = 0;
	const char *ip = "not ready";

	mutex_lock(&tp_mutex);
	tp_node = list_first_entry_or_null(&tp_ips, typeof(*tp_node), entry);
	if (tp_node) {
		if (tp_node->op->query_mode2 && tp_node->op->support_dc2 &&
			tp_node->op->support_dc2())
			modes = BIT(MML_MODE_MML_DECOUPLE) | BIT(MML_MODE_MML_DECOUPLE2);
		else
			/* enable mdp decouple bit if no mml dc2 support */
			modes = BIT(MML_MODE_MML_DECOUPLE) | BIT(MML_MODE_MDP_DECOUPLE);
		if (tp_node->op->support_couple) {
			enum mml_mode m = tp_node->op->support_couple();

			if (m >= 0)
				modes |= BIT(m);
		}

		ip = tp_node->ip;
	}
	mml_log("platform %s modes %#x", ip, modes);
	mutex_unlock(&tp_mutex);

	return modes;
}

u32 mml_topology_get_hw_caps(void)
{
	struct topology_ip_node *tp_node;
	const char *ip = "not ready";
	static u32 caps;

	if (caps)
		goto done;

	mutex_lock(&tp_mutex);
	tp_node = list_first_entry_or_null(&tp_ips, typeof(*tp_node), entry);
	if (tp_node) {
		if (tp_node->op->support_hw_caps)
			caps = tp_node->op->support_hw_caps();
		else
			caps = MML_HW_PQ_HDR | MML_HW_PQ_HDR10 | MML_HW_PQ_HDR10P |
				MML_HW_PQ_HLG | MML_HW_PQ_HDRVIVID;
		ip = tp_node->ip;
	}
	mml_log("platform %s hw_caps %#x", ip, caps);
	mutex_unlock(&tp_mutex);

done:
	return caps;
}

struct mml_topology_cache *mml_topology_create(struct mml_dev *mml,
					       struct platform_device *pdev,
					       struct cmdq_client **clts,
					       u32 clt_cnt)
{
	struct mml_topology_cache *tp;
	struct topology_ip_node *tp_node;
	const char *ip;
	u32 i;
	int err;

	err = of_property_read_string(pdev->dev.of_node, "topology", &ip);
	if (err < 0) {
		mml_err("fail to parse topology from dts");
		ip = "mt6893";
	}

	tp = devm_kzalloc(&pdev->dev, sizeof(*tp), GFP_KERNEL);
	if (!tp)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < ARRAY_SIZE(tp->path_clts); i++)
		INIT_LIST_HEAD(&tp->path_clts[i].tasks);
	mutex_init(&tp->qos_mutex);

	mutex_lock(&tp_mutex);
	list_for_each_entry(tp_node, &tp_ips, entry) {
		if (strcmp(tp_node->ip, ip) == 0) {
			tp->op = tp_node->op;
			break;
		}
	}
	mutex_unlock(&tp_mutex);

	if (tp->op->init_cache) {
		err = tp->op->init_cache(mml, tp, clts, clt_cnt);
		if (err) {
			devm_kfree(&pdev->dev, tp);
			tp = err == -EAGAIN ? NULL : ERR_PTR(err);
		}
	}
	return tp;
}

static s32 topology_select_path(struct mml_frame_config *cfg)
{
	struct mml_topology_cache *tp = mml_topology_get_cache(cfg->mml);
	s32 ret;

	if (unlikely(!tp)) {
		mml_err("%s path not exists", __func__);
		return -ENXIO;
	}

	if (cfg->path[0]) {
		mml_err("%s select path twice", __func__);
		return -EBUSY;
	}

	if (!tp->op->select)
		return -EPIPE;

	ret = tp->op->select(tp, cfg);
	if (ret < 0)
		return ret;

	/* assign mml-frame or mml-tile to this config */
	if (cfg->path[0]->sys_en[mml_sys_frame])
		cfg->sysid = mml_sys_frame;
	else if (cfg->path[0]->sys_en[mml_sys_tile])
		cfg->sysid = mml_sys_tile;
	else
		cfg->sysid = mml_sys_frame;	/* backward compatible */

	return 0;
}

#define has_cfg_op(_comp, op) \
	(_comp->config_ops && _comp->config_ops->op)
#define call_cfg_op(_comp, op, ...) \
	(has_cfg_op(_comp, op) ? \
		_comp->config_ops->op(_comp, ##__VA_ARGS__) : 0)

#define call_hw_op(_comp, op, ...) \
	(_comp->hw_ops->op ? _comp->hw_ops->op(_comp, ##__VA_ARGS__) : 0)

#define call_dbg_op(_comp, op, ...) \
	((_comp->debug_ops && _comp->debug_ops->op) ? \
		_comp->debug_ops->op(_comp, ##__VA_ARGS__) : 0)

static s32 core_prepare(struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	struct mml_comp *comp;
	u32 i;
	int ret = 0;

	mml_msg("%s task %p pipe %u", __func__, task, pipe);
	mml_trace_ex_begin("%s_%u", __func__, pipe);

	for (i = 0; i < path->node_cnt; i++) {
		/* collect infos for later easy use */
		cache->cfg[i].pipe = pipe;
		cache->cfg[i].node = &path->nodes[i];
		cache->cfg[i].tile_eng_idx = path->nodes[i].tile_eng_idx;
	}

	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		mml_mmp(comp_prepare, MMPROFILE_FLAG_PULSE, comp->id, 0);
		call_cfg_op(comp, prepare, task, &cache->cfg[i]);
		ret = call_cfg_op(comp, buf_prepare, task, &cache->cfg[i]);
		if (ret < 0) {
			mml_err("buf_prepare fail comp %u", comp->id);
			break;
		}
	}

	mml_trace_ex_end();

	return ret;
}

static s32 core_reuse(struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	struct mml_comp *comp;
	u32 i;
	int ret;

	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		ret = call_cfg_op(comp, buf_prepare, task, &cache->cfg[i]);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static s32 command_make(struct mml_task *task, u32 pipe)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_topology_path *path = cfg->path[pipe];
	struct cmdq_pkt *pkt = cmdq_pkt_create(path->clt);
	struct mml_task_reuse *reuse = &task->reuse[pipe];

	struct mml_pipe_cache *cache = &cfg->cache[pipe];
	struct mml_comp_config *ccfg = cache->cfg;
	const struct mml_frame_dest *dest = &cfg->info.dest[0];
	u32 tile_cnt;
	bool reverse;

	struct mml_comp *comp;
	u32 i, tile, tile_idx, tile_wait = 0;
	s32 ret;

	if (IS_ERR(pkt)) {
		mml_err("%s fail err %ld", __func__, PTR_ERR(pkt));
		return PTR_ERR(pkt);
	}
	task->pkts[pipe] = pkt;
	task->pkts[pipe]->no_irq = cfg->dpc;
	pkt->user_data = (void *)task;

	if (!cfg->frame_tile[pipe]) {
		mml_err("%s no tile for input pipe %u", __func__, pipe);
		ret = -EINVAL;
		goto err;
	}
	tile_cnt = cfg->frame_tile[pipe]->tile_cnt;

	/* get total label count to create label array */
	cache->label_cnt = 0;
	if (cfg->info.mode != MML_MODE_DDP_ADDON) {
		for (i = 0; i < path->node_cnt; i++) {
			comp = path->nodes[i].comp;
			cache->label_cnt += call_cfg_op(comp, get_label_count, task, &ccfg[i]);
		}
		reuse->labels = vzalloc(cache->label_cnt * sizeof(*reuse->labels));
		if (!reuse->labels) {
			ret = -ENOMEM;
			goto err;
		}

		reuse->label_mods = kcalloc(cache->label_cnt, sizeof(*reuse->label_mods), GFP_KERNEL);
		if (!reuse->label_mods) {
			ret = -ENOMEM;
			goto err;
		}

		reuse->label_check = kcalloc(cache->label_cnt, sizeof(*reuse->label_check), GFP_KERNEL);
		if (!reuse->label_check) {
			ret = -ENOMEM;
			goto err;
		}
	}

	/* call all component init and frame op, include mmlsys and mutex */
	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_cfg_op(comp, init, task, &ccfg[i]);
	}
	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_cfg_op(comp, frame, task, &ccfg[i]);
	}

	reverse = cfg->info.mode == MML_MODE_RACING &&
		(dest->rotate == MML_ROT_180 || dest->rotate == MML_ROT_270);

	for (tile = 0; tile < tile_cnt; tile++) {
		tile_idx = reverse ? tile_cnt - 1 - tile : tile;

		for (i = 0; i < path->node_cnt; i++) {
			comp = path->nodes[i].comp;
			call_cfg_op(comp, tile, task, &ccfg[i], tile_idx);
		}

		if (cfg->shadow) {
			/* skip first tile wait */
			if (tile) {
				for (i = 0; i < path->node_cnt; i++) {
					comp = path->nodes[i].comp;
					call_cfg_op(comp, wait, task, &ccfg[i],
						    tile_wait);
				}
				for (i = 0; i < path->node_cnt; i++) {
					comp = path->nodes[i].comp;
					call_cfg_op(comp, reset, task, &ccfg[i]);
				}
			}
			/* first tile must wait eof before trigger */
			path->mutex->config_ops->mutex(path->mutex, task,
						       &ccfg[path->mutex_idx]);
			if (path->mutex2)
				path->mutex2->config_ops->mutex(path->mutex2, task,
								&ccfg[path->mutex2_idx]);

			tile_wait = tile_idx;

			call_cfg_op(path->mutex, wait_sof, task, &ccfg[i], tile_wait);
			if (path->mutex2)
				call_cfg_op(path->mutex2, wait_sof, task, &ccfg[i], tile_wait);

			/* last tile needs wait again */
			if (tile == tile_cnt - 1) {
				for (i = 0; i < path->node_cnt; i++) {
					comp = path->nodes[i].comp;
					call_cfg_op(comp, wait, task, &ccfg[i],
						    tile_wait);
				}
			}
		} else {
			path->mutex->config_ops->mutex(path->mutex, task,
						       &ccfg[path->mutex_idx]);
			if (path->mutex2)
				path->mutex2->config_ops->mutex(path->mutex2, task,
								&ccfg[path->mutex2_idx]);

			call_cfg_op(path->mutex, wait_sof, task, &ccfg[i], tile_idx);
			if (path->mutex2)
				call_cfg_op(path->mutex2, wait_sof, task, &ccfg[i], tile_idx);

			for (i = 0; i < path->node_cnt; i++) {
				comp = path->nodes[i].comp;
				call_cfg_op(comp, wait, task, &ccfg[i],
					    tile_idx);
			}
		}
	}

	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_cfg_op(comp, post, task, &ccfg[i]);
	}

	/* leave/terminate this task */
	for (i = path->node_cnt; i > 0; i--) {
		comp = path->nodes[i-1].comp;
		call_cfg_op(comp, done, task, &ccfg[i-1]);
	}

	return 0;

err:
	cmdq_pkt_destroy(task->pkts[pipe]);
	task->pkts[pipe] = NULL;
	return ret;
}

static s32 command_reuse(struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	struct mml_comp_config *ccfg = cache->cfg;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_comp *comp;
	u32 label_cnt, i;

	label_cnt = task->reuse[pipe].label_idx;
	if (label_cnt)
		memset(task->reuse[pipe].label_check, 0, sizeof(bool) * label_cnt);

	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_cfg_op(comp, reframe, task, &ccfg[i]);
	}
	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_cfg_op(comp, repost, task, &ccfg[i]);
	}

	mml_msg("%s task %p pipe %u pkt %p label cnt %u/%u",
		__func__, task, pipe, task->pkts[pipe],
		task->reuse[pipe].label_idx, cache->label_cnt);
	cmdq_pkt_reuse_buf_va(task->pkts[pipe], task->reuse[pipe].labels,
		task->reuse[pipe].label_idx);
	if (task->dpc_reuse_sys.jump_to_begin.va)
		cmdq_pkt_reuse_poll(pkt, &task->dpc_reuse_sys);
	if (task->dpc_reuse_mutex.jump_to_begin.va)
		cmdq_pkt_reuse_poll(pkt, &task->dpc_reuse_mutex);
	/* make sure this pkt not jump to others */
	cmdq_pkt_refinalize(task->pkts[pipe]);

	for (i = 0; i < label_cnt; i++) {
		if (!task->reuse[pipe].label_check[i])
			mml_msg("[warn]job %u not update reuse idx %u mod %u offset %#x",
				task->job.jobid, i,
				task->reuse[pipe].label_mods[i],
				task->reuse[pipe].labels[i].offset);
	}

	return 0;
}

static void get_fmt_str(char *fmt, size_t sz, enum mml_color f)
{
	int ret;

	ret = snprintf(fmt, sz, "%u%s%s%s%s%s%s%s%s",
		MML_FMT_HW_FORMAT(f),
		MML_FMT_SWAP(f) ? "s" : "",
		MML_FMT_IS_AYUV(f) ? "y" : "",
		MML_FMT_BLOCK(f) ? "b" : "",
		MML_FMT_INTERLACED(f) ? "i" : "",
		MML_FMT_UFO(f) ? "u" : "",
		MML_FMT_10BIT_TILE(f) ? "t" :
		MML_FMT_10BIT_PACKED(f) ? "p" :
		MML_FMT_10BIT_LOOSE(f) ? "l" : "",
		MML_FMT_10BIT_JUMP(f) ? "j" : "",
		MML_FMT_AFBC(f) ? "c" :
		MML_FMT_HYFBC(f) ? "h" : "");
	if (ret < 0)
		fmt[0] = '\0';
}

static void get_frame_str(char *frame, size_t sz, const struct mml_frame_data *data)
{
	char fmt[24];
	int ret;

	get_fmt_str(fmt, sizeof(fmt), data->format);
	ret = snprintf(frame, sz, "(%u, %u)[%u %u] %#010x C%s P%hu%s",
		data->width, data->height, data->y_stride,
		MML_FMT_AFBC(data->format) ? data->vert_stride : data->uv_stride,
		data->format, fmt,
		data->profile,
		data->secure ? " sec" : "");
	if (ret < 0)
		frame[0] = '\0';
}

static const char *ovlid_str(enum mml_mode mode, enum mml_layer_id ovlsys_id)
{
	if (mode != MML_MODE_DIRECT_LINK)
		return "";

	switch (ovlsys_id) {
	case MML_DLO_OVLSYS0:
		return " OVL0";
	case MML_DLO_OVLSYS1:
		return " OVL1";
	default:
		break;
	}

	return "";
}

static void dump_task(struct mml_task *task)
{
	const struct mml_frame_config *cfg = task->config;
	const struct mml_frame_dest *dest;
	char frame[60];
	u32 i, sz = 0;
	s32 ret;

	mml_mmp(dumpinfo, MMPROFILE_FLAG_START, task->job.jobid, 0);

	get_frame_str(frame, sizeof(frame), &cfg->info.src);
	mml_log("    in:%s plane:%hhu alpha:%s%s%s%s job:%u mode:%hhu %s%s acttime %u",
		frame,
		task->buf.src.cnt,
		cfg->alpharot ? "rot" :
		cfg->alpharsz ? "rsz" :
		cfg->info.alpha ? "ignore" : "false",
		task->buf.src.fence ? " fence" : "",
		task->buf.src.flush ? " flush" : "",
		task->buf.src.invalid ? " invalid" : "",
		task->job.jobid,
		cfg->info.mode,
		cfg->disp_vdo ? "vdo" : "cmd",
		task->adaptor_type == MML_ADAPTOR_M2M ?
			" m2m" : ovlid_str(cfg->info.mode, cfg->info.ovlsys_id),
		cfg->info.act_time);
	if (cfg->info.dest[0].pq_config.en_region_pq) {
		get_frame_str(frame, sizeof(frame), &cfg->info.seg_map);
		mml_log(" pq in:%s plane:%hhu%s%s",
			frame,
			task->buf.seg_map.cnt,
			task->buf.seg_map.fence ? " fence" : "",
			task->buf.seg_map.flush ? " flush" : "");
	}
	for (i = 0; i < cfg->info.dest_cnt; i++) {
		dest = &cfg->info.dest[i];
		get_frame_str(frame, sizeof(frame), &dest->data);
		mml_log(" out %u:%s plane:%hhu r:%hu%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
			i,
			frame,
			task->buf.dest[i].cnt,
			dest->rotate,
			dest->flip ? " flip" : "",
			task->buf.dest[i].fence ? " fence" : "",
			task->buf.dest[i].flush ? " flush" : "",
			task->buf.dest[i].invalid ? " invalid" : "",
			dest->pq_config.en ? " PQ" : "",
			dest->pq_config.en_fg ? " FG" : "",
			dest->pq_config.en_hdr ? " HDR" : "",
			dest->pq_config.en_ccorr ? " CCORR" : "",
			dest->pq_config.en_dre ? " DRE" : "",
			dest->pq_config.en_sharp ? " SHP" : "",
			dest->pq_config.en_ur ? " UR" : "",
			dest->pq_config.en_dc ? " DC" : "",
			dest->pq_config.en_color ? " COLOR" : "",
			dest->pq_config.en_c3d ? " C3D" : "");
		mml_log("crop %u:(%u, %u, %u, %u) compose:(%u, %u, %u, %u)",
			i,
			dest->crop.r.left,
			dest->crop.r.top,
			dest->crop.r.width,
			dest->crop.r.height,
			dest->compose.left,
			dest->compose.top,
			dest->compose.width,
			dest->compose.height);
	}
	for (i = 0; i < ARRAY_SIZE(cfg->dl_out); i++) {
		if (!cfg->dl_out[i].width && !cfg->dl_out[i].height)
			continue;
		ret = snprintf(&frame[sz], sizeof(frame) - sz,
			" %u:(%u, %u, %u, %u)",
			i,
			cfg->dl_out[i].left,
			cfg->dl_out[i].top,
			cfg->dl_out[i].width,
			cfg->dl_out[i].height);
		if (ret > 0) {
			sz += ret;
			if (sz >= sizeof(frame))
				break;
		}
	}
	if (sz)
		mml_log("  dl%s", frame);

	mml_mmp(dumpinfo, MMPROFILE_FLAG_END, task->job.jobid, 0);
}

static void core_comp_dump(struct mml_task *task, u32 pipe, int cnt)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_topology_path *path = cfg->path[pipe];
	struct mml_comp *comp;
	u32 i;

	mml_err("dump %d task %p pipe %u config %p job %u mode %u topology %u%s",
		cnt, task, pipe, cfg, task->job.jobid, cfg->info.mode, path->path_id,
		task->dump_full ? "" : " (reduce dump)");
	/* print info for this task */
	dump_task(task);

	/* dump simple info to reduce kernel log impact */
	if (!task->dump_full)
		return;

	if (cfg->dual) {
		mml_err("dump another pipe thread status for dual:");
		if (pipe == 0)
			cmdq_thread_dump(cfg->path[1]->clt->chan, task->pkts[1], NULL, NULL);
		else
			cmdq_thread_dump(cfg->path[0]->clt->chan, task->pkts[0], NULL, NULL);
	}

	mml_clock_lock(cfg->mml);
	call_hw_op(path->mmlsys, mminfra_pw_enable);
	mml_dpc_exc_keep_task(task, path);
	call_hw_op(path->mmlsys, pw_enable, cfg->info.mode);
	if (path->mmlsys2)
		call_hw_op(path->mmlsys2, pw_enable, cfg->info.mode);
	mml_clock_unlock(cfg->mml);

	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_dbg_op(comp, dump);
	}

	if (cnt >= 0)
		mml_dpc_dump();

	mml_clock_lock(cfg->mml);
	if (path->mmlsys2)
		call_hw_op(path->mmlsys2, pw_disable, cfg->info.mode);
	call_hw_op(path->mmlsys, pw_disable, cfg->info.mode);
	mml_dpc_exc_release_task(task, path);
	call_hw_op(path->mmlsys, mminfra_pw_disable);
	mml_clock_unlock(cfg->mml);
}

static s32 core_enable(struct mml_task *task, u32 pipe)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_topology_path *path = cfg->path[pipe];
	struct mml_comp *comp;
	u32 i;

	mml_msg("%s task %p pipe %u", __func__, task, pipe);

	mml_clock_lock(cfg->mml);

	mml_trace_ex_begin("%s_%s_%u", __func__, "cmdq", pipe);
	cmdq_mbox_enable(((struct cmdq_client *)task->pkts[pipe]->cl)->chan);
	mml_trace_ex_end();
	mml_trace_ex_begin("%s_%s_%u", __func__, "pw", pipe);
	call_hw_op(path->mmlsys, pw_enable, cfg->info.mode);
	if (path->mmlsys2)
		call_hw_op(path->mmlsys2, pw_enable, cfg->info.mode);
	mml_trace_ex_end();

	if (cfg->info.mode == MML_MODE_DIRECT_LINK && cfg->dpc) {
		/* keep and release pw off until next DT */
		mml_msg_dpc("%s dpc auto for DL", __func__);
	} else if (mml_isdc(cfg->info.mode) || !cfg->dpc) {
		mml_msg_dpc("%s dpc exception flow enable for mode %u", __func__, cfg->info.mode);
		mml_dpc_dc_enable(cfg->mml, path->mmlsys->sysid, true);
		if (path->mmlsys2)
			mml_dpc_dc_enable(cfg->mml, path->mmlsys2->sysid, true);
	}

	mml_trace_ex_begin("%s_%s_%u", __func__, "clk", pipe);
	mml_mmp(clk_enable, MMPROFILE_FLAG_PULSE, 0, 1);

	call_hw_op(path->mmlsys, clk_enable);
	call_hw_op(path->mutex, clk_enable);
	if (path->mmlsys2)
		call_hw_op(path->mmlsys2, clk_enable);
	if (path->mutex2)
		call_hw_op(path->mutex2, clk_enable);

	for (i = 0; i < path->node_cnt; i++) {
		if (i == path->mmlsys_idx || i == path->mutex_idx)
			continue;
		if (path->mmlsys2 && i == path->mmlsys2_idx)
			continue;
		if (path->mutex2 && i == path->mutex2_idx)
			continue;
		comp = path->nodes[i].comp;
		call_hw_op(comp, clk_enable);
	}
	mml_trace_ex_end();

#ifndef MML_FPGA
	cmdq_util_prebuilt_init(CMDQ_PREBUILT_MML);
#endif

	/* callback disp for later config ddren in dispsys */
	if (pipe == 0 && cfg->task_ops->dispen)
		cfg->task_ops->dispen(task, true);

	task->pipe[pipe].en.clk = true;
	mml_mmp(clk_enable, MMPROFILE_FLAG_PULSE, 0, 0);
	mml_clock_unlock(cfg->mml);

	return 0;
}

static s32 core_disable(struct mml_task *task, u32 pipe)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_topology_path *path = cfg->path[pipe];
	struct mml_comp *comp;
	int i;

	mml_trace_ex_begin("%s_%s_%u", __func__, "clk", pipe);

	if (mml_comp_dump) {
		task->dump_full = true; /* back to full for manually debug */
		core_comp_dump(task, pipe, -1);
		/* dump once */
		if (mml_comp_dump == 2)
			mml_comp_dump = 0;
	}

	mml_clock_lock(cfg->mml);

	for (i = 0; i < path->node_cnt; i++) {
		if (i == path->mmlsys_idx || i == path->mutex_idx)
			continue;
		if (path->mmlsys2 && i == path->mmlsys2_idx)
			continue;
		if (path->mutex2 && i == path->mutex2_idx)
			continue;
		comp = path->nodes[i].comp;
		call_hw_op(comp, clk_disable, cfg->dpc);
	}

	if (path->mutex2)
		call_hw_op(path->mutex2, clk_disable, cfg->dpc);
	if (path->mmlsys2)
		call_hw_op(path->mmlsys2, clk_disable, cfg->dpc);
	call_hw_op(path->mutex, clk_disable, cfg->dpc);
	call_hw_op(path->mmlsys, clk_disable, cfg->dpc);

	mml_trace_ex_end();

	mml_trace_ex_begin("%s_%s_%u", __func__, "pw", pipe);

	if (cfg->info.mode == MML_MODE_DIRECT_LINK && cfg->dpc) {
		/* no restore operation for dl w/ mml_dpc on */
		mml_msg_dpc("%s dpc flow disable for dl w/ dpc", __func__);
	} else if (mml_isdc(cfg->info.mode) || !cfg->dpc) {
		mml_msg_dpc("%s dpc exception flow disable for mode %u", __func__, cfg->info.mode);
		/* must before pw_disable */
		if (path->mmlsys2)
			mml_dpc_dc_enable(cfg->mml, path->mmlsys2->sysid, false);
		mml_dpc_dc_enable(cfg->mml, path->mmlsys->sysid, false);
	}

	if (path->mmlsys2)
		call_hw_op(path->mmlsys2, pw_disable, cfg->info.mode);
	call_hw_op(path->mmlsys, pw_disable, cfg->info.mode);

	mml_trace_ex_end();

	mml_trace_ex_begin("%s_%s_%u", __func__, "cmdq", pipe);
	cmdq_mbox_disable(((struct cmdq_client *)task->pkts[pipe]->cl)->chan);
	mml_trace_ex_end();

	/* callback disp to turn off dispsys */
	if (pipe == 0 && cfg->task_ops->dispen)
		cfg->task_ops->dispen(task, false);

	/* reset back to disabled */
	task->pipe[pipe].en.clk = false;
	mml_clock_unlock(cfg->mml);

	mml_msg("%s task %p pipe %u", __func__, task, pipe);

	return 0;
}

static void mml_core_qos_reset(struct mml_task *task, u32 pipe)
{
	const struct mml_frame_config *cfg = task->config;
	const struct mml_topology_path *path = cfg->path[pipe];
	struct mml_comp *comp;
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;

		comp->srt_bw = 0;
		comp->hrt_bw = 0;
		comp->stash_srt_bw = 0;
		comp->stash_hrt_bw = 0;
	}
}

static void mml_core_qos_calc(struct mml_task *task, u32 pipe, u32 throughput)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_topology_path *path = cfg->path[pipe];
	struct mml_pipe_cache *cache = &cfg->cache[pipe];
	struct mml_comp *comp;
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		if (comp->hw_ops->qos_set)
			mml_comp_qos_calc(comp, task, &cache->cfg[i], throughput);
	}
}

static void mml_core_qos_set(struct mml_task *task, u32 pipe, u32 throughput, u32 tput_up)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_topology_path *path = cfg->path[pipe];
	struct mml_pipe_cache *cache = &cfg->cache[pipe];
	struct mml_comp *comp;
	u32 i;

	memset(&task->dpc_srt_bw[0], 0, sizeof(task->dpc_srt_bw));
	memset(&task->dpc_hrt_bw[0], 0, sizeof(task->dpc_hrt_bw));
	memset(&task->dpc_srt_write_bw[0], 0, sizeof(task->dpc_srt_write_bw));
	memset(&task->dpc_hrt_write_bw[0], 0, sizeof(task->dpc_hrt_write_bw));

	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_hw_op(comp, qos_set, task, &cache->cfg[i], throughput, tput_up);
	}
}

static void mml_core_qos_clear(struct mml_task *task, u32 pipe)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_topology_path *path = cfg->path[pipe];
	struct mml_comp *comp;
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_hw_op(comp, qos_clear, task, cfg->dpc);
	}
}

static void mml_core_qos_update_dpc(struct mml_frame_config *cfg, bool trigger)
{
	struct mml_topology_cache *tp = mml_topology_get_cache(cfg->mml);
	struct mml_task_pipe *task_pipe;
	struct mml_task *task;
	const struct mml_topology_path *path = cfg->path[0];
	u32 srt_bw[mml_max_sys] = {0}, hrt_bw[mml_max_sys] = {0}, srt_bw_max = 0, hrt_bw_max = 0;
	u32 stash_srt_bw[mml_max_sys] = {0}, stash_hrt_bw[mml_max_sys] = {0};
	u32 dpc_dvfs_lv = 0;
	enum mml_sys_id sysid;
	u32 i;

	if (unlikely(!tp))
		return;

	for (i = 0; i < ARRAY_SIZE(tp->path_clts); i++) {
		u32 task_srt_max[mml_max_sys] = {0}, task_hrt_max[mml_max_sys] = {0};
		u32 task_stash_srt_max[mml_max_sys] = {0}, task_stash_hrt_max[mml_max_sys] = {0};

		/* scan all tasks in this cmdq client and find max srt hrt */
		list_for_each_entry(task_pipe, &tp->path_clts[i].tasks, entry_clt) {
			task = task_pipe->task;

			for (sysid = 0; sysid < mml_max_sys; sysid++) {
				task_srt_max[sysid] =
					max_t(u32, task_srt_max[sysid], task->dpc_srt_bw[sysid]);
				task_hrt_max[sysid] =
					max_t(u32, task_hrt_max[sysid], task->dpc_hrt_bw[sysid]);

				task_stash_srt_max[sysid] = max_t(u32, task_stash_srt_max[sysid],
					task->dpc_srt_write_bw[sysid]);
				task_stash_hrt_max[sysid] = max_t(u32, task_stash_hrt_max[sysid],
					task->dpc_hrt_write_bw[sysid]);
			}
		}

		for (sysid = 0; sysid < mml_max_sys; sysid++) {
			srt_bw[sysid] += task_srt_max[sysid];
			hrt_bw[sysid] += task_hrt_max[sysid];
			stash_srt_bw[sysid] += task_stash_srt_max[sysid];
			stash_hrt_bw[sysid] += task_stash_hrt_max[sysid];
		}
	}

	for (sysid = 0; sysid < mml_max_sys; sysid++) {
		srt_bw_max = max_t(u32, srt_bw_max, srt_bw[sysid]);
		hrt_bw_max = max_t(u32, hrt_bw_max, hrt_bw[sysid]);
		dpc_dvfs_lv = max_t(u32, dpc_dvfs_lv, tp->qos[sysid].current_level);
	}

	mml_msg_dpc("%s dpc dvfs level %u srt %u hrt %u hrt_mode %u trigger %s",
		__func__, dpc_dvfs_lv, srt_bw_max, hrt_bw_max, mtk_mml_hrt_mode,
		trigger ? "true" : "false");
	if (mtk_mml_hrt_mode == MML_HRT_OSTD_ONLY || mtk_mml_hrt_mode == MML_HRT_MMQOS) {
		/* dpc off case set bw to 0 */
		hrt_bw_max = 0;
	} else if (mtk_mml_hrt_mode == MML_HRT_LIMIT) {
		/* no dpc if lower than hrt bound */
		if (hrt_bw_max < mml_hrt_bound)
			hrt_bw_max = 0;
	}

	/* set dpc hrt/srt bw */
	for (sysid = 0; sysid < mml_max_sys; sysid++) {
		mml_dpc_srt_bw_set(sysid, srt_bw[sysid], false);
		mml_dpc_hrt_bw_set(sysid, hrt_bw_max, false);

		mml_mmp(dpc_bw_srt, MMPROFILE_FLAG_PULSE, sysid, srt_bw[sysid]);
		mml_mmp(dpc_bw_hrt, MMPROFILE_FLAG_PULSE, sysid, hrt_bw_max);

		mml_dpc_channel_bw_set_by_idx(sysid, stash_srt_bw[sysid], false);
		mml_dpc_channel_bw_set_by_idx(sysid, stash_hrt_bw[sysid], true);
	}

	/* set dpc dvfs (mminfra, bus) */
	mml_dpc_dvfs_set(dpc_dvfs_lv, true);
	mml_dpc_dvfs_bw_set(path->mmlsys->id, hrt_bw_max);

	if (path->mmlsys2)
		mml_dpc_dvfs_bw_set(path->mmlsys2->id, hrt_bw_max);

	/* and update in dvfs end case */
	if (trigger)
		mml_dpc_dvfs_trigger();

	mml_mmp(dpc_dvfs, MMPROFILE_FLAG_PULSE, trigger, dpc_dvfs_lv);
}

u64 mml_core_time_dur_us(const struct timespec64 *lhs, const struct timespec64 *rhs)
{
	struct timespec64 delta = timespec64_sub(*lhs, *rhs);

	return div_u64((u64)delta.tv_sec * 1000000000 + delta.tv_nsec, 1000);
}

static u32 mml_core_calc_tput_couple(struct mml_task *task, u32 pixel, u32 pipe)
{
	const struct mml_frame_config *cfg = task->config;
	const struct mml_frame_info *info = &cfg->info;
	const struct mml_frame_data *src = &info->src;
	const struct mml_frame_dest *dest = &info->dest[0];
	u32 act_time_us = div_u64(info->act_time, 1000);

	if (!act_time_us)
		act_time_us = 1;
	/* in inline rotate case mml must complete 1 frame in disp
	 * giving act time, thus calc tput by act_time directly.
	 */
	task->pipe[pipe].throughput = div_u64(pixel, act_time_us);
	mml_mmp(throughput, MMPROFILE_FLAG_PULSE, task->pipe[pipe].throughput, act_time_us);

	if (info->mode == MML_MODE_RACING) {
		if (MML_FMT_COMPRESS(src->format) &&
			((dest->crop.r.width & 0x1f) || (dest->crop.r.height & 0xf))) {
			/* for compress format afbc and hyfbc read block overhead */
			task->pipe[pipe].throughput = (task->pipe[pipe].throughput * 38) >> 5;
		} else {
			/* workaround, increase mml throughput to avoid underrun */
			task->pipe[pipe].throughput = task->pipe[pipe].throughput * 11 / 10;
		}
	} else if (info->mode == MML_MODE_DIRECT_LINK) {
		/* workaround, increase mml throughput to avoid underrun */
		if (cfg->panel_w > dest->data.width)
			task->pipe[pipe].throughput = (u32)((u64)task->pipe[pipe].throughput *
				cfg->panel_w / dest->data.width);
		/* always increase urate */
		task->pipe[pipe].throughput = task->pipe[pipe].throughput * mml_urate / 100;
	}

	return act_time_us;
}

static u64 mml_core_calc_tput(struct mml_task *task, u32 pixel, u32 pipe,
	const struct timespec64 *end, const struct timespec64 *start)
{
	u64 duration = mml_core_time_dur_us(end, start);

	if (!duration || duration <= dc_sw_reserve)
		duration = 1;
	else
		duration -= dc_sw_reserve;

	/* truoughput by end time */
	task->pipe[pipe].throughput = (u32)div_u64(pixel, duration);

	mml_mmp(throughput, MMPROFILE_FLAG_PULSE, task->pipe[pipe].throughput, (u32)duration);

	return duration;
}

static struct mml_path_client *core_get_path_clt(struct mml_task *task, u32 pipe)
{
	struct mml_topology_cache *tp = mml_topology_get_cache(task->config->mml);

	if (unlikely(!tp)) {
		mml_err("%s mml_topology_get_cache return null", __func__);
		return NULL;
	}
	return &tp->path_clts[task->config->path[pipe]->clt_id];
}

static u32 mml_qos_max_freq(const struct mml_topology_path *path,
	const struct mml_topology_cache *tp)
{
	return path->sys_en[mml_sys_frame] ?
		tp->qos[mml_sys_frame].freq_max : tp->qos[mml_sys_tile].freq_max;
}

static void mml_core_dvfs_begin(struct mml_task *task, u32 pipe)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_topology_cache *tp = mml_topology_get_cache(cfg->mml);
	struct mml_path_client *path_clt = core_get_path_clt(task, pipe);
	struct mml_task_pipe *task_pipe_tmp;
	struct timespec64 curr_time, dvfs_end_time;
	u32 throughput, tput_up, max_freq;
	u32 max_pixel = cfg->cache[pipe].max_tput_pixel;
	u64 duration = 0;
	u64 boost_time = 0;
	u32 tmp_pipe = pipe;

	if (unlikely(!path_clt)) {
		mml_err("%s core_get_path_clt return null", __func__);
		return;
	}

	if (unlikely(!tp)) {
		mml_err("%s mml_topology_get_cache return null", __func__);
		return;
	}

	mml_trace_ex_begin("%s", __func__);
	mutex_lock(&tp->qos_mutex);

	ktime_get_real_ts64(&curr_time);
	if (cfg->info.mode == MML_MODE_RACING || cfg->info.mode == MML_MODE_DIRECT_LINK) {
		mml_msg_qos(
			"task dvfs begin %p job %u pipe %u cur %2u.%03llu act_time %u clt id %hhu dur %u fps %u",
			task, task->job.jobid, pipe,
			(u32)curr_time.tv_sec, div_u64(curr_time.tv_nsec, 1000000),
			cfg->info.act_time,
			cfg->path[pipe]->clt_id, cfg->duration, cfg->fps);
	} else {
		mml_msg_qos(
			"task dvfs begin %p job %u pipe %u cur %2u.%03llu end %2u.%03llu clt id %hhu dur %u fps %u",
			task, task->job.jobid, pipe,
			(u32)curr_time.tv_sec, div_u64(curr_time.tv_nsec, 1000000),
			(u32)task->end_time.tv_sec, div_u64(task->end_time.tv_nsec, 1000000),
			cfg->path[pipe]->clt_id, cfg->duration, cfg->fps);
	}

	/* do not append to list and no qos/dvfs for this task */
	if (!(mml_qos & MML_QOS_EN_MASK))
		goto done;

	if (timespec64_compare(&curr_time, &task->end_time) > 0)
		task->end_time = curr_time;

	if (!list_empty(&path_clt->tasks))
		task_pipe_tmp = list_last_entry(&path_clt->tasks,
			typeof(*task_pipe_tmp), entry_clt);
	else
		task_pipe_tmp = NULL;

	if (task_pipe_tmp && timespec64_compare(&task_pipe_tmp->task->end_time, &curr_time) >= 0)
		task->submit_time = task_pipe_tmp->task->end_time;
	else
		task->submit_time = curr_time;

	task->pipe[pipe].throughput = 0;
	if (cfg->info.mode == MML_MODE_RACING || cfg->info.mode == MML_MODE_DIRECT_LINK) {
		/* racing mode uses different calculation since start time
		 * consistent with disp
		 */
		duration = mml_core_calc_tput_couple(task, max_pixel, pipe);

	} else if (timespec64_compare(&task->submit_time, &task->end_time) < 0) {
		/* calculate remaining time to complete pixels */
		duration = mml_core_calc_tput(task, max_pixel, pipe,
			&task->end_time, &task->submit_time);
	}

	max_freq = mml_qos_max_freq(cfg->path[pipe], tp);
	task->pipe[pipe].throughput = min_t(u32, task->pipe[pipe].throughput, max_freq);

	if (unlikely(mml_qos & MML_QOS_FORCE_CLOCK_MASK))
		task->pipe[pipe].throughput = mml_qos_force_clk;

	if (task->pipe[pipe].throughput) {
		throughput = task->pipe[pipe].throughput;
		list_for_each_entry(task_pipe_tmp, &path_clt->tasks, entry_clt) {
			/* find the max throughput (frequency) between tasks on same client */
			throughput = max(throughput, task_pipe_tmp->throughput);
		}
	} else {
		/* there is no time for this task, use max throughput */
		task->pipe[pipe].throughput = max_freq;
		/* make sure end time >= submit time to ensure
		 * next task calculate correct duration
		 */
		task->end_time = task->submit_time;
		/* use max as throughput this round */
		throughput = task->pipe[pipe].throughput;
	}

	/* now append at tail, this order should same as cmdq exec order */
	list_add_tail(&task->pipe[pipe].entry_clt, &path_clt->tasks);

	boost_time = div_u64((u64)cfg->dvfs_boost_time.tv_sec * 1000000000 +
		cfg->dvfs_boost_time.tv_nsec, 1000);
	mml_trace_begin("%u_%llu_%llu", throughput, duration, boost_time);
	task->freq_time[pipe] = sched_clock();
	path_clt->throughput = throughput;
	tput_up = mml_qos_update_sys(cfg->mml, cfg->dpc, cfg->path[pipe], true);

	/* note the running task not always current begin task */
	task_pipe_tmp = list_first_entry_or_null(&path_clt->tasks,
		typeof(*task_pipe_tmp), entry_clt);
	while (task_pipe_tmp && task_pipe_tmp->task->done) {
		if (task_pipe_tmp ==
			list_last_entry(&path_clt->tasks, typeof(*task_pipe_tmp), entry_clt)) {
			task_pipe_tmp = NULL;
			break;
		}
		task_pipe_tmp = list_next_entry(task_pipe_tmp, entry_clt);
	}
	if (unlikely(!task_pipe_tmp))
		goto done;
	/* clear so that qos set api report max bw */
	task_pipe_tmp->bandwidth = 0;
	task->bw_time[pipe] = sched_clock();
	/* check running task use sw pipe0 for right sigle pipe */
	if (task_pipe_tmp->task->config->info.dl_pos == MML_DL_POS_RIGHT)
		tmp_pipe = 0;
	mml_core_qos_calc(task, tmp_pipe, throughput);
	mml_core_qos_set(task_pipe_tmp->task, tmp_pipe, throughput, tput_up);
	if (cfg->dpc)
		tp->dpc_qos_ref++;
	mml_core_qos_update_dpc(cfg, false);

	mml_trace_end();

	ktime_get_real_ts64(&dvfs_end_time);
	cfg->dvfs_boost_time =
		timespec64_sub(dvfs_end_time, curr_time);
	boost_time = div_u64((u64)cfg->dvfs_boost_time.tv_sec * 1000000000 +
		cfg->dvfs_boost_time.tv_nsec, 1000);
	if (boost_time > 2000) {
		cfg->dvfs_boost_time.tv_sec = 0;
		cfg->dvfs_boost_time.tv_nsec = 2000000;
	}

	mml_msg_qos("task dvfs begin %p pipe %u throughput %u (%u) bandwidth %u pixel %u",
		task, pipe, throughput, task_pipe_tmp->throughput,
		task_pipe_tmp->bandwidth, max_pixel);
done:
	mutex_unlock(&tp->qos_mutex);
	mml_trace_ex_end();
}

static void mml_core_dvfs_end(struct mml_task *task, u32 pipe)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_topology_cache *tp = mml_topology_get_cache(cfg->mml);
	struct mml_path_client *path_clt = core_get_path_clt(task, pipe);
	struct mml_task_pipe *task_pipe_cur, *task_pipe_tmp;
	struct timespec64 curr_time;
	u32 throughput = 0, tput_up, max_pixel = 0, bandwidth = 0;
	bool racing_mode = true;
	bool overdue = false;
	u32 tmp_pipe = pipe;

	if (unlikely(!path_clt)) {
		mml_err("%s core_get_path_clt return null", __func__);
		return;
	}

	/* this task must done right now, skip all compare */
	if (unlikely(!tp)) {
		mml_err("%s mml_topology_get_cache return null", __func__);
		return;
	}

	mml_trace_ex_begin("%s", __func__);
	mutex_lock(&tp->qos_mutex);

	ktime_get_real_ts64(&curr_time);

	if (mml_isdc(cfg->info.mode) && timespec64_compare(&curr_time, &task->end_time) > 0) {
		overdue = true;
		mml_trace_tag_start(MML_TTAG_OVERDUE);
	}

	mml_msg_qos("task dvfs end %p job %i pipe %u cur %2u.%03llu end %2u.%03llu clt id %hhu%s",
		task, task->job.jobid, pipe,
		(u32)curr_time.tv_sec, div_u64(curr_time.tv_nsec, 1000000),
		(u32)task->end_time.tv_sec, div_u64(task->end_time.tv_nsec, 1000000),
		cfg->path[pipe]->clt_id,
		overdue ? " overdue" : "");

	if (list_empty(&task->pipe[pipe].entry_clt)) {
		/* task may already removed from other config (thread),
		 * so safe to leave directly.
		 */
		goto exit;
	}

	list_for_each_entry(task_pipe_cur, &path_clt->tasks, entry_clt) {
		if (task_pipe_cur->task->config->info.mode == MML_MODE_RACING ||
			task_pipe_cur->task->config->info.mode == MML_MODE_DIRECT_LINK)
			continue;
		racing_mode = false;
		break;
	}

	list_for_each_entry_safe(task_pipe_cur, task_pipe_tmp, &path_clt->tasks,
		entry_clt) {
		/* remove task from list include tasks before current
		 * ending task, since cmdq already finish them, too.
		 */
		list_del_init(&task_pipe_cur->entry_clt);

		if (task == task_pipe_cur->task) {
			/* found ending one, stops delete */
			break;
		}

		if (!racing_mode)
			mml_msg_qos("task dvfs end %p pipe %u clear qos (pre-end)",
				task_pipe_cur->task, pipe);
	}

	task_pipe_cur = list_first_entry_or_null(&path_clt->tasks, typeof(*task_pipe_cur),
		entry_clt);
	/* find current item which still running */
	while (task_pipe_cur && task_pipe_cur->task->done) {
		if (task_pipe_cur ==
			list_last_entry(&path_clt->tasks, typeof(*task_pipe_cur), entry_clt)) {
			task_pipe_cur = NULL;
			break;
		}
		task_pipe_cur = list_next_entry(task_pipe_cur, entry_clt);
	}

	if (task_pipe_cur) {
		/* calculate remaining time to complete pixels */
		max_pixel = task_pipe_cur->task->config->cache[pipe].max_tput_pixel;

		/* for racing mode, use throughput from act time directly */
		if (racing_mode) {
			throughput = 0;
			list_for_each_entry(task_pipe_tmp, &path_clt->tasks, entry_clt) {
				if (task_pipe_tmp->task->done)
					continue;
				/* find the max between tasks on same client */
				throughput = max(throughput, task_pipe_tmp->throughput);
			}
			goto done;
		}

		if (timespec64_compare(&curr_time, &task_pipe_cur->task->end_time) >= 0) {
			throughput = mml_qos_max_freq(cfg->path[pipe], tp);
			goto done;
		}

		mml_core_calc_tput(task_pipe_cur->task, max_pixel, pipe,
			&task_pipe_cur->task->end_time, &curr_time);

		throughput = 0;
		list_for_each_entry(task_pipe_tmp, &path_clt->tasks, entry_clt) {
			if (task_pipe_tmp->task->done)
				continue;
			/* find the max throughput (frequency) between tasks on same client */
			throughput = max(throughput, task_pipe_tmp->throughput);
		}
	} else {
		/* no task anymore, clear */
		throughput = 0;
	}

done:
	path_clt->throughput = throughput;

	tput_up = mml_qos_update_sys(cfg->mml, cfg->dpc, cfg->path[pipe], false);
	if (throughput) {
		/* clear so that qos set api report max bw */
		task_pipe_cur->bandwidth = 0;
		/* check running task use sw pipe0 for right sigle pipe */
		if (task_pipe_cur->task->config->info.dl_pos == MML_DL_POS_RIGHT)
			tmp_pipe = 0;
		mml_core_qos_reset(task, tmp_pipe);
		list_for_each_entry(task_pipe_tmp, &path_clt->tasks, entry_clt) {
			if (task_pipe_tmp->task->done)
				continue;
			/* force all comp find max bw after reset */
			mml_core_qos_calc(task_pipe_tmp->task, tmp_pipe, throughput);
		}
		/* update the max bw for each comp for first task in this client */
		mml_core_qos_set(task, tmp_pipe, throughput, tput_up);
		bandwidth = task_pipe_cur->bandwidth;
	} else {
		if (task->config->info.dl_pos == MML_DL_POS_RIGHT)
			tmp_pipe = 0;
		mml_core_qos_reset(task, tmp_pipe);
		mml_core_qos_clear(task, tmp_pipe);
	}

	/* update/clear dpc bandwidth */
	mml_core_qos_update_dpc(cfg, true);
	if (cfg->dpc)
		tp->dpc_qos_ref--;

	mml_msg_qos("task dvfs end %s %s task %p throughput %u bandwidth %u pixel %u",
		racing_mode ? "racing" : "update",
		task_pipe_cur ? "new" : "last",
		task_pipe_cur ? task_pipe_cur->task : task,
		throughput, bandwidth, max_pixel);
exit:
	if (overdue)
		mml_trace_tag_end(MML_TTAG_OVERDUE);
	mutex_unlock(&tp->qos_mutex);
	mml_trace_ex_end();
}

static void core_taskdone_comp(struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	struct mml_comp_config *ccfg = cache->cfg;
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		struct mml_comp *comp = path->nodes[i].comp;

		call_hw_op(comp, task_done, task, &ccfg[i]);
	}
}

static void core_buffer_unprepare(struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		struct mml_comp *comp = path->nodes[i].comp;

		call_cfg_op(comp, buf_unprepare, task, &cache->cfg[i]);
	}
}

static void core_buffer_unmap(struct mml_task *task)
{
	const struct mml_topology_path *path = task->config->path[0];
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		struct mml_comp *comp = path->nodes[i].comp;

		call_cfg_op(comp, buf_unmap, task, &path->nodes[i]);
	}
}

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
static void core_dump_alloc(struct mml_file_buf *buf, struct mml_frm_dump_data *frm)
{
	u32 size = 0, i;

	for (i = 0; i < MML_MAX_PLANES; i++)
		size += buf->size[i];

	/* for better performance, reuse buffer if size ok */
	if (frm->bufsize < size) {
		vfree(frm->frame);
		frm->bufsize = 0;
		frm->size = 0;

		frm->frame = vmalloc(size);
		if (!frm->frame)
			return;

		/* assign real size of this buffer */
		frm->bufsize = size;
	}

	frm->size = size;
}

void mml_core_dump_buf(struct mml_task *task, const struct mml_frame_data *data,
	struct mml_file_buf *buf, struct mml_frm_dump_data *frm)
{
	u64 stamp = div_u64(sched_clock(), 1000000);
	char fmt[24];
	int ret;

	/* support only out0 for now, maybe support multi out later */
	get_fmt_str(fmt, sizeof(fmt), data->format);
	ret = snprintf(frm->name, sizeof(frm->name), "mml_%llu_%u_%s_f%s_%u_%u_%u.bin",
		stamp, task->job.jobid, frm->prefix, fmt,
		data->width, data->height, data->y_stride);
	if (ret >= sizeof(frm->name))
		frm->name[sizeof(frm->name)-1] = 0;

	core_dump_alloc(buf, frm);
	mml_log("%s size %u name %s", __func__, frm->size, frm->name);

	/* support only plane 0 for now, maybe support multi plane later */
	if (mml_buf_va_get(buf) < 0 || !buf->dma[0].va) {
		mml_err("%s dump fail %s no va", __func__, frm->name);
		frm->size = 0;	/* keep buf for reuse, only mark content empty */
		return;
	}

	memcpy(frm->frame, buf->dma[0].va, frm->size);
	mml_log("%s copy frame %s", __func__, frm->name);
}

static void core_dump_inst(struct cmdq_pkt *pkt)
{
	kfree(mml_inst_dump);
	mml_inst_dump = NULL;
	kfree(mml_inst_raw);
	mml_inst_raw = NULL;

	if (!mml_inst_dump && !mml_inst_raw) {
		mml_inst_dump = cmdq_pkt_parse_buf(pkt, &mml_inst_dump_sz, &mml_inst_raw, &mml_inst_raw_sz);

		mml_log("%s raw buffer %p size %u", __func__, mml_inst_raw, mml_inst_raw_sz);
	} else {
		/* overwrite pointer may leak */
		mml_err("%s overwrite not NULL pointer mml_inst_dump or mml_inst_raw", __func__);
	}
}

char *mml_core_get_dump_inst(u32 *size, void **raw, u32 *size_raw)
{
	*size = mml_inst_dump_sz;
	*raw = mml_inst_raw;
	*size_raw = mml_inst_raw_sz;
	return mml_inst_dump;
}

static bool mml_check_dumpsrv(enum dump_srv_option buf_opt, struct mml_file_buf *buf)
{
	bool dump;

	if (mml_dump_srv != DUMPCTRL_ENABLE)
		return false;

	mutex_lock(&mml_dump_mutex);
	dump = mml_dump_srv_opt & buf_opt;
	if (dump) {
		if (mml_dump_srv_opt & DUMPOPT_ONCE)
			mml_dump_srv = DUMPCTRL_PAUSE;
		/* for not dram case remove dump */
		if (!buf->dma[0].dmabuf) {
			dump = false;
			mml_dump_srv_opt = mml_dump_srv_opt & ~buf_opt;
		}
	}
	mutex_unlock(&mml_dump_mutex);

	if (dump && !buf->invalid)
		mml_buf_invalid(buf);

	return dump;
}
#endif	/* CONFIG_MTK_MML_DEBUG */

static void core_taskdone_kt_work(struct kthread_work *work)
{
	struct mml_task *task = container_of(work, struct mml_task, kt_work_done);
	u32 i;

	mml_trace_begin("%s", __func__);

	for (i = 0; i < task->buf.dest_cnt; i++) {
		if (task->buf.dest[i].invalid) {
			mml_trace_ex_begin("%s_invalid", __func__);
			mml_buf_invalid(&task->buf.dest[i]);
			mml_trace_ex_end();
		}
	}

	/* before clean up, signal buffer fence */
	if (task->fence) {
		dma_fence_put(task->fence);
		task->fence = NULL;
	}

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	if (mml_check_dumpsrv(DUMPOPT_DEST, &task->buf.dest[0])) {
		char fmt[24];
		struct mml_frame_data *data = &task->config->info.dest[0].data;

		get_fmt_str(fmt, sizeof(fmt), data->format);
		mml_dump_buf(task, data,
			data->width + task->config->info.dest[0].compose.left,
			data->height + task->config->info.dest[0].compose.top,
			"dest", fmt,
			&task->buf.dest[0], MMLDUMPT_DEST,
			mml_dump_srv_opt & DUMPOPT_DEST_ASYNC);
	}
#endif

	queue_work(task->config->wq_done, &task->work_done);
	mml_trace_end();
}

static void mml_core_mminfra_enable(struct mml_dev *mml, u32 pipe, struct mml_comp *mmlsys)
{
	mml_clock_lock(mml);
	mml_trace_ex_begin("%s_mminfra_pw_enable_%u", __func__, pipe);
	if (likely(mmlsys))
		call_hw_op(mmlsys, mminfra_pw_enable);
	mml_trace_ex_end();
	mml_clock_unlock(mml);
}

static void mml_core_mminfra_disable(struct mml_dev *mml, u32 pipe, struct mml_comp *mmlsys)
{
	mml_clock_lock(mml);
	mml_trace_ex_begin("%s_mminfra_pw_disable_%u", __func__, pipe);
	if (likely(mmlsys))
		call_hw_op(mmlsys, mminfra_pw_disable);
	mml_trace_ex_end();
	mml_clock_unlock(mml);
}

static void core_taskdone(struct work_struct *work)
{
	struct mml_task *task = container_of(work, struct mml_task, work_done);
	struct mml_frame_config *cfg = task->config;
	const struct mml_topology_path *path = cfg->path[0];
	u32 *perf, hw_time = 0;
	u32 jobid = task->job.jobid;

	mml_trace_begin("%s", __func__);
	mml_mmp(taskdone, MMPROFILE_FLAG_START, jobid, 0);
	mml_msg("%s job %u", __func__, jobid);

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	mml_dump_output(cfg->mml, path->mmlsys->sysid, task);
#endif

	/* dl mode fast on/off during hw run, so enable mminfra and except flow back */
	if (!mml_isdc(cfg->info.mode)) {
		mml_core_mminfra_enable(cfg->mml, 0, path->mmlsys);
		mml_dpc_exc_keep_task(task, path);
	}

	/* remove task in qos list and setup next */
	if (task->pkts[0])
		mml_core_dvfs_end(task, 0);
	if (cfg->dual && task->pkts[1])
		mml_core_dvfs_end(task, 1);

	if (mml_hw_perf && task->pkts[0]) {
		perf = cmdq_pkt_get_perf_ret(task->pkts[0]);
		if (perf) {
			hw_time = perf[1] > perf[0] ?
				perf[1] - perf[0] : ~perf[0] + 1 + perf[1];
			CMDQ_TICK_TO_US(hw_time);
		}
	}
	mml_mmp(exec, MMPROFILE_FLAG_END, jobid, hw_time);

	if (task->pkts[0])
		core_taskdone_comp(task, 0);
	if (cfg->dual && task->pkts[1])
		core_taskdone_comp(task, 1);

	if (task->pipe[0].en.clk)
		core_disable(task, 0);
	if (task->pipe[1].en.clk)
		core_disable(task, 1);

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	if (task->dump_queued[MMLDUMPT_SRC])
		mml_dump_wait(task, MMLDUMPT_SRC);
	if (task->dump_queued[MMLDUMPT_DEST])
		mml_dump_wait(task, MMLDUMPT_DEST);
#endif

	if (task->pkts[0])
		core_buffer_unprepare(task, 0);
	if (cfg->dual && task->pkts[1])
		core_buffer_unprepare(task, 1);

	core_buffer_unmap(task);

	/* task life time dpc off */
	if (cfg->dpc && cfg->info.mode != MML_MODE_DDP_ADDON)
		mml_dpc_task_cnt_dec(task);

	mml_dpc_exc_release_task(task, path);
	mml_core_mminfra_disable(cfg->mml, 0, cfg->path[0]->mmlsys);

	if (unlikely(cfg->task_ops->frame_err && !task->pkts[0] &&
		(!cfg->dual || !task->pkts[1])))
		cfg->task_ops->frame_err(task);
	else
		cfg->task_ops->frame_done(task);

	mml_mmp(taskdone, MMPROFILE_FLAG_END, jobid, 0);
	mml_trace_end();
}

static void core_taskdone_check(struct mml_task *task)
{
	struct mml_frame_config *cfg = task->config;
	u32 cnt;

	/* cnt can be 1 or 2, if dual on and count 2 means pipes done */
	cnt = atomic_inc_return(&task->pipe_done);
	mml_mmp(taskdone, MMPROFILE_FLAG_PULSE, task->job.jobid, ((cfg->dual << 16) | cnt));
	if (cfg->dual && cnt <= 1)
		return;

	task->done = true;
	if (task->fence) {
		dma_fence_signal(task->fence);
		mml_mmp(fence_sig, MMPROFILE_FLAG_PULSE, task->job.jobid,
			mmp_data2_fence(task->fence->context, task->fence->seqno));
	}
	kthread_queue_work(cfg->ctx_kt_done, &task->kt_work_done);
}

static void core_taskdone_cb(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data.data;
	struct mml_task *task = (struct mml_task *)pkt->user_data;
	u32 pipe;

	if (pkt == task->pkts[0]) {
		pipe = 0;
	} else if (pkt == task->pkts[1]) {
		pipe = 1;
	} else {
		mml_err("%s task %p pkt %p not match both pipe (%p and %p)",
			__func__, task, pkt, task->pkts[0], task->pkts[1]);
		return;
	}

	mml_trace_begin("mml_taskdone_cb_pipe%u", pipe);

	if (data.err)
		mml_mmp(irq_stop, MMPROFILE_FLAG_PULSE, task->job.jobid,
			((data.err & GENMASK(15, 0)) << 16) | pipe);
	else
		mml_mmp(irq_done, MMPROFILE_FLAG_PULSE, task->job.jobid, pipe);

	core_taskdone_check(task);
	mml_trace_end();
}

static s32 core_config(struct mml_task *task, u32 pipe)
{
	int ret;

	if (task->state == MML_TASK_INITIAL) {
		struct mml_tile_cache *tile_cache =
			task->config->task_ops->get_tile_cache(task, pipe);

		/* prepare data in each component for later tile use */
		ret = core_prepare(task, pipe);
		if (ret < 0) {
			mml_err("core prepare fail");
			return ret;
		}

		/* call to tile to calculate */
		mml_trace_ex_begin("%s_%s_%u", __func__, "tile", pipe);
		ret = calc_tile(task, pipe, tile_cache);
		mml_trace_ex_end();
		if (ret < 0) {
			mml_err("calc tile fail %d", ret);
			return ret;
		}

		/* dump frame tile for debug */
		if (mtk_mml_msg)
			dump_frame_tile(task->config->frame_tile[pipe]);
	} else {
		if (task->state == MML_TASK_DUPLICATE) {
			/* task need duplcicate before reuse */
			mml_trace_ex_begin("%s_%s_%u", __func__, "dup", pipe);
			ret = task->config->task_ops->dup_task(task, pipe);
			mml_trace_ex_end();
			if (ret < 0) {
				mml_err("dup task fail %d", ret);
				return ret;
			}
		}

		/* pkt exists, reuse it directly */
		mml_trace_ex_begin("%s_%s_%u", __func__, "reuse", pipe);
		ret = core_reuse(task, pipe);
		mml_trace_ex_end();
		if (ret < 0) {
			mml_err("core reuse fail");
			return ret;
		}
	}

	return 0;
}

static s32 core_command(struct mml_task *task, u32 pipe)
{
	s32 ret;

	mml_trace_ex_begin("%s_%s_%u", __func__, "cmd", pipe);
	if (pipe == 0)
		mml_mmp(command0, MMPROFILE_FLAG_START, task->job.jobid, pipe);
	else
		mml_mmp(command1, MMPROFILE_FLAG_START, task->job.jobid, pipe);

	if (task->state == MML_TASK_INITIAL) {
		/* make commands into pkt for later flush */
		ret = command_make(task, pipe);
	} else {
		ret = command_reuse(task, pipe);
	}

	if (pipe == 0)
		mml_mmp(command0, MMPROFILE_FLAG_END, task->job.jobid, pipe);
	else
		mml_mmp(command1, MMPROFILE_FLAG_END, task->job.jobid, pipe);
	mml_trace_ex_end();
	return ret;
}

enum mml_fence_index {
	mml_fence_src,
	mml_fence_src1,
	mml_fence_dest,
	mml_fence_dest1,
	mml_fence_name_total
};

const char *mml_fence_name[] = {
	[mml_fence_src] = "src",
	[mml_fence_src1] = "src1",
	[mml_fence_dest] = "dest",
	[mml_fence_dest1] = "dest1",
};

static void wait_dma_fence(enum mml_fence_index name_idx, struct dma_fence *fence, u32 jobid)
{
	long ret;
	const char *name = mml_fence_name[name_idx];

	if (!fence)
		return;

	mml_trace_ex_begin("%s_%s", __func__, name);
	mml_mmp(fence, MMPROFILE_FLAG_START, jobid,
		mmp_data2_fence(fence->context, fence->seqno));
	ret = dma_fence_wait_timeout(fence, false, msecs_to_jiffies(200));
	if (ret <= 0) {
		mml_err("wait fence %s %s-%s%llu-%llu fail %p ret %ld",
			name, fence->ops->get_driver_name(fence),
			fence->ops->get_timeline_name(fence),
			fence->context, fence->seqno, fence, ret);
		mml_mmp(fence_timeout, MMPROFILE_FLAG_PULSE, jobid,
			mmp_data2_fence(fence->context, fence->seqno));
	}
	mml_mmp(fence, MMPROFILE_FLAG_END, jobid, name_idx);

	mml_trace_ex_end();
}

static void core_taskdump(struct mml_task *task, u32 pipe, int err)
{
	struct mml_frame_config *cfg = task->config;
	const struct mml_topology_path *path = cfg->path[pipe];
	int cnt;

	if (err == -EBUSY) {
		/* inline rotate case self trigger, mark mmp and do nothing */
		mml_mmp(irq_loop, MMPROFILE_FLAG_PULSE, task->job.jobid, pipe);
		return;
	}

	cnt = mml_err_cnt++;

	mml_mmp(irq_err, MMPROFILE_FLAG_PULSE, task->job.jobid, cnt);

	/* turn on cmdq save log */
	mml_cmdq_err = 1;
	core_comp_dump(task, pipe, cnt);
	if (cnt < 3)
		mml_err("dump smi");

	/* record current fail task and do dump
	 * note: so this task maybe record multiple times due to error record
	 * and frame done record
	 */
	mml_record_track(cfg->mml, task);
	mml_record_dump(cfg->mml);
	mml_err("error dump %d end", cnt);

	call_dbg_op(path->mmlsys, reset, cfg, pipe);

	mml_err("error %d engine reset end", cnt);
	mml_cmdq_err = 0;

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	if (mml_timeout_dump) {
		mml_dump_input(task->config->mml, path->mmlsys->sysid, task, true);
		mml_timeout_dump = false;
	}
#endif

}

static void core_taskdump0_cb(struct cmdq_cb_data data)
{
	struct mml_task *task = (struct mml_task *)data.data;

	core_taskdump(task, 0, data.err);
}

static void core_taskdump1_cb(struct cmdq_cb_data data)
{
	struct mml_task *task = (struct mml_task *)data.data;

	core_taskdump(task, 1, data.err);
}

static const cmdq_async_flush_cb dump_cbs[MML_PIPE_CNT] = {
	[0] = core_taskdump0_cb,
	[1] = core_taskdump1_cb,
};

static int aee_cb(struct cmdq_cb_data data)
{
	static DEFINE_RATELIMIT_STATE(aee_rate, 30 * HZ, 2);
	bool aee = __ratelimit(&aee_rate);
	struct cmdq_pkt *pkt = data.data;
	struct mml_task *task = pkt->user_data;

	if (!aee) {
		task->dump_full = false;
		mml_err("task job %u skip full dump", task->job.jobid);
		return CMDQ_NO_AEE;
	}

	return CMDQ_AEE_WARN;
}

static void mml_core_stop_racing_pipe(struct mml_frame_config *cfg, u32 pipe, bool force)
{
	const struct mml_topology_path *path = cfg->path[pipe];
	struct mml_comp *comp;
	u32 i;

	if (force) {
		/* call cmdq to stop hardware thread directly */
		cmdq_mbox_channel_stop(cfg->path[pipe]->clt->chan);
		mml_mmp(racing_stop, MMPROFILE_FLAG_PULSE, cfg->last_jobid, pipe);

		for (i = 0; i < path->node_cnt; i++) {
			comp = path->nodes[i].comp;
			call_dbg_op(comp, reset, cfg, pipe);
		}
	} else {
		if (cmdq_mbox_get_usage(cfg->path[pipe]->clt->chan) <= 0)
			return;
		cmdq_thread_set_spr(cfg->path[pipe]->clt->chan, MML_CMDQ_NEXT_SPR,
			MML_NEXTSPR_NEXT);
		mml_mmp(racing_stop, MMPROFILE_FLAG_PULSE, cfg->last_jobid,
			pipe | BIT(4));
	}
}

static s32 core_flush(struct mml_task *task, u32 pipe)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_client *tp_clt = cfg->path[pipe]->clt;
	struct cmdq_client *rb_clt = mml_get_cmdq_clt(cfg->mml,
		pipe + GCE_THREAD_START);
	int i, ret;
	struct cmdq_pkt *pkt = task->pkts[pipe];

	mml_msg("%s task %p pipe %u pkt %p job %u",
		__func__, task, pipe, pkt, task->job.jobid);
	mml_trace_ex_begin("%s", __func__);

	core_enable(task, pipe);

	if (cfg->dpc) {
		cmdq_check_thread_complete(tp_clt->chan);

		if (cfg->info.mode == MML_MODE_DDP_ADDON ||
		    cfg->info.mode == MML_MODE_DIRECT_LINK)
			cmdq_check_thread_complete(rb_clt->chan);
	}

	/* before flush, wait buffer fence being signaled */
	task->wait_fence_time[pipe] = sched_clock();
	wait_dma_fence(mml_fence_src, task->buf.src.fence, task->job.jobid);
	if (cfg->info.dest[0].pq_config.en_region_pq)
		wait_dma_fence(mml_fence_src1, task->buf.seg_map.fence, task->job.jobid);
	for (i = 0; i < task->buf.dest_cnt; i++)
		wait_dma_fence(i == 0 ? mml_fence_dest : mml_fence_dest1, task->buf.dest[i].fence,
			       task->job.jobid);

	/* flush only once for both pipe */
	mutex_lock(&cfg->pipe_mutex);
	if (!task->buf.flushed) {
		/* also make sure buffer content flushed by other module */
		if (task->buf.src.flush) {
			mml_msg("%s flush source", __func__);
			mml_trace_ex_begin("%s_flush_src", __func__);
			mml_buf_flush(&task->buf.src);
			mml_trace_ex_end();
		}

		if (cfg->info.dest[0].pq_config.en_region_pq &&
		    task->buf.seg_map.flush) {
			mml_msg("%s flush region pq source", __func__);
			mml_trace_ex_begin("%s_flush_seg_map", __func__);
			mml_buf_flush(&task->buf.seg_map);
			mml_trace_ex_end();
		}

		for (i = 0; i < task->buf.dest_cnt; i++) {
			if (task->buf.dest[i].flush) {
				mml_msg("%s flush dest %d plane %hhu",
					__func__, i, task->buf.dest[i].cnt);
				mml_trace_ex_begin("%s_flush_dst_%u", __func__, i);
				mml_buf_flush(&task->buf.dest[i]);
				mml_trace_ex_end();
			}
		}

		task->buf.flushed = true;
	}
	mutex_unlock(&cfg->pipe_mutex);

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	mml_dump_input(task->config->mml, cfg->path[pipe]->mmlsys->sysid, task, false);

	if (pipe == 0 && mml_check_dumpsrv(DUMPOPT_SRC, &task->buf.src)) {
		char fmt[24];
		struct mml_frame_data *data = &cfg->info.src;

		get_fmt_str(fmt, sizeof(fmt), data->format);
		mml_dump_buf(task, data, data->width, data->height, "src", fmt,
			&task->buf.src, MMLDUMPT_SRC,
			mml_dump_srv_opt & DUMPOPT_SRC_ASYNC);
	}
#endif

	/* assign error handler */
	pkt->err_cb.cb = dump_cbs[pipe];
	pkt->aee_cb = aee_cb;
	pkt->err_cb.data = (void *)task;
	task->dump_full = true;

	if (cfg->info.mode == MML_MODE_RACING) {
		/* force stop current running racing */
		task->pkts[pipe]->self_loop = true;

		if (cfg->dual) {
			/* for racing mode sync with other pipe */
			mml_mmp(wait_ready, MMPROFILE_FLAG_PULSE, task->job.jobid, pipe);
			complete(&task->pipe[pipe].ready);
			wait_for_completion(&task->pipe[(pipe + 1) & 0x1].ready);
		}
	} else if (cfg->info.mode == MML_MODE_DIRECT_LINK) {
		/* direct link mode also loop, set flag to cmdq pass timeout */
		if (cfg->disp_vdo)
			task->pkts[pipe]->self_loop = true;
	}

	/* do dvfs/bandwidth calc right before flush to cmdq */
	mml_core_dvfs_begin(task, pipe);

	mml_trace_ex_begin("%s_cmdq", __func__);
	task->flush_time[pipe] = sched_clock();
	mml_mmp(flush, MMPROFILE_FLAG_PULSE, task->job.jobid,
		(unsigned long)task->pkts[pipe]);
	ret = cmdq_pkt_flush_async(pkt, core_taskdone_cb, (void *)task->pkts[pipe]);

	/* only start at pipe 0 and end at receive both pipe irq */
	if (pipe == 0)
		mml_mmp(exec, MMPROFILE_FLAG_START, task->job.jobid, 0);
	mml_trace_ex_end();

	mml_trace_ex_end();

	return ret;
}

static void core_config_pipe(struct mml_task *task, u32 pipe)
{
	s32 err;
	struct mml_frame_config *cfg = task->config;

	mml_trace_ex_begin("%s_%u_%u", __func__, pipe, task->job.jobid);
	task->config_pipe_time[pipe] = sched_clock();

	err = core_config(task, pipe);
	if (err < 0) {
		mml_err("config fail task %p pipe %u pkt %p",
			task, pipe, task->pkts[pipe]);
		core_taskdone_check(task);
		goto exit;
	}

	/* do not make command and flush from mml in addon case */
	if (cfg->nocmd) {
		mml_msg("%s task %p pipe %u done no cmd", __func__, task, pipe);
		goto exit;
	}

	err = core_command(task, pipe);
	if (err < 0) {
		mml_err("command fail task %p pipe %u pkt %p",
			task, pipe, task->pkts[pipe]);
		core_taskdone_check(task);
		goto exit;
	}
	err = core_flush(task, pipe);
	if (err < 0) {
		mml_err("flush fail task %p pipe %u pkt %p",
			task, pipe, task->pkts[pipe]);
		core_taskdone_check(task);
	}

	if (mml_pkt_dump && pipe == mml_pkt_dump_p && cfg->info.mode == mml_pkt_dump_m) {
		mml_clock_lock(cfg->mml);

		if (mml_pkt_dump == 1)
			cmdq_pkt_dump_buf(task->pkts[pipe], 0);

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
		if (mml_pkt_dump == 2) {
			core_dump_inst(task->pkts[pipe]);
			mml_pkt_dump = 0; /* avoid impact performance */
		}
#endif

		mml_clock_unlock(cfg->mml);
	}

	mml_msg("%s task %p job %u pipe %u pkt %p done",
		__func__, task, task->job.jobid, pipe, task->pkts[pipe]);
exit:
	if (cfg->dpc && cfg->info.mode != MML_MODE_DDP_ADDON && err < 0)
		mml_dpc_task_cnt_dec(task);

	mml_trace_ex_end();
}

static void core_config_pipe1_work(struct kthread_work *work)
{
	struct mml_task *task;

	task = container_of(work, struct mml_task, work_config[1]);
	core_config_pipe(task, 1);
}

static void core_buffer_map(struct mml_task *task)
{
	const struct mml_topology_path *path = task->config->path[0];
	struct mml_comp *comp;
	u32 i;

	mml_trace_begin("%s", __func__);
	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_cfg_op(comp, buf_map, task, &path->nodes[i]);
	}
	mml_trace_end();
}

static void core_config_task(struct mml_task *task)
{
	struct mml_frame_config *cfg = task->config;
	const u32 jobid = task->job.jobid;
	enum mml_mode mode = cfg->info.mode;
	s32 err;

	mml_trace_begin("%s_%u", __func__, jobid);
	if (mode == MML_MODE_DDP_ADDON)
		mml_mmp2(config_dle, MMPROFILE_FLAG_START,
			cfg->info.src.width, cfg->info.src.height,
			cfg->info.dest[0].data.width, cfg->info.dest[0].data.height);
	else
		mml_mmp2(config, MMPROFILE_FLAG_START,
			cfg->info.src.width, cfg->info.src.height,
			cfg->info.dest[0].data.width, cfg->info.dest[0].data.height);

	mml_msg("%s begin task %p config %p job %u",
		__func__, task, cfg, jobid);

	/* always set priority */
	if (cfg->task_ops->kt_setsched)
		cfg->task_ops->kt_setsched(task->ctx);

	/* topology */
	if (task->state == MML_TASK_INITIAL) {
		/* topology will fill in path instance */
		err = topology_select_path(cfg);
		dump_task(task);
		if (err < 0) {
			mml_err("%s select path fail %d", __func__, err);
			goto done;
		}
	}

	mml_update_pq_status(&cfg->info.dest[0].pq_config);
	mml_update_status_to_tppa(&cfg->info);

	/* before pipe1 start, make sure iova map from device by pipe0 */
	core_buffer_map(task);

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	task->dump_queued[MMLDUMPT_SRC] = false;
	task->dump_queued[MMLDUMPT_DEST] = false;
#endif

	/* enable mminfra and except flow during config */
	mml_core_mminfra_enable(cfg->mml, 0, cfg->path[0]->mmlsys);
	mml_dpc_exc_keep_task(task, cfg->path[0]);
	if (cfg->dpc && cfg->info.mode != MML_MODE_DDP_ADDON)
		mml_dpc_task_cnt_inc(task);

	/* create dual work_thread[1] */
	if (cfg->dual) {
		if (mode == MML_MODE_RACING) {
			/* init for dual pipe sync flush */
			init_completion(&task->pipe[0].ready);
			init_completion(&task->pipe[1].ready);
		}
		if (cfg->task_ops->queue)
			cfg->task_ops->queue(task, 1);
	}

	/* hold config in this task to avoid config release before call submit_done */
	cfg->cfg_ops->get(cfg);
	/* ref count to 2 thus destroy can be one of submit done and frame done */
	kref_get(&task->ref);

	core_config_pipe(task, 0);

	/* check single pipe or (dual) pipe 1 done then callback */
	if (cfg->dual) {
		if (cfg->task_ops->queue)
			kthread_flush_work(&task->work_config[1]);
		else
			core_config_pipe(task, 1);
	}

	cfg->task_ops->submit_done(task);

	/* The dl mode fast on/off during hw run, so disable mminfra and except flow
	 * And addon mode has no taskdone flow, thus release mminfra here to avoid power leak.
	 */
	if (!mml_isdc(mode)) {
		mml_dpc_exc_release_task(task, cfg->path[0]);
		mml_core_mminfra_disable(cfg->mml, 0, cfg->path[0]->mmlsys);
	}

	cfg->cfg_ops->put(cfg);

done:
	if (mode == MML_MODE_DDP_ADDON)
		mml_mmp(config_dle, MMPROFILE_FLAG_END, jobid, 0);
	else
		mml_mmp(config, MMPROFILE_FLAG_END, jobid, 0);
	mml_trace_end();
}

static void core_config_task_work(struct kthread_work *work)
{
	struct mml_task *task;

	task = container_of(work, struct mml_task, work_config[0]);
	core_config_task(task);
}

static s32 task_cnt_get(char *buf, const struct kernel_param *kp)
{
	s32 len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "MML driver task count: %d",
		(s32)atomic_read(&mml_task_ref));
	buf[len] = 0;

	return len;
}

static const struct kernel_param_ops task_cnt_param_ops = {
	.get = task_cnt_get,
};
module_param_cb(task_cnt, &task_cnt_param_ops, NULL, 0644);

struct mml_task *mml_core_create_task(u32 jobid)
{
	struct mml_task *task;
	s32 ret;

	mml_mmp(task_create, MMPROFILE_FLAG_PULSE, 0, 0);

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	if (unlikely(!task)) {
		mml_err("failed to create mml task");
		return ERR_PTR(-ENOMEM);
	}
	if (atomic_inc_return(&mml_task_ref) >= mml_task_check_cnt) {
		static bool aeeonce;

		mml_err("too many mml tasks:%d job %u",
			atomic_read(&mml_task_ref), jobid);

		if (!aeeonce) {
			aeeonce = true;
			mml_fatal("mml", "too many mml tasks:%d job %u",
				atomic_read(&mml_task_ref), jobid);
		}

	}
	INIT_LIST_HEAD(&task->entry);
	INIT_LIST_HEAD(&task->pipe[0].entry_clt);
	INIT_LIST_HEAD(&task->pipe[1].entry_clt);
	kthread_init_work(&task->work_config[0], core_config_task_work);
	kthread_init_work(&task->work_config[1], core_config_pipe1_work);
	INIT_WORK(&task->work_done, core_taskdone);
	kthread_init_work(&task->kt_work_done, core_taskdone_kt_work);

	kref_init(&task->ref);
	task->pipe[0].task = task;
	task->pipe[1].task = task;

	ret = mml_pq_task_create(task);
	if (ret) {
		kfree(task);
		return ERR_PTR(ret);
	}

	return task;
}

void mml_core_destroy_task(struct mml_task *task)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(task->reuse); i++) {
		vfree(task->reuse[i].labels);
		kfree(task->reuse[i].label_mods);
		kfree(task->reuse[i].label_check);
	}
	for (i = 0; i < ARRAY_SIZE(task->pkts); i++) {
		if (task->pkts[i])
			cmdq_pkt_destroy(task->pkts[i]);
	}
	mml_pq_task_release(task);
	kfree(task);
	atomic_dec(&mml_task_ref);
}

static void core_destroy_wq(struct workqueue_struct **wq)
{
	if (*wq) {
		destroy_workqueue(*wq);
		*wq = NULL;
	}
}

void mml_core_init_config(struct mml_frame_config *cfg)
{
	INIT_LIST_HEAD(&cfg->entry);
	INIT_LIST_HEAD(&cfg->tasks);
	INIT_LIST_HEAD(&cfg->await_tasks);
	INIT_LIST_HEAD(&cfg->done_tasks);
	mutex_init(&cfg->pipe_mutex);
	mutex_init(&cfg->hist_div_mutex);
	/* create work_thread 0, wait thread */
	cfg->wq_done = alloc_ordered_workqueue("mml_done", WORK_CPU_UNBOUND | WQ_HIGHPRI);
}

void mml_core_deinit_config(struct mml_frame_config *cfg)
{
	u32 pipe, i;

	/* make command, engine allocated private data */
	for (pipe = 0; pipe < MML_PIPE_CNT; pipe++) {
		if (!cfg->path[pipe])
			continue;
		for (i = 0; i < cfg->path[pipe]->node_cnt; i++)
			kfree(cfg->cache[pipe].cfg[i].data);
		destroy_frame_tile(cfg->frame_tile[pipe]);
	}
	core_destroy_wq(&cfg->wq_done);
}

static void core_update_config(struct mml_frame_config *cfg)
{
	const struct mml_frame_data *src = &cfg->info.src;
	const struct mml_frame_dest *dest;
	const struct mml_crop *crop;
	u32 i;

	cfg->frame_in.width = src->width;
	cfg->frame_in.height = src->height;
	for (i = 0; i < cfg->info.dest_cnt; i++) {
		dest = &cfg->info.dest[i];
		cfg->frame_in_crop[i] = dest->crop;
		cfg->out_rotate[i] = dest->rotate;
		cfg->out_flip[i] = dest->flip;
		if (dest->rotate == MML_ROT_0 || dest->rotate == MML_ROT_180) {
			cfg->frame_out[i].width = dest->compose.width;
			cfg->frame_out[i].height = dest->compose.height;
		} else {
			cfg->frame_out[i].width = dest->compose.height;
			cfg->frame_out[i].height = dest->compose.width;
		}
	}

	dest = &cfg->info.dest[0];
	crop = &dest->crop;
	if ((cfg->info.dest_cnt == 1 ||
	    !memcmp(crop, &dest[1].crop, sizeof(struct mml_crop))) &&
	    (crop->r.width != src->width || crop->r.height != src->height)) {
		u32 in_crop_w = crop->r.width;
		u32 in_crop_h = crop->r.height;

		if (in_crop_w + crop->r.left > src->width)
			in_crop_w = src->width - crop->r.left;
		if (in_crop_h + crop->r.top > src->height)
			in_crop_h = src->height - crop->r.top;

		cfg->frame_tile_sz.width = in_crop_w;
		cfg->frame_tile_sz.height = in_crop_h;
	} else {
		cfg->frame_tile_sz.width = src->width;
		cfg->frame_tile_sz.height = src->height;
	}

	/* store frame crop size as init, maybe change later in rrot/rsz */
	cfg->frame_in_hdr = cfg->frame_tile_sz;
}

void mml_core_submit_task(struct mml_frame_config *cfg, struct mml_task *task)
{
	/* reset to 0 in case reuse task */
	atomic_set(&task->pipe_done, 0);
	task->done = false;
	if (task->state == MML_TASK_INITIAL)
		core_update_config(cfg);

	if (cfg->task_ops->queue)
		cfg->task_ops->queue(task, 0);
	else
		core_config_task(task);
}

void mml_core_stop_racing(struct mml_frame_config *cfg, bool force)
{
	if (!cfg->path[0])
		return;

	mml_core_stop_racing_pipe(cfg, 0, force);
	if (cfg->dual)
		mml_core_stop_racing_pipe(cfg, 1, force);
}

static s32 check_label_idx(struct mml_task_reuse *reuse,
			     struct mml_pipe_cache *cache)
{
	if (reuse->label_idx >= cache->label_cnt) {
		mml_err("out of label cnt idx %u count %u",
			reuse->label_idx, cache->label_cnt);
		return -ENOMEM;
	}
	return 0;
}

void mml_add_reuse_label(u32 comp_id, struct mml_task_reuse *reuse, u16 *label_idx, u32 value)
{
	*label_idx = reuse->label_idx;
	reuse->labels[reuse->label_idx].val = value;
	reuse->label_mods[reuse->label_idx] = comp_id;
	reuse->label_idx++;
}

s32 mml_assign(u32 comp_id, struct cmdq_pkt *pkt, u16 reg_idx, u32 value,
	       struct mml_task_reuse *reuse,
	       struct mml_pipe_cache *cache,
	       u16 *label_idx)
{
	if (!cache->label_cnt)
		return cmdq_pkt_assign_command(pkt, reg_idx, value);

	if (check_label_idx(reuse, cache))
		return -ENOMEM;

	cmdq_pkt_assign_command_reuse(pkt, reg_idx, value,
		&reuse->labels[reuse->label_idx]);

	mml_add_reuse_label(comp_id, reuse, label_idx, value);
	return 0;
}

s32 mml_write(u32 comp_id, struct cmdq_pkt *pkt, dma_addr_t addr, u32 value, u32 mask,
	      struct mml_task_reuse *reuse,
	      struct mml_pipe_cache *cache,
	      u16 *label_idx)
{
	if (!cache->label_cnt)
		return cmdq_pkt_write_value_addr(pkt, addr, value, mask);

	if (check_label_idx(reuse, cache))
		return -ENOMEM;

	cmdq_pkt_write_value_addr_reuse(pkt, addr, value, mask,
		&reuse->labels[reuse->label_idx]);

	mml_add_reuse_label(comp_id, reuse, label_idx, value);
	return 0;
}

void mml_update(u32 comp_id, struct mml_task_reuse *reuse, u16 label_idx, u32 value)
{
	if (label_idx >= reuse->label_idx)
		mml_err("%s label idx %u/%u mod %u overflow value %#x",
			__func__, label_idx, reuse->label_idx, comp_id, value);

	if (comp_id != reuse->label_mods[label_idx])
		mml_err("%s label idx %u/%u mod %u %u module overwrite value %#x",
			__func__, label_idx, reuse->label_mods[label_idx], comp_id,
			reuse->label_mods[label_idx], value);

	reuse->labels[label_idx].val = value;
	reuse->label_check[label_idx] = true;
}

void mml_reuse_touch(u32 comp_id, struct mml_task_reuse *reuse, u16 label_idx)
{
	if (label_idx >= reuse->label_idx)
		mml_err("%s label idx %u/%u mod %u overflow",
			__func__, label_idx, reuse->label_idx, comp_id);

	if (comp_id != reuse->label_mods[label_idx])
		mml_err("%s label idx %u/%u mod %u %u module overwrite",
			__func__, label_idx, reuse->label_mods[label_idx], comp_id,
			reuse->label_mods[label_idx]);

	reuse->label_check[label_idx] = true;
}

static s32 mml_reuse_add_offset(struct mml_task_reuse *reuse,
	struct mml_reuse_array *reuses)
{
	struct cmdq_reuse *anchor, *last;
	u64 offset = 0, off_begin = 0;
	u32 reuses_idx;

	if (reuse->label_idx < 2 || !reuses->idx)
		goto add;

	anchor = &reuse->labels[reuse->label_idx - 2];
	last = anchor + 1;
	if (last->va < anchor->va)
		goto add;

	offset = (u64)(last->va - anchor->va);
	if (offset > MML_REUSE_OFFSET_MAX) {
		offset = 0;
		goto add;
	}

	/* if offset match or no last offset, use same mml_reuse_offset and
	 * increase the count only instead of add new one into reuse array
	 */
	reuses_idx = reuses->idx - 1;
	if (!reuses->offs[reuses_idx].offset && reuses->offs[reuses_idx].cnt) {
		reuses->offs[reuses_idx].offset = offset;
		/* reduce current label since it can offset by previous */
		reuse->label_idx--;
		goto inc;
	}

	off_begin = reuses->offs[reuses_idx].offset * reuses->offs[reuses_idx].cnt;
	if (offset && offset == off_begin) {
		if (!reuses->offs[reuses_idx].cnt)
			mml_err("%s reuse idx %u no count offset %llu",
				__func__, reuses_idx, offset);
		/* reduce current label since it can offset by previous */
		reuse->label_idx--;
		goto inc;
	}

	/* use last(new) label and clear offset since this is new mml_reuse_offset */
	offset = 0;

add:
	if (reuses->idx >= reuses->offs_size) {
		mml_err("%s label idx %u but reuse array full %u",
			__func__, reuse->label_idx - 1, reuses->idx);
		return -ENOMEM;
	}
	reuse->labels[reuse->label_idx - 1].op = 0;
	reuses->offs[reuses->idx].offset = (u16)offset;
	reuses->offs[reuses->idx].label_idx = reuse->label_idx - 1;
	/* increase reuse array index to next item */
	reuses->idx++;

inc:
	reuses->offs[reuses->idx - 1].cnt++;
	return 0;
}

s32 mml_write_array(u32 comp_id, struct cmdq_pkt *pkt, dma_addr_t addr, u32 value, u32 mask,
	struct mml_task_reuse *reuse, struct mml_pipe_cache *cache,
	struct mml_reuse_array *reuses)
{
	s32 ret;

	if (!cache->label_cnt)
		return cmdq_pkt_write_value_addr(pkt, addr, value, mask);

	ret = mml_write(comp_id, pkt, addr, value, mask, reuse, cache,
		&reuses->offs[reuses->idx].label_idx);

	if (ret < 0)
		return ret;
	return mml_reuse_add_offset(reuse, reuses);
}

void mml_update_array(u32 comp_id, struct mml_task_reuse *reuse,
	struct mml_reuse_array *reuses, u32 reuse_idx, u32 off_idx, u32 value)
{
	u32 label_idx = reuses->offs[reuse_idx].label_idx;
	struct cmdq_reuse *label = &reuse->labels[label_idx];
	u64 *va = label->va + reuses->offs[reuse_idx].offset * off_idx;

	if (comp_id != reuse->label_mods[label_idx])
		mml_err("%s label idx %u/%u mod %u %u module overwrite value %#x",
			__func__, label_idx, comp_id, reuse->label_mods[label_idx],
			reuse->label_idx, value);

	*va = (*va & GENMASK_ULL(63, 32)) | value;
	reuse->label_check[label_idx] = true;
}

#ifdef CONFIG_TRACING
#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
static noinline int tracing_mark_write(char *buf)
{
	trace_puts(buf);
	return 0;
}
#endif
#endif

int mml_tracing_mark_write(char *fmt, ...)
{
#ifdef CONFIG_TRACING
#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	char buf[MML_TRACE_MSG_LEN];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (len >= MML_TRACE_MSG_LEN) {
		mml_err("%s trace size %u exceed limit", __func__, len);
		return -1;
	}

	tracing_mark_write(buf);
#endif
#endif
	return 0;
}
