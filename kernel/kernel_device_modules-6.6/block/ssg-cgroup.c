// SPDX-License-Identifier: GPL-2.0
/*
 *  Control Group of SamSung Generic I/O scheduler
 *
 *  Copyright (C) 2021 Changheun Lee <nanich.lee@samsung.com>
 */

#include <linux/blkdev.h>
#include <linux/blk-mq.h>

#include "blk-cgroup.h"
#include "blk-mq.h"
#include "ssg.h"



static struct blkcg_policy ssg_blkcg_policy;



#define CPD_TO_SSG_BLKCG(_cpd) \
	container_of((_cpd), struct ssg_blkcg, cpd)
#define BLKCG_TO_SSG_BLKCG(_blkcg) \
	CPD_TO_SSG_BLKCG(blkcg_to_cpd((_blkcg), &ssg_blkcg_policy))

#define PD_TO_SSG_BLKG(_pd) \
	container_of((_pd), struct ssg_blkg, pd)
#define BLKG_TO_SSG_BLKG(_blkg) \
	PD_TO_SSG_BLKG(blkg_to_pd((_blkg), &ssg_blkcg_policy))

#define CSS_TO_SSG_BLKCG(css) BLKCG_TO_SSG_BLKCG(css_to_blkcg(css))

#define MIN_SCHED_TAGS		32
#define MIN_AVAILABLE_RATIO	30 /* note: max_async_write_ratio is 25% */
#define MAX_AVAILABLE_RATIO	100



static struct blkcg_policy_data *ssg_blkcg_cpd_alloc(gfp_t gfp)
{
	struct ssg_blkcg *ssg_blkcg;

	ssg_blkcg = kzalloc(sizeof(struct ssg_blkcg), gfp);
	if (ZERO_OR_NULL_PTR(ssg_blkcg))
		return NULL;

	spin_lock_init(&ssg_blkcg->lock);
	ssg_blkcg->max_available_ratio = MAX_AVAILABLE_RATIO;
	ssg_blkcg->boost_on = false;

	return &ssg_blkcg->cpd;
}

static void ssg_blkcg_cpd_free(struct blkcg_policy_data *cpd)
{
	struct ssg_blkcg *ssg_blkcg = CPD_TO_SSG_BLKCG(cpd);

	if (IS_ERR_OR_NULL(ssg_blkcg))
		return;

	kfree(ssg_blkcg);
}

static void ssg_blkg_set_shallow_depth(struct blkcg_gq *blkg,
		struct blk_mq_tags *tags)
{
	unsigned int depth = tags->bitmap_tags.sb.depth;
	unsigned int map_nr = tags->bitmap_tags.sb.map_nr;
	struct ssg_blkg *ssg_blkg;
	struct ssg_blkcg *ssg_blkcg;

	if (depth < MIN_SCHED_TAGS)
		return;

	ssg_blkg = BLKG_TO_SSG_BLKG(blkg);
	if (IS_ERR_OR_NULL(ssg_blkg))
		return;

	ssg_blkcg = BLKCG_TO_SSG_BLKCG(blkg->blkcg);
	if (IS_ERR_OR_NULL(ssg_blkcg))
		return;

	spin_lock(&ssg_blkcg->lock);
	ssg_blkg->max_available_rqs =
		depth * ssg_blkcg->max_available_ratio / 100U;
	ssg_blkg->shallow_depth =
		max_t(unsigned int, 1, ssg_blkg->max_available_rqs / map_nr);
	spin_unlock(&ssg_blkcg->lock);
}

static struct blkg_policy_data *ssg_blkcg_pd_alloc(struct gendisk *disk,
		struct blkcg *blkcg, gfp_t gfp)
{
	struct ssg_blkg *ssg_blkg;

	ssg_blkg = kzalloc_node(sizeof(struct ssg_blkg), gfp, disk->node_id);
	if (ZERO_OR_NULL_PTR(ssg_blkg))
		return NULL;

	return &ssg_blkg->pd;
}

static void ssg_blkcg_pd_init(struct blkg_policy_data *pd)
{
	struct ssg_blkg *ssg_blkg;
	struct blk_mq_hw_ctx *hctx;
	unsigned long i;

	ssg_blkg = PD_TO_SSG_BLKG(pd);
	if (IS_ERR_OR_NULL(ssg_blkg))
		return;

	atomic_set(&ssg_blkg->current_rqs, 0);
	queue_for_each_hw_ctx(pd->blkg->q, hctx, i)
		ssg_blkg_set_shallow_depth(pd->blkg, hctx->sched_tags);
}

static void ssg_blkcg_pd_free(struct blkg_policy_data *pd)
{
	struct ssg_blkg *ssg_blkg = PD_TO_SSG_BLKG(pd);

	if (IS_ERR_OR_NULL(ssg_blkg))
		return;

	kfree(ssg_blkg);
}

unsigned int ssg_blkcg_shallow_depth(struct request_queue *q)
{
	struct blkcg_gq *blkg;
	struct ssg_blkg *ssg_blkg;

	rcu_read_lock();
	blkg = blkg_lookup(css_to_blkcg(curr_css()), q);
	ssg_blkg = BLKG_TO_SSG_BLKG(blkg);
	rcu_read_unlock();

	if (IS_ERR_OR_NULL(ssg_blkg))
		return 0;

	if (atomic_read(&ssg_blkg->current_rqs) < ssg_blkg->max_available_rqs)
		return 0;

	return ssg_blkg->shallow_depth;
}

void ssg_blkcg_depth_updated(struct blk_mq_hw_ctx *hctx)
{
	struct request_queue *q = hctx->queue;
	struct cgroup_subsys_state *pos_css;
	struct blkcg_gq *blkg;
	struct ssg_blkg *ssg_blkg;

	rcu_read_lock();
	blkg_for_each_descendant_pre(blkg, pos_css, q->root_blkg) {
		ssg_blkg = BLKG_TO_SSG_BLKG(blkg);
		if (IS_ERR_OR_NULL(ssg_blkg))
			continue;

		atomic_set(&ssg_blkg->current_rqs, 0);
		ssg_blkg_set_shallow_depth(blkg, hctx->sched_tags);
	}
	rcu_read_unlock();
}

void ssg_blkcg_inc_rq(struct blkcg_gq *blkg)
{
	struct ssg_blkg *ssg_blkg = BLKG_TO_SSG_BLKG(blkg);

	if (IS_ERR_OR_NULL(ssg_blkg))
		return;

	atomic_inc(&ssg_blkg->current_rqs);
}

void ssg_blkcg_dec_rq(struct blkcg_gq *blkg)
{
	struct ssg_blkg *ssg_blkg = BLKG_TO_SSG_BLKG(blkg);

	if (IS_ERR_OR_NULL(ssg_blkg))
		return;

	atomic_dec(&ssg_blkg->current_rqs);
}

bool ssg_blkcg_check_boost(struct blkcg_gq *blkg)
{
	struct ssg_blkcg *ssg_blkcg;

	if (IS_ERR_OR_NULL(blkg))
		return 0;

	ssg_blkcg = BLKCG_TO_SSG_BLKCG(blkg->blkcg);
	if (IS_ERR_OR_NULL(ssg_blkcg))
		return 0;

	return ssg_blkcg->boost_on;
}

static void __ssg_blkcg_update_shallow_depth(struct blk_mq_hw_ctx *hctx)
{
	struct request_queue *q = hctx->queue;
	struct cgroup_subsys_state *pos_css;
	struct blkcg_gq *blkg;

	rcu_read_lock();
	blkg_for_each_descendant_pre(blkg, pos_css, q->root_blkg)
		ssg_blkg_set_shallow_depth(blkg, hctx->sched_tags);
	rcu_read_unlock();
}

static void ssg_blkcg_update_shallow_depth(struct blkcg *blkcg)
{
	struct blkcg_gq *blkg;
	struct blk_mq_hw_ctx *hctx;
	unsigned long i;

	hlist_for_each_entry(blkg, &blkcg->blkg_list, blkcg_node)
		queue_for_each_hw_ctx(blkg->q, hctx, i)
			__ssg_blkcg_update_shallow_depth(hctx);
}

static int ssg_blkcg_show_max_available_ratio(struct seq_file *sf, void *v)
{
	struct ssg_blkcg *ssg_blkcg = CSS_TO_SSG_BLKCG(seq_css(sf));

	if (IS_ERR_OR_NULL(ssg_blkcg))
		return -EINVAL;

	seq_printf(sf, "%d\n", ssg_blkcg->max_available_ratio);

	return 0;
}

static int ssg_blkcg_set_max_available_ratio(struct cgroup_subsys_state *css,
		struct cftype *cftype, u64 ratio)
{
	struct blkcg *blkcg;
	struct ssg_blkcg *ssg_blkcg;

	blkcg = css_to_blkcg(css);
	if (IS_ERR_OR_NULL(blkcg))
		return -EINVAL;

	ssg_blkcg = BLKCG_TO_SSG_BLKCG(blkcg);
	if (IS_ERR_OR_NULL(ssg_blkcg))
		return -EINVAL;

	if (ratio < MIN_AVAILABLE_RATIO || ratio > MAX_AVAILABLE_RATIO)
		return -EINVAL;

	spin_lock(&ssg_blkcg->lock);
	ssg_blkcg->max_available_ratio = ratio;
	spin_unlock(&ssg_blkcg->lock);

	ssg_blkcg_update_shallow_depth(blkcg);

	return 0;
}

static int ssg_blkcg_show_boost_on(struct seq_file *sf, void *v)
{
	struct ssg_blkcg *ssg_blkcg = CSS_TO_SSG_BLKCG(seq_css(sf));

	if (IS_ERR_OR_NULL(ssg_blkcg))
		return -EINVAL;

	seq_printf(sf, "%d\n", ssg_blkcg->boost_on);

	return 0;
}

static int ssg_blkcg_set_boost_on(struct cgroup_subsys_state *css,
		struct cftype *cftype, u64 boost_on)
{
	struct blkcg *blkcg;
	struct ssg_blkcg *ssg_blkcg;

	blkcg = css_to_blkcg(css);
	if (IS_ERR_OR_NULL(blkcg))
		return -EINVAL;

	ssg_blkcg = BLKCG_TO_SSG_BLKCG(blkcg);
	if (IS_ERR_OR_NULL(ssg_blkcg))
		return -EINVAL;

	spin_lock(&ssg_blkcg->lock);
	ssg_blkcg->boost_on = !!boost_on;
	spin_unlock(&ssg_blkcg->lock);

	return 0;
}

struct cftype ssg_blkg_files[] = {
	{
		.name = "ssg.max_available_ratio",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = ssg_blkcg_show_max_available_ratio,
		.write_u64 = ssg_blkcg_set_max_available_ratio,
	},
	{
		.name = "ssg.boost_on",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = ssg_blkcg_show_boost_on,
		.write_u64 = ssg_blkcg_set_boost_on,
	},

	{} /* terminate */
};

static struct blkcg_policy ssg_blkcg_policy = {
	.legacy_cftypes = ssg_blkg_files,

	.cpd_alloc_fn = ssg_blkcg_cpd_alloc,
	.cpd_free_fn = ssg_blkcg_cpd_free,

	.pd_alloc_fn = ssg_blkcg_pd_alloc,
	.pd_init_fn = ssg_blkcg_pd_init,
	.pd_free_fn = ssg_blkcg_pd_free,
};

int ssg_blkcg_activate(struct request_queue *q)
{
	return blkcg_activate_policy(q->disk, &ssg_blkcg_policy);
}

void ssg_blkcg_deactivate(struct request_queue *q)
{
	blkcg_deactivate_policy(q->disk, &ssg_blkcg_policy);
}

int ssg_blkcg_init(void)
{
	return blkcg_policy_register(&ssg_blkcg_policy);
}

void ssg_blkcg_exit(void)
{
	blkcg_policy_unregister(&ssg_blkcg_policy);
}
