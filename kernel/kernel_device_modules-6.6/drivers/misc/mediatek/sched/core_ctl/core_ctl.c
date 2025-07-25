// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/syscore_ops.h>
#include <linux/module.h>
#include <uapi/linux/sched/types.h>
#include <trace/hooks/sched.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <linux/sched/clock.h>

#ifndef __CHECKER__
#define CREATE_TRACE_POINTS
#include "core_ctl_trace.h"
#endif

#include "sched_avg.h"
#include <sched_sys_common.h>
#include <thermal_interface.h>
#include <eas/eas_plus.h>
#include "core_ctl.h"

#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_6893)
extern int get_immediate_tslvts1_1_wrap(void);
#endif
#define TAG "core_ctl"

struct ppm_table {
	unsigned long power;
	unsigned int leakage;
	unsigned int thermal_leakage;
	unsigned int freq;
	unsigned int capacity;
	unsigned long eff;
	unsigned long thermal_eff;
};

struct cluster_ppm_data {
	bool init;
	int opp_nr;
	struct ppm_table *ppm_tbl;
};

struct cluster_data {
	bool inited;
	bool enable;
	int nr_up;
	int nr_down;
	int need_spread_cpus;
	unsigned int cluster_id;
	unsigned int min_cpus;
	unsigned int max_cpus;
	unsigned int first_cpu;
	unsigned int active_cpus;
	unsigned int num_cpus;
	unsigned int nr_assist;
	unsigned int up_thres;
	unsigned int down_thres;
	unsigned int thermal_degree_thres;
	unsigned int thermal_up_thres;
	unsigned int nr_not_preferred_cpus;
	unsigned int need_cpus;
	unsigned int new_need_cpus;
	unsigned int boost;
	unsigned int nr_paused_cpus;
	unsigned int cpu_busy_up_thres;
	unsigned int cpu_busy_down_thres;
	cpumask_t cpu_mask;
	bool pending;
	spinlock_t pending_lock;
	struct task_struct *core_ctl_thread;
	struct kobject kobj;
	s64 offline_throttle_ms;
	s64 next_offline_time;
	struct cluster_ppm_data ppm_data;
};

struct cpu_data {
	bool not_preferred;
	bool paused_by_cc;
	bool force_paused;
	unsigned int cpu;
	unsigned int cpu_util_pct;
	unsigned int is_busy;
	struct cluster_data *cluster;
};

#define ENABLE		1
#define DISABLE		0
#define L_CLUSTER_ID	0
#define BL_CLUSTER_ID	1
#define AB_CLUSTER_ID	2
#define CORE_OFF	true
#define CORE_ON		false
#define MAX_CLUSTERS		3
#define MAX_CPUS_PER_CLUSTER	6
#define MAX_NR_DOWN_THRESHOLD	4
#define MAX_BTASK_THRESH	100
#define MAX_CPU_TJ_DEGREE	100000
#define BIG_TASK_AVG_THRESHOLD	25

#define for_each_cluster(cluster, idx) \
	for ((cluster) = &cluster_state[idx]; (idx) < num_clusters;\
		(idx)++, (cluster) = &cluster_state[idx])

#define core_ctl_debug(x...)		\
	do {				\
		if (debug_enable)	\
			pr_info(x);	\
	} while (0)

static DEFINE_PER_CPU(struct cpu_data, cpu_state);
static struct cluster_data cluster_state[MAX_CLUSTERS];
static unsigned int num_clusters;
static DEFINE_SPINLOCK(core_ctl_state_lock);
static DEFINE_SPINLOCK(core_ctl_window_check_lock);
static DEFINE_SPINLOCK(core_ctl_pause_lock);
static DEFINE_RWLOCK(core_ctl_policy_lock);
static bool initialized;
static unsigned int default_min_cpus[MAX_CLUSTERS] = {4, 2, 0};
ATOMIC_NOTIFIER_HEAD(core_ctl_notifier);
static bool debug_enable;
module_param_named(debug_enable, debug_enable, bool, 0600);

static void periodic_debug_handler(struct work_struct *work);
static int periodic_debug_enable;
static int periodic_debug_delay = 50;
module_param_named(periodic_debug_delay, periodic_debug_delay, int, 0600);
static DECLARE_DELAYED_WORK(periodic_debug, periodic_debug_handler);

enum {
	DISABLE_DEBUG = 0,
	DEBUG_STD,
	DEBUG_DETAIL,
	DEBUG_CNT
};

static int set_core_ctl_debug_level(const char *buf,
			       const struct kernel_param *kp)
{
	int ret = 0;
	unsigned int val = 0;

	ret = kstrtouint(buf, 0, &val);
	if (val >= DEBUG_CNT)
		ret = -EINVAL;

	if (!ret) {
		if (periodic_debug_enable != val) {
			periodic_debug_enable = val;
			if (periodic_debug_enable > DISABLE_DEBUG)
				mod_delayed_work(system_power_efficient_wq,
						&periodic_debug,
						msecs_to_jiffies(periodic_debug_delay));
		}
	}
	return ret;
}

static struct kernel_param_ops set_core_ctl_debug_param_ops = {
	.set = set_core_ctl_debug_level,
	.get = param_get_uint,
};

param_check_uint(periodic_debug_enable, &periodic_debug_enable);
module_param_cb(periodic_debug_enable, &set_core_ctl_debug_param_ops, &periodic_debug_enable, 0600);
MODULE_PARM_DESC(periodic_debug_enable, "echo periodic debug trace if needed");

static unsigned int enable_policy;
/*
 *  core_ctl_enable_policy - enable policy of core control
 *  @enable: true if set, false if unset.
 */
int core_ctl_enable_policy(unsigned int policy)
{
	unsigned int old_val;
	unsigned long flags;
	bool success = false;

	write_lock_irqsave(&core_ctl_policy_lock, flags);
	if (policy != enable_policy) {
		old_val = enable_policy;
		enable_policy = policy;
		success = true;
	}
	write_unlock_irqrestore(&core_ctl_policy_lock, flags);
	if (success)
		pr_info("%s: Change policy from %d to %d successfully.",
			TAG, old_val, policy);
	return 0;
}
EXPORT_SYMBOL(core_ctl_enable_policy);

static int set_core_ctl_policy(const char *buf,
			       const struct kernel_param *kp)
{
	int ret = 0;
	unsigned int val = 0;

	ret = kstrtouint(buf, 0, &val);
	if (val >= POLICY_CNT)
		ret = -EINVAL;

	if (!ret)
		core_ctl_enable_policy(val);
	return ret;
}

static struct kernel_param_ops set_core_ctl_policy_param_ops = {
	.set = set_core_ctl_policy,
	.get = param_get_uint,
};

param_check_uint(policy_enable, &enable_policy);
module_param_cb(policy_enable, &set_core_ctl_policy_param_ops, &enable_policy, 0600);
MODULE_PARM_DESC(policy_enable, "echo cpu pause policy if needed");

static unsigned int apply_limits(const struct cluster_data *cluster,
				 unsigned int need_cpus)
{
	return min(max(cluster->min_cpus, need_cpus), cluster->max_cpus);
}

/**
 * cpumask_complement - *dstp = ~*srcp
 * @dstp: the cpumask result
 * @srcp: the input to invert
 */
static inline void cpumask_complement(struct cpumask *dstp,
				      const struct cpumask *srcp)
{
	bitmap_complement(cpumask_bits(dstp), cpumask_bits(srcp),
					      nr_cpumask_bits);
}

int sched_isolate_count(const cpumask_t *mask, bool include_offline)
{
	cpumask_t count_mask = CPU_MASK_NONE;

	if (include_offline) {
		cpumask_complement(&count_mask, cpu_online_mask);
		cpumask_or(&count_mask, &count_mask, cpu_pause_mask);
		cpumask_and(&count_mask, &count_mask, mask);
	} else
		cpumask_and(&count_mask, mask, cpu_pause_mask);

	return cpumask_weight(&count_mask);
}

static unsigned int get_active_cpu_count(const struct cluster_data *cluster)
{
	return cluster->num_cpus -
		sched_isolate_count(&cluster->cpu_mask, true);
}

static bool is_active(const struct cpu_data *state)
{
	return cpu_online(state->cpu) && !cpu_paused(state->cpu);
}

static bool adjustment_possible(const struct cluster_data *cluster,
				unsigned int need)
{
	/*
	 * Why need to check nr_paused_cpu ?
	 * Consider the following situation,
	 * num_cpus = 4, min_cpus = 4 and a cpu
	 * force paused. That will do inactive
	 * core-on in the time.
	 */
	return (need < cluster->active_cpus || (need > cluster->active_cpus
			&& cluster->nr_paused_cpus));
}

static void wake_up_core_ctl_thread(struct cluster_data *cluster)
{
	unsigned long flags;

	spin_lock_irqsave(&cluster->pending_lock, flags);
	cluster->pending = true;
	spin_unlock_irqrestore(&cluster->pending_lock, flags);

	wake_up_process(cluster->core_ctl_thread);
}

bool is_cluster_init(unsigned int cid)
{
	return  cluster_state[cid].inited;
}

static bool demand_eval(struct cluster_data *cluster)
{
	unsigned long flags;
	unsigned int need_cpus = 0;
	bool ret = false;
	bool need_flag = false;
	unsigned int new_need;
	unsigned int old_need;
	s64 now, elapsed;

	if (unlikely(!cluster->inited))
		return ret;

	spin_lock_irqsave(&core_ctl_state_lock, flags);

	read_lock(&core_ctl_policy_lock);
	if (cluster->boost || !cluster->enable || !enable_policy)
		need_cpus = cluster->max_cpus;
	else
		need_cpus = cluster->new_need_cpus;
	read_unlock(&core_ctl_policy_lock);

	/* check again active cpus. */
	cluster->active_cpus = get_active_cpu_count(cluster);
	new_need = apply_limits(cluster, need_cpus);
	/*
	 * When there is no adjustment in need, avoid
	 * unnecessary waking up core_ctl thread
	 */
	need_flag = adjustment_possible(cluster, new_need);
	old_need = cluster->need_cpus;

	now = ktime_to_ms(ktime_get());

	/* core-on */
	if (new_need > cluster->active_cpus) {
		ret = true;
	} else {
		/*
		 * If no more CPUs are needed or paused,
		 * just update the next offline time.
		 */
		if (new_need == cluster->active_cpus) {
			cluster->next_offline_time = now;
			cluster->need_cpus = new_need;
			goto unlock;
		}

		/* Does it exceed throttle time ? */
		elapsed = now - cluster->next_offline_time;
		ret = elapsed >= cluster->offline_throttle_ms;
	}

	if (ret) {
		cluster->next_offline_time = now;
		cluster->need_cpus = new_need;
	}
	trace_core_ctl_demand_eval(cluster->cluster_id,
			old_need, new_need,
			cluster->active_cpus,
			cluster->min_cpus,
			cluster->max_cpus,
			cluster->boost,
			cluster->enable,
			ret && need_flag);
unlock:
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	return ret && need_flag;
}

static void apply_demand(struct cluster_data *cluster)
{
	if (demand_eval(cluster))
		wake_up_core_ctl_thread(cluster);
}

static int test_set_val(struct cluster_data *cluster, unsigned int val)
{
	unsigned long flags;
	struct cpumask disable_mask;
	unsigned int disable_cpus;
	unsigned int test_cluster_min_cpus[MAX_CLUSTERS];
	unsigned int total_min_cpus = 0;
	int i;

	cpumask_clear(&disable_mask);
	cpumask_complement(&disable_mask, cpu_online_mask);
	cpumask_or(&disable_mask, &disable_mask, cpu_pause_mask);
	cpumask_and(&disable_mask, &disable_mask, cpu_possible_mask);
	disable_cpus = cpumask_weight(&disable_mask);
	if (disable_cpus >= (nr_cpu_ids-2))
		return -EINVAL;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for(i=0; i<MAX_CLUSTERS; i++)
		test_cluster_min_cpus[i] = cluster_state[i].min_cpus;

	test_cluster_min_cpus[cluster->cluster_id] = val;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	for(i=0; i<MAX_CLUSTERS; i++)
		total_min_cpus += test_cluster_min_cpus[i];
	if (total_min_cpus < 2){
		pr_info("%s: invalid setting, retain cpus should >= 2", TAG);
		return -EINVAL;
	}

	return 0;
}

static inline int core_ctl_pause_cpu(unsigned int cpu)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_pause_lock, flags);
	ret = sched_pause_cpu(cpu);
	spin_unlock_irqrestore(&core_ctl_pause_lock, flags);

	return ret;
}

static inline int core_ctl_resume_cpu(unsigned int cpu)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_pause_lock, flags);
	ret = sched_resume_cpu(cpu);
	spin_unlock_irqrestore(&core_ctl_pause_lock, flags);

	return ret;
}

static void set_min_cpus(struct cluster_data *cluster, unsigned int val)
{
	unsigned long flags;

	if(val < cluster->min_cpus){
		if (test_set_val(cluster, val))
			return;
	}
	spin_lock_irqsave(&core_ctl_state_lock, flags);
	cluster->min_cpus = min(val, cluster->max_cpus);
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	wake_up_core_ctl_thread(cluster);
}

static void set_max_cpus(struct cluster_data *cluster, unsigned int val)
{
	unsigned long flags;

	if(val < cluster->min_cpus){
		if (test_set_val(cluster, val))
			return;
	}

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	val = min(val, cluster->num_cpus);
	cluster->max_cpus = val;
	cluster->min_cpus = min(cluster->min_cpus, cluster->max_cpus);
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	wake_up_core_ctl_thread(cluster);
}

static void set_thermal_up_thres(struct cluster_data *cluster, unsigned int val)
{
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	cluster->thermal_up_thres = val;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	wake_up_core_ctl_thread(cluster);
}

void set_offline_throttle_ms(struct cluster_data *cluster, unsigned int val)
{
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	cluster->offline_throttle_ms = val;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	apply_demand(cluster);
}

static inline
void update_next_cluster_down_thres(unsigned int index,
				     unsigned int new_thresh)
{
	struct cluster_data *next_cluster;

	if (index == num_clusters - 1)
		return;

	next_cluster = &cluster_state[index + 1];
	next_cluster->down_thres = new_thresh;
}

static inline
void set_not_preferred_locked(int cpu, bool enable)
{
	struct cpu_data *c;
	struct cluster_data *cluster;
	bool changed = false;

	c = &per_cpu(cpu_state, cpu);
	cluster = c->cluster;
	if (enable) {
		changed = !c->not_preferred;
		c->not_preferred = 1;
	} else {
		if (c->not_preferred) {
			c->not_preferred = 0;
			changed = !c->not_preferred;
		}
	}

	if (changed) {
		if (enable)
			cluster->nr_not_preferred_cpus += 1;
		else
			cluster->nr_not_preferred_cpus -= 1;
	}
}

static int set_up_thres(struct cluster_data *cluster, unsigned int val)
{
	unsigned int old_thresh;
	unsigned long flags;
	int ret = 0;

	if (val > MAX_BTASK_THRESH)
		val = MAX_BTASK_THRESH;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	old_thresh = cluster->up_thres;

	if (old_thresh != val) {
		ret = set_over_threshold(cluster->cluster_id, val);
		if (!ret) {
			cluster->up_thres = val;
			update_next_cluster_down_thres(
				cluster->cluster_id,
				cluster->up_thres);
		}
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	return ret;
}

/* ==================== export function ======================== */

int core_ctl_set_min_cpus(unsigned int cid, unsigned int min)
{
	struct cluster_data *cluster;

	if (cid >= num_clusters)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_min_cpus(cluster, min);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_min_cpus);

int core_ctl_set_max_cpus(unsigned int cid, unsigned int max)
{
	struct cluster_data *cluster;

	if (cid >= num_clusters)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_max_cpus(cluster, max);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_max_cpus);

/*
 *  core_ctl_set_limit_cpus - set min/max cpus of the cluster
 *  @cid: cluster id
 *  @min: min cpus
 *  @max: max cpus.
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_limit_cpus(unsigned int cid,
			     unsigned int min,
			     unsigned int max)
{
	struct cluster_data *cluster;
	unsigned long flags;

	if (cid >= num_clusters)
		return -EINVAL;

	if (max < min)
		return -EINVAL;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	cluster = &cluster_state[cid];
	max = min(max, cluster->num_cpus);
	min = min(min, max);
	cluster->max_cpus = max;
	cluster->min_cpus = min;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	core_ctl_debug("%s: Try to adjust cluster %u limit cpus. min_cpus: %u, max_cpus: %u",
			TAG, cid, min, max);
	wake_up_core_ctl_thread(cluster);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_limit_cpus);

/*
 *  core_ctl_set_offline_throttle_ms - set throttle time of core-off judgement
 *  @cid: cluster id
 *  @throttle_ms: The unit of throttle time is microsecond
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_offline_throttle_ms(unsigned int cid,
				     unsigned int throttle_ms)
{
	struct cluster_data *cluster;

	if (cid >= num_clusters)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_offline_throttle_ms(cluster, throttle_ms);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_offline_throttle_ms);

/*
 *  core_ctl_set_boost
 *  @return: 0 if success, else return errno
 *
 *  When boost is enbled, all cluster of CPUs will be core-on.
 */
int core_ctl_set_boost(bool boost)
{
	int ret = 0;
	unsigned int index = 0;
	unsigned long flags;
	struct cluster_data *cluster;
	bool changed = false;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for_each_cluster(cluster, index) {
		if (boost) {
			changed = !cluster->boost;
			cluster->boost = 1;
		} else {
			if (cluster->boost) {
				cluster->boost = 0;
				changed = !cluster->boost;
			} else {
				/* FIXME: change to continue ? */
				ret = -EINVAL;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	if (changed) {
		index = 0;
		for_each_cluster(cluster, index)
			apply_demand(cluster);
	}

	core_ctl_debug("%s: boost=%d ret=%d ", TAG, boost, ret);
	return ret;
}
EXPORT_SYMBOL(core_ctl_set_boost);

#define	MAX_CPU_MASK	((1 << nr_cpu_ids) - 1)
/*
 *  core_ctl_set_not_preferred - set not_prefer for the specific cpu number
 *  @not_preferred_cpus: Stand for cpu bitmap, 1 if set, 0 if unset.
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_not_preferred(unsigned int not_preferred_cpus)
{
	unsigned long flags;
	int i;
	bool bval;

	if (not_preferred_cpus > MAX_CPU_MASK)
		return -EINVAL;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for (i = 0; i < nr_cpu_ids; i++) {
		bval = !!(not_preferred_cpus & (1 << i));
		set_not_preferred_locked(i, bval);
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(core_ctl_set_not_preferred);

/*
 *  core_ctl_set_up_thres - adjuset up threshold value
 *  @cid: cluster id
 *  @val: Percentage of big core capactity. (0 - 100)
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_up_thres(int cid, unsigned int val)
{
	struct cluster_data *cluster;

	if (cid >= num_clusters)
		return -EINVAL;

	/* Range of up thrash should be 0 - 100 */
	if (val > MAX_BTASK_THRESH)
		val = MAX_BTASK_THRESH;

	cluster = &cluster_state[cid];
	return set_up_thres(cluster, val);
}
EXPORT_SYMBOL(core_ctl_set_up_thres);

/*
 *  core_ctl_force_pause_cpu - force pause or resume cpu
 *  @cpu: cpu number
 *  @is_pause: set true if pause, set false if resume.
 *
 *  return 0 if success, else return errno
 */
static bool test_disable_cpu(unsigned int cpu);
static void core_ctl_call_notifier(unsigned int cpu, unsigned int is_pause);
int core_ctl_force_pause_cpu(unsigned int cpu, bool is_pause)
{
	int ret;
	unsigned long flags;
	struct cpu_data *c;
	struct cluster_data *cluster;

	if (cpu > nr_cpu_ids)
		return -EINVAL;

	if (!cpu_online(cpu))
		return -EBUSY;

	/* Avoid hotplug change online mask */
	cpu_hotplug_disable();
	if (is_pause && !test_disable_cpu(cpu)){
		cpu_hotplug_enable();
		pr_info("%s: force pause failed, retain cpus should >= 2", TAG);
		return -EBUSY;
	}

	c = &per_cpu(cpu_state, cpu);
	cluster = c->cluster;

	if(is_pause)
		ret = core_ctl_pause_cpu(cpu);
	else
		ret = core_ctl_resume_cpu(cpu);

	/* error occurs */
	if (ret)
		goto print_out;

	/* Update cpu state */
	spin_lock_irqsave(&core_ctl_state_lock, flags);
	c->force_paused = is_pause;
	/* Handle conflict with original policy */
	if (c->paused_by_cc) {
		c->paused_by_cc = false;
		cluster->nr_paused_cpus--;
	}
	cluster->active_cpus = get_active_cpu_count(cluster);
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	core_ctl_call_notifier(cpu, is_pause);

print_out:
	if (is_pause){
		if (ret < 0){
			pr_info("[Core Force Pause] Pause request ret=%d, cpu=%d, paused=0x%lx, online=0x%lx, act=0x%lx\n",
				ret, cpu, cpu_pause_mask->bits[0], cpu_online_mask->bits[0],
				cpu_active_mask->bits[0]);
		} else if (ret){
			pr_info("[Core Force Pause] Already Pause: cpu=%d, paused=0x%lx, online=0x%lx, act=0x%lx\n",
				cpu, cpu_pause_mask->bits[0], cpu_online_mask->bits[0],
				cpu_active_mask->bits[0]);
		} else {
			pr_info("[Core Force Pause] Pause success: cpu=%d, paused=0x%lx, online=0x%lx, act=0x%lx\n",
				cpu, cpu_pause_mask->bits[0], cpu_online_mask->bits[0],
				cpu_active_mask->bits[0]);
		}
	} else {
		if (ret < 0){
			pr_info("[Core Force Pause] Resume request ret=%d, cpu=%d, paused=0x%lx, online=0x%lx, act=0x%lx\n",
				ret, cpu, cpu_pause_mask->bits[0], cpu_online_mask->bits[0],
				cpu_active_mask->bits[0]);
		} else if (ret){
			pr_info("[Core Force Pause] Already Resume: cpu=%d, paused=0x%lx, online=0x%lx, act=0x%lx\n",
				cpu, cpu_pause_mask->bits[0], cpu_online_mask->bits[0],
				cpu_active_mask->bits[0]);
		} else {
			pr_info("[Core Force Pause] Resume success: cpu=%d, paused=0x%lx, online=0x%lx, act=0x%lx\n",
				cpu, cpu_pause_mask->bits[0], cpu_online_mask->bits[0],
				cpu_active_mask->bits[0]);
		}
	}
	cpu_hotplug_enable();
	return ret;
}
EXPORT_SYMBOL(core_ctl_force_pause_cpu);

static ssize_t store_thermal_up_thres(struct cluster_data *state,
		const char *buf, size_t threshold)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	set_thermal_up_thres(state, val);
	return threshold;
}

/*
 *  core_ctl_set_cpu_busy_thres - set threshold of cpu busy state
 *  @cid: cluster id
 *  @pct: percentage of cpu loading(0-100).
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_cpu_busy_thres(unsigned int cid, unsigned int pct)
{
	unsigned long flags;
	struct cluster_data *cluster;

	if (pct > 100 || cid > 2)
		return -EINVAL;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	cluster = &cluster_state[cid];
	cluster->cpu_busy_up_thres = pct;
	cluster->cpu_busy_down_thres = pct > 20 ? pct - 20 : 0;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_cpu_busy_thres);

void core_ctl_notifier_register(struct notifier_block *n)
{
	atomic_notifier_chain_register(&core_ctl_notifier, n);
}
EXPORT_SYMBOL(core_ctl_notifier_register);

void core_ctl_notifier_unregister(struct notifier_block *n)
{
	atomic_notifier_chain_unregister(&core_ctl_notifier, n);
}
EXPORT_SYMBOL(core_ctl_notifier_unregister);

/* ==================== sysctl node ======================== */

static ssize_t store_min_cpus(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	set_min_cpus(state, val);
	return count;
}

static ssize_t show_min_cpus(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->min_cpus);
}

static ssize_t store_max_cpus(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	set_max_cpus(state, val);
	return count;
}

static ssize_t show_max_cpus(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->max_cpus);
}

static ssize_t store_offline_throttle_ms(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	set_offline_throttle_ms(state, val);
	return count;
}

static ssize_t show_offline_throttle_ms(const struct cluster_data *state,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%lld\n", state->offline_throttle_ms);
}

static ssize_t store_up_thres(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	/* No need to change up_thres for the last cluster */
	if (state->cluster_id >= num_clusters-1)
		return -EINVAL;

	if (val > MAX_BTASK_THRESH)
		val = MAX_BTASK_THRESH;

	set_up_thres(state, val);
	return count;
}

static ssize_t show_up_thres(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->up_thres);
}

static ssize_t store_not_preferred(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int i;
	unsigned int val[MAX_CPUS_PER_CLUSTER];
	unsigned long flags;
	int ret;

	ret = sscanf(buf, "%u %u %u %u %u %u\n",
			&val[0], &val[1], &val[2], &val[3],
			&val[4], &val[5]);
	if (ret != state->num_cpus)
		return -EINVAL;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for (i = 0; i < state->num_cpus; i++)
		set_not_preferred_locked(i + state->first_cpu, val[i]);
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	return count;
}

static ssize_t show_not_preferred(const struct cluster_data *state, char *buf)
{
	ssize_t count = 0;
	struct cpu_data *c;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for (i = 0; i < state->num_cpus; i++) {
		c = &per_cpu(cpu_state, i + state->first_cpu);
		count += scnprintf(buf + count, PAGE_SIZE - count,
				"CPU#%d: %u\n", c->cpu, c->not_preferred);
	}
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	return count;
}

static ssize_t store_core_ctl_boost(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	/* only allow first cluster */
	if (state->cluster_id != 0)
		return -EINVAL;

	if (val == 0 || val == 1)
		core_ctl_set_boost(val);
	else
		return -EINVAL;

	return count;
}

static ssize_t show_core_ctl_boost(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->boost);
}

static ssize_t store_enable(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;
	bool bval;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	bval = !!val;
	if (bval != state->enable) {
		state->enable = bval;
		apply_demand(state);
	}

	return count;
}

static ssize_t show_enable(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->enable);
}

static ssize_t show_thermal_up_thres(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->thermal_up_thres);
}

static ssize_t show_cpu_busy_up_thres(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->cpu_busy_up_thres);
}

static ssize_t show_global_state(const struct cluster_data *state, char *buf)
{
	struct cpu_data *c;
	struct cluster_data *cluster;
	ssize_t count = 0;
	unsigned int cpu;

	spin_lock_irq(&core_ctl_state_lock);
	for_each_possible_cpu(cpu) {
		c = &per_cpu(cpu_state, cpu);
		cluster = c->cluster;
		if (!cluster || !cluster->inited)
			continue;

		/* Only show this cluster */
		if (!cpumask_test_cpu(cpu, &state->cpu_mask))
			continue;

		if (cluster->first_cpu == cpu) {
			count += snprintf(buf + count, PAGE_SIZE - count,
					"Cluster%u\n", state->cluster_id);
			count += snprintf(buf + count, PAGE_SIZE - count,
					"\tFirst CPU: %u\n", cluster->first_cpu);
			count += snprintf(buf + count, PAGE_SIZE - count,
					"\tActive CPUs: %u\n",
					get_active_cpu_count(cluster));
			count += snprintf(buf + count, PAGE_SIZE - count,
					"\tNeed CPUs: %u\n", cluster->need_cpus);
			count += snprintf(buf + count, PAGE_SIZE - count,
					"\tNR Paused CPUs(pause by core_ctl): %u\n",
					cluster->nr_paused_cpus);
		}

		count += snprintf(buf + count, PAGE_SIZE - count,
				"CPU%u\n", cpu);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tOnline: %u\n", cpu_online(c->cpu));
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tPaused: %u\n", cpu_paused(c->cpu));
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tPaused by core_ctl: %u\n", c->paused_by_cc);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tCPU utils(%%): %u\n", c->cpu_util_pct);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tIs busy: %u\n", c->is_busy);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tNot preferred: %u\n", c->not_preferred);
	}
	spin_unlock_irq(&core_ctl_state_lock);

	return count;
}

struct core_ctl_attr {
	struct attribute attr;
	ssize_t (*show)(const struct cluster_data *state, char *buf);
	ssize_t (*store)(struct cluster_data *state, const char *buf, size_t count);
};

#define core_ctl_attr_ro(_name)         \
static struct core_ctl_attr _name =     \
__ATTR(_name, 0400, show_##_name, NULL)

#define core_ctl_attr_rw(_name)                 \
static struct core_ctl_attr _name =             \
__ATTR(_name, 0600, show_##_name, store_##_name)

core_ctl_attr_rw(min_cpus);
core_ctl_attr_rw(max_cpus);
core_ctl_attr_rw(offline_throttle_ms);
core_ctl_attr_rw(up_thres);
core_ctl_attr_rw(not_preferred);
core_ctl_attr_rw(core_ctl_boost);
core_ctl_attr_rw(enable);
core_ctl_attr_ro(global_state);
core_ctl_attr_rw(thermal_up_thres);
core_ctl_attr_ro(cpu_busy_up_thres);

static struct attribute *default_attrs[] = {
	&min_cpus.attr,
	&max_cpus.attr,
	&offline_throttle_ms.attr,
	&up_thres.attr,
	&not_preferred.attr,
	&core_ctl_boost.attr,
	&enable.attr,
	&global_state.attr,
	&thermal_up_thres.attr,
	&cpu_busy_up_thres.attr,
	NULL
};
ATTRIBUTE_GROUPS(default);

#define to_cluster_data(k) container_of(k, struct cluster_data, kobj)
#define to_attr(a) container_of(a, struct core_ctl_attr, attr)
static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct cluster_data *data = to_cluster_data(kobj);
	struct core_ctl_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->show)
		ret = cattr->show(data, buf);

	return ret;
}

static ssize_t store(struct kobject *kobj, struct attribute *attr,
		const char *buf, size_t count)
{
	struct cluster_data *data = to_cluster_data(kobj);
	struct core_ctl_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->store)
		ret = cattr->store(data, buf, count);

	return ret;
}

static const struct sysfs_ops sysfs_ops = {
	.show   = show,
	.store  = store,
};

static struct kobj_type ktype_core_ctl = {
	.sysfs_ops      = &sysfs_ops,
	.default_groups  = default_groups,
};

static unsigned long heaviest_thres = 520;
static unsigned int max_task_util;
static unsigned int big_cpu_ts;

/* ==================== algorithm of core control ======================== */

#define MAX_NR_RUNNING_THRESHOLD   4
/*
 * Get number of the busy CPU cores, protected by core_ctl_state_lock.
 */
static void get_busy_cpus(void)
{
	struct cluster_data *cluster;
	struct cpu_data *c;
	int cpus = 0, cid = 0, i = 0;

	for (cid = 0; cid < num_clusters; cid++) {
		cluster = &cluster_state[cid];
		for_each_cpu(i, &cluster->cpu_mask) {
			c = &per_cpu(cpu_state, i);
			if (c->is_busy && get_max_nr_running(i) > MAX_NR_RUNNING_THRESHOLD)
				cpus++;
		}
		cluster->need_spread_cpus = cpus;
		cpus = 0;
	}
}

#define BIG_TASK_AVG_THRESHOLD 25
/*
 * Get number of big tasks, protected by core_ctl_state_lock.
 */
static void get_nr_running_big_task(void)
{
	int avg_down[MAX_CLUSTERS] = {0};
	int avg_up[MAX_CLUSTERS] = {0};
	int nr_up[MAX_CLUSTERS] = {0};
	int nr_down[MAX_CLUSTERS] = {0};
	int need_spread_cpus[MAX_CLUSTERS] = {0};
	int i, delta;

	for (i = 0; i < num_clusters; i++) {
		sched_get_nr_over_thres_avg(i,
					  &avg_down[i],
					  &avg_up[i],
					  &nr_down[i],
					  &nr_up[i]);
	}

	for (i = 0; i < num_clusters; i++) {
		/* reset nr_up and nr_down */
		cluster_state[i].nr_up = 0;
		cluster_state[i].nr_down = 0;

		if (nr_up[i]) {
			if (avg_up[i]/nr_up[i] > BIG_TASK_AVG_THRESHOLD)
				cluster_state[i].nr_up = nr_up[i];
			else /* min(avg_up[i]/BIG_TASK_AVG_THRESHOLD,nr_up[i]) */
				cluster_state[i].nr_up =
					avg_up[i]/BIG_TASK_AVG_THRESHOLD > nr_up[i] ?
					nr_up[i] : avg_up[i]/BIG_TASK_AVG_THRESHOLD;
		}
		/*
		 * The nr_up is part of nr_down, so
		 * the real nr_down is nr_down minus nr_up.
		 */
		delta = nr_down[i] - nr_up[i];
		if (nr_down[i] && delta > 0) {
			if (((avg_down[i]-avg_up[i]) / delta)
					> BIG_TASK_AVG_THRESHOLD)
				cluster_state[i].nr_down = delta;
			else
				cluster_state[i].nr_down =
					(avg_down[i]-avg_up[i])/
					delta < BIG_TASK_AVG_THRESHOLD ?
					delta : (avg_down[i]-avg_up[i])/
					BIG_TASK_AVG_THRESHOLD;
		}
		/* nr can't be negative */
		cluster_state[i].nr_up =
			cluster_state[i].nr_up < 0 ? 0 : cluster_state[i].nr_up;
		cluster_state[i].nr_down =
			cluster_state[i].nr_down < 0 ? 0 : cluster_state[i].nr_down;
	}

	for (i = 0; i < num_clusters; i++) {
		nr_up[i] = cluster_state[i].nr_up;
		nr_down[i] = cluster_state[i].nr_down;
		need_spread_cpus[i] = cluster_state[i].need_spread_cpus;
	}
	trace_core_ctl_update_nr_over_thres(nr_up, nr_down, need_spread_cpus);
}

/*
 * prev_cluster_nr_assist:
 *   Tasks that are eligible to run on the previous
 *   cluster but cannot run because of insufficient
 *   CPUs there. It's indicative of number of CPUs
 *   in this cluster that should assist its
 *   previous cluster to makeup for insufficient
 *   CPUs there.
 */
static inline int get_prev_cluster_nr_assist(int index)
{
	struct cluster_data *prev_cluster;

	if (index == 0)
		return 0;

	index--;
	prev_cluster = &cluster_state[index];
	return prev_cluster->nr_assist;

}

#define CORE_CTL_PERIODIC_TRACK_MS	4
static inline bool window_check(void)
{
	unsigned long flags;
	ktime_t now = ktime_get();
	static ktime_t tracking_last_update;
	bool do_check = false;

	spin_lock_irqsave(&core_ctl_window_check_lock, flags);
	if (ktime_after(now, ktime_add_ms(
		tracking_last_update, CORE_CTL_PERIODIC_TRACK_MS))) {
		do_check = true;
		tracking_last_update = now;
	}
	spin_unlock_irqrestore(&core_ctl_window_check_lock, flags);
	return do_check;
}

static void check_heaviest_util(void)
{
	struct cluster_data *big_cluster;
	struct cluster_data *mid_cluster;
	unsigned int max_capacity;

	if (num_clusters <= 2)
		return;

	sched_max_util_task(&max_task_util);
	big_cluster = &cluster_state[num_clusters - 1];
	mid_cluster = &cluster_state[big_cluster->cluster_id - 1];
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
	big_cpu_ts = get_cpu_temp(big_cluster->first_cpu);
#elif IS_ENABLED(CONFIG_MTK_PLAT_POWER_6893)
	big_cpu_ts = get_immediate_tslvts1_1_wrap();
#endif

	heaviest_thres = mid_cluster->up_thres;
	if (big_cpu_ts > big_cluster->thermal_degree_thres)
		heaviest_thres = mid_cluster->thermal_up_thres;

	/*
	 * Check for biggest task in system,
	 * if it's over threshold, force to enable
	 * prime core.
	 */
	max_capacity = get_max_capacity(mid_cluster->cluster_id);
	heaviest_thres = div64_u64(heaviest_thres * max_capacity, 100);

	if (big_cluster->new_need_cpus)
		return;

	/* If max util is over threshold */
	if (max_task_util > heaviest_thres) {
		big_cluster->new_need_cpus++;

		/*
		 * For example:
		 *   Consider TLP=1 and heaviest is over threshold,
		 *   prefer to decrease a need CPU of mid cluster
		 *   to save power consumption
		 */
		if (mid_cluster->new_need_cpus > 0)
			mid_cluster->new_need_cpus--;
	}
}

static inline void core_ctl_main_algo(void)
{
	unsigned int index = 0;
	struct cluster_data *cluster;
	unsigned int orig_need_cpu[MAX_CLUSTERS] = {0};
	struct cpumask active_cpus;

	/* get needed spread cpus */
	get_busy_cpus();
	/* get TLP of over threshold tasks */
	get_nr_running_big_task();

	/* Apply TLP of tasks */
	for_each_cluster(cluster, index) {
		int temp_need_cpus = 0;

		if (index == 0) {
			cluster->nr_assist = cluster->nr_up;
			cluster->nr_assist += cluster->need_spread_cpus;
			cluster->new_need_cpus = cluster->num_cpus;
			continue;
		}

		temp_need_cpus += cluster->nr_up;
		temp_need_cpus += cluster->nr_down;
		temp_need_cpus += cluster->need_spread_cpus;
		temp_need_cpus += get_prev_cluster_nr_assist(index);

		cluster->new_need_cpus = temp_need_cpus;

		/* nr_assist(i) = max(0, need_cpus(i) - max_cpus(i)) */
		cluster->nr_assist =
			(temp_need_cpus > cluster->max_cpus ?
			 (temp_need_cpus - cluster->max_cpus) : 0);
	}

	/*
	 * Ensure prime cpu make core-on
	 * if heaviest task is over threshold.
	 */
	check_heaviest_util();

	index = 0;
	for_each_cluster(cluster, index) {
		orig_need_cpu[index] = cluster->new_need_cpus;
	}

	cpumask_andnot(&active_cpus, cpu_online_mask, cpu_pause_mask);
	trace_core_ctl_algo_info(big_cpu_ts, heaviest_thres, max_task_util,
			cpumask_bits(&active_cpus)[0], orig_need_cpu);
}

unsigned long (*calc_eff_hook)(unsigned int first_cpu, int opp, unsigned int temp,
	unsigned long dyn_power, unsigned int cap);
EXPORT_SYMBOL(calc_eff_hook);
static int need_update_ppm_eff = 1;
static int update_ppm_eff(void);

void core_ctl_tick(void *data, struct rq *rq)
{
	unsigned int index = 0;
	unsigned long flags;
	struct cluster_data *cluster;
	int cpu = 0;
	struct cpu_data *c;

	/* prevent irq disable on cpu 0 */
	if (rq->cpu == 0)
		return;

	if (!window_check())
		return;

	if (need_update_ppm_eff != 0 && calc_eff_hook) {
		/* Prevent other threads into this section */
		need_update_ppm_eff = 0;
		need_update_ppm_eff = update_ppm_eff();
	}

	spin_lock_irqsave(&core_ctl_state_lock, flags);

	/* check CPU is busy or not */
	for_each_possible_cpu(cpu) {
		c = &per_cpu(cpu_state, cpu);
		cluster = c->cluster;

		if (!cluster || !cluster->inited)
			continue;

		c->cpu_util_pct = get_cpu_util_pct(cpu, false);
		if (c->cpu_util_pct >= cluster->cpu_busy_up_thres)
			c->is_busy = true;
		else if (c->cpu_util_pct < cluster->cpu_busy_down_thres)
			c->is_busy = false;
	}

	read_lock(&core_ctl_policy_lock);
	if (enable_policy) {
		read_unlock(&core_ctl_policy_lock);
		core_ctl_main_algo();
	} else
		read_unlock(&core_ctl_policy_lock);

	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	/* reset index value */
	/* index = 0; */
	for_each_cluster(cluster, index) {
		apply_demand(cluster);
	}
}

inline void core_ctl_update_active_cpu(unsigned int cpu)
{
	unsigned long flags;
	struct cpu_data *c;
	struct cluster_data *cluster;

	if (cpu > nr_cpu_ids)
		return;

	spin_lock_irqsave(&core_ctl_state_lock, flags);
	c = &per_cpu(cpu_state, cpu);
	cluster = c->cluster;
	cluster->active_cpus = get_active_cpu_count(cluster);
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
}
EXPORT_SYMBOL(core_ctl_update_active_cpu);

static struct cpumask try_to_pause(struct cluster_data *cluster, int need)
{
	unsigned long flags;
	unsigned int num_cpus = cluster->num_cpus;
	unsigned int nr_paused = 0;
	int cpu;
	bool success;
	bool check_not_prefer = cluster->nr_not_preferred_cpus;
	bool check_busy = true;
	int ret = 0;
	struct cpumask cpu_pause_res;

	cpumask_clear(&cpu_pause_res);

again:
	nr_paused = 0;
	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for (cpu = nr_cpu_ids-1; cpu >= 0; cpu--) {
		struct cpu_data *c;

		success = false;
		if (!cpumask_test_cpu(cpu, &cluster->cpu_mask))
			continue;

		if (!num_cpus--)
			break;

		c = &per_cpu(cpu_state, cpu);
		if (!is_active(c))
			continue;

		if (!test_disable_cpu(cpu))
			continue;

		if (check_busy && c->is_busy)
			continue;

		if (c->force_paused)
			continue;

		if (cluster->active_cpus == need)
			break;

		/*
		 * Pause only the not_preferred CPUs.
		 * If none of the CPUs are selected as not_preferred,
		 * then all CPUs are eligible for isolation.
		 */
		if (check_not_prefer && !c->not_preferred)
			continue;

		spin_unlock_irqrestore(&core_ctl_state_lock, flags);
		core_ctl_debug("%s: Trying to pause CPU%u\n", TAG, c->cpu);
		ret = core_ctl_pause_cpu(cpu);
		if (ret < 0){
			core_ctl_debug("%s Unable to pause CPU%u err=%d\n", TAG, c->cpu, ret);
		} else if (!ret) {
			success = true;
			cpumask_set_cpu(c->cpu, &cpu_pause_res);
			core_ctl_call_notifier(cpu, 1);
			nr_paused++;
		} else {
			cpumask_set_cpu(c->cpu, &cpu_pause_res);
			core_ctl_debug("%s Unable to pause CPU%u already paused\n", TAG, c->cpu);
		}
		spin_lock_irqsave(&core_ctl_state_lock, flags);
		if (success) {
			/* check again, prevent a seldom racing issue */
			if (cpu_online(c->cpu))
				c->paused_by_cc = true;
			else {
				nr_paused--;
				pr_info("%s: Pause failed because cpu#%d is offline. ",
					TAG, c->cpu);
			}
		}
		cluster->active_cpus = get_active_cpu_count(cluster);
	}
	cluster->nr_paused_cpus += nr_paused;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	/*
	 * If the number of not prefer CPUs is not
	 * equal to need CPUs, then check it again.
	 */
	if (check_busy || (check_not_prefer &&
		cluster->active_cpus != need)) {
		num_cpus = cluster->num_cpus;
		check_not_prefer = false;
		check_busy = false;
		goto again;
	}
	return cpu_pause_res;
}

static struct cpumask try_to_resume(struct cluster_data *cluster, int need)
{
	unsigned long flags;
	unsigned int num_cpus = cluster->num_cpus, cpu;
	unsigned int nr_resumed = 0;
	bool check_not_prefer = cluster->nr_not_preferred_cpus;
	bool success;
	int ret = 0;
	struct cpumask cpu_resume_res;

	cpumask_clear(&cpu_resume_res);

again:
	nr_resumed = 0;
	spin_lock_irqsave(&core_ctl_state_lock, flags);
	for_each_cpu(cpu, &cluster->cpu_mask) {
		struct cpu_data *c;

		success = false;
		if (!num_cpus--)
			break;

		c = &per_cpu(cpu_state, cpu);

		if (!c->paused_by_cc)
			continue;

		if (c->force_paused)
			continue;

		if (!cpu_online(c->cpu))
			continue;

		if (!cpu_paused(c->cpu))
			continue;

		if (cluster->active_cpus == need)
			break;

		/* The Normal CPUs are resumed prior to not prefer CPUs */
		if (!check_not_prefer && c->not_preferred)
			continue;

		spin_unlock_irqrestore(&core_ctl_state_lock, flags);

		core_ctl_debug("%s: Trying to resume CPU%u\n", TAG, c->cpu);
		ret = core_ctl_resume_cpu(cpu);
		if (ret < 0){
			core_ctl_debug("%s Unable to resume CPU%u err=%d\n", TAG, c->cpu, ret);
		} else if (!ret) {
			success = true;
			cpumask_set_cpu(c->cpu, &cpu_resume_res);
			core_ctl_call_notifier(cpu, 0);
			nr_resumed++;
		} else {
			cpumask_set_cpu(c->cpu, &cpu_resume_res);
			core_ctl_debug("%s: Unable to resume CPU%u already resumed\n", TAG, c->cpu);
		}
		spin_lock_irqsave(&core_ctl_state_lock, flags);
		if (success)
			c->paused_by_cc = false;
		cluster->active_cpus = get_active_cpu_count(cluster);
	}
	cluster->nr_paused_cpus -= nr_resumed;
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);
	/*
	 * After un-isolated the number of prefer CPUs
	 * is not enough for need CPUs, then check
	 * not_prefer CPUs again.
	 */
	if (check_not_prefer &&
		cluster->active_cpus != need) {
		num_cpus = cluster->num_cpus;
		check_not_prefer = false;
		goto again;
	}
	return cpu_resume_res;
}

static void __ref do_core_ctl(struct cluster_data *cluster)
{
	unsigned int need;
	struct cpumask cpu_pause_res;
	struct cpumask cpu_resume_res;
	bool pause_resume = 0;

	cpumask_clear(&cpu_pause_res);
	cpumask_clear(&cpu_resume_res);
	need = apply_limits(cluster, cluster->need_cpus);

	if (adjustment_possible(cluster, need)) {
		core_ctl_debug("%s: Trying to adjust cluster %u from %u to %u\n",
				TAG, cluster->cluster_id, cluster->active_cpus, need);

		/* Avoid hotplug change online mask */
		cpu_hotplug_disable();
		if (cluster->active_cpus > need){
			cpu_pause_res = try_to_pause(cluster, need);
			pause_resume = 0;
		} else if (cluster->active_cpus < need){
			cpu_resume_res = try_to_resume(cluster, need);
			pause_resume = 1;
		}

		if (!pause_resume){
			if (!cpumask_empty(&cpu_pause_res)){
				pr_info("[Core Pause] pause_res=0x%lx, online=0x%lx, act=0x%lx, paused=0x%lx\n",
					cpu_pause_res.bits[0], cpu_online_mask->bits[0],
					cpu_active_mask->bits[0], cpu_pause_mask->bits[0]);
			}
		} else {
			if (!cpumask_empty(&cpu_resume_res)){
				pr_info("[Core Pause] resume_res=0x%lx, online=0x%lx, act=0x%lx, paused=0x%lx\n",
					cpu_resume_res.bits[0], cpu_online_mask->bits[0],
					cpu_active_mask->bits[0], cpu_pause_mask->bits[0]);
			}
		}
		cpu_hotplug_enable();
	} else
		core_ctl_debug("%s: failed to adjust cluster %u from %u to %u. (min = %u, max = %u)\n",
				TAG, cluster->cluster_id, cluster->active_cpus, need,
				cluster->min_cpus, cluster->max_cpus);

	if (periodic_debug_enable > DISABLE_DEBUG)
		mod_delayed_work(system_power_efficient_wq,
				&periodic_debug, 0);
}

static int __ref try_core_ctl(void *data)
{
	struct cluster_data *cluster = data;
	unsigned long flags;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&cluster->pending_lock, flags);
		if (!cluster->pending) {
			spin_unlock_irqrestore(&cluster->pending_lock, flags);
			schedule();
			if (kthread_should_stop())
				break;
			spin_lock_irqsave(&cluster->pending_lock, flags);
		}
		set_current_state(TASK_RUNNING);
		cluster->pending = false;
		spin_unlock_irqrestore(&cluster->pending_lock, flags);

		do_core_ctl(cluster);
	}

	return 0;
}

static int cpu_pause_cpuhp_state(unsigned int cpu,  bool online)
{
	struct cpu_data *state = &per_cpu(cpu_state, cpu);
	struct cluster_data *cluster = state->cluster;
	unsigned int need;
	bool do_wakeup = false, resume = false;
	unsigned long flags;
	struct cpumask cpu_resume_req;
	unsigned int pause_cpus;

	if (unlikely(!cluster || !cluster->inited))
		return 0;

	spin_lock_irqsave(&core_ctl_state_lock, flags);

	if (online)
		cluster->active_cpus = get_active_cpu_count(cluster);
	else {
		pause_cpus = cpumask_weight(cpu_pause_mask);
		if (pause_cpus > 0){
			if (!test_disable_cpu(cpu)) {
				spin_unlock_irqrestore(&core_ctl_state_lock, flags);
				return -EINVAL;
			}
		}

		/*
		* When CPU is offline, CPU should be un-isolated.
		* Thus, un-isolate this CPU that is going down if
		* it was isolated by core_ctl.
		*/
		if (state->paused_by_cc) {
			state->paused_by_cc = false;
			cluster->nr_paused_cpus--;
			resume = true;
		}
		state->cpu_util_pct = 0;
		state->force_paused = false;
		cluster->active_cpus = get_active_cpu_count(cluster);
	}

	need = apply_limits(cluster, cluster->need_cpus);
	do_wakeup = adjustment_possible(cluster, need);
	spin_unlock_irqrestore(&core_ctl_state_lock, flags);

	/* cpu is inactive */
	if (resume) {
		cpumask_clear(&cpu_resume_req);
		cpumask_set_cpu(cpu, &cpu_resume_req);
		resume_cpus(&cpu_resume_req);
	}
	if (do_wakeup)
		wake_up_core_ctl_thread(cluster);
	return 0;
}

static int core_ctl_prepare_online_cpu(unsigned int cpu)
{
	return cpu_pause_cpuhp_state(cpu, true);
}

static int core_ctl_prepare_dead_cpu(unsigned int cpu)
{
	return cpu_pause_cpuhp_state(cpu, false);
}

static bool test_disable_cpu(unsigned int cpu)
{
	struct cpumask disable_mask;
	unsigned int disable_cpus;

	cpumask_clear(&disable_mask);
	cpumask_complement(&disable_mask, cpu_online_mask);
	cpumask_or(&disable_mask, &disable_mask, cpu_pause_mask);
	cpumask_set_cpu(cpu, &disable_mask);
	cpumask_and(&disable_mask, &disable_mask, cpu_possible_mask);
	disable_cpus = cpumask_weight(&disable_mask);
	if (disable_cpus > (nr_cpu_ids-2))
		return false;

	return true;
}

static struct cluster_data *find_cluster_by_first_cpu(unsigned int first_cpu)
{
	unsigned int i;

	for (i = 0; i < num_clusters; ++i) {
		if (cluster_state[i].first_cpu == first_cpu)
			return &cluster_state[i];
	}

	return NULL;
}

static void periodic_debug_handler(struct work_struct *work)
{
	struct cluster_data *cluster;
	unsigned int index = 0;
	unsigned int max_cpus[MAX_CLUSTERS];
	unsigned int min_cpus[MAX_CLUSTERS];

	if (periodic_debug_enable == DISABLE_DEBUG)
		return;

	for_each_cluster(cluster, index) {
		max_cpus[index] = cluster->max_cpus;
		min_cpus[index] = cluster->min_cpus;
	}

	trace_core_ctl_periodic_debug_handler(enable_policy, max_cpus, min_cpus,
			cpumask_bits(cpu_online_mask)[0], cpumask_bits(cpu_pause_mask)[0]);
	mod_delayed_work(system_power_efficient_wq,
			&periodic_debug, msecs_to_jiffies(periodic_debug_delay));
}

static void core_ctl_call_notifier(unsigned int cpu, unsigned int is_pause)
{
	struct core_ctl_notif_data ndata = {0};
	struct notifier_block *nb;

	nb = rcu_dereference_raw(core_ctl_notifier.head);
	if(!nb)
		return;

	ndata.cpu = cpu;
	ndata.is_pause = is_pause;
	ndata.paused_mask = cpu_pause_mask->bits[0];
	ndata.online_mask = cpu_online_mask->bits[0];
	atomic_notifier_call_chain(&core_ctl_notifier, is_pause, &ndata);

	trace_core_ctl_call_notifier(cpu, is_pause, cpumask_bits(cpu_online_mask)[0], cpumask_bits(cpu_pause_mask)[0]);
}

/* ==================== init section ======================== */

static int ppm_data_init(struct cluster_data *cluster);

static int cluster_init(const struct cpumask *mask)
{
	struct device *dev;
	unsigned int first_cpu = cpumask_first(mask);
	struct cluster_data *cluster;
	struct cpu_data *state;
	unsigned int cpu;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
	int ret = 0;

	/* first_cpu is defined */
	if (find_cluster_by_first_cpu(first_cpu))
		return ret;

	dev = get_cpu_device(first_cpu);
	if (!dev)
		return -ENODEV;


	core_ctl_debug("%s: Creating CPU group %d\n", TAG, first_cpu);

	if (num_clusters == MAX_CLUSTERS) {
		pr_info("%s: Unsupported number of clusters. Only %u supported\n",
				TAG, MAX_CLUSTERS);
		return -EINVAL;
	}
	cluster = &cluster_state[num_clusters];
	cluster->cluster_id = num_clusters;
	++num_clusters;

	cpumask_copy(&cluster->cpu_mask, mask);
	cluster->num_cpus = cpumask_weight(mask);
	if (cluster->num_cpus > MAX_CPUS_PER_CLUSTER) {
		pr_info("%s: HW configuration not supported\n", TAG);
		return -EINVAL;
	}
	cluster->first_cpu = first_cpu;
	cluster->min_cpus = 1;
	cluster->max_cpus = cluster->num_cpus;
	cluster->need_cpus = cluster->num_cpus;
	cluster->offline_throttle_ms = 100;
	cluster->enable = true;
	cluster->nr_down = 0;
	cluster->nr_up = 0;
	cluster->nr_assist = 0;

	cluster->min_cpus = default_min_cpus[cluster->cluster_id];

	if (cluster->cluster_id ==
			(arch_get_nr_clusters() - 1))
		cluster->up_thres = INT_MAX;
	else
		cluster->up_thres =
			get_over_threshold(cluster->cluster_id);

	if (cluster->cluster_id == 0)
		cluster->down_thres = INT_MAX;
	else
		cluster->down_thres =
			get_over_threshold(cluster->cluster_id - 1);

	if (cluster->cluster_id == AB_CLUSTER_ID) {
		cluster->thermal_degree_thres = 65000;
		cluster->thermal_up_thres = INT_MAX;
	} else {
		cluster->thermal_degree_thres = INT_MAX;
		cluster->thermal_up_thres = 80;
	}

	ret = ppm_data_init(cluster);
	if (ret)
		pr_info("initialize ppm data failed ret = %d", ret);

	cluster->nr_not_preferred_cpus = 0;
	spin_lock_init(&cluster->pending_lock);

	for_each_cpu(cpu, mask) {
		core_ctl_debug("%s: Init CPU%u state\n", TAG, cpu);
		state = &per_cpu(cpu_state, cpu);
		state->cluster = cluster;
		state->cpu = cpu;
	}

	cluster->cpu_busy_up_thres = 80;
	cluster->cpu_busy_down_thres = 60;

	cluster->next_offline_time =
		ktime_to_ms(ktime_get()) + cluster->offline_throttle_ms;
	cluster->active_cpus = get_active_cpu_count(cluster);

	cluster->core_ctl_thread = kthread_run(try_core_ctl, (void *) cluster,
			"core_ctl_v3/%d", first_cpu);
	if (IS_ERR(cluster->core_ctl_thread))
		return PTR_ERR(cluster->core_ctl_thread);

	sched_setscheduler_nocheck(cluster->core_ctl_thread, SCHED_FIFO,
			&param);

	cluster->inited = true;

	kobject_init(&cluster->kobj, &ktype_core_ctl);
	return kobject_add(&cluster->kobj, &dev->kobj, "core_ctl");
}

static inline int get_opp_count(struct cpufreq_policy *policy)
{
	int opp_nr;
	struct cpufreq_frequency_table *freq_pos;

	cpufreq_for_each_entry_idx(freq_pos, policy->freq_table, opp_nr);
	return opp_nr;
}

#define NORMAL_TEMP	45
#define THERMAL_TEMP	85

/*
 * x1: BL cap
 * y1: BL eff
 * x2: B  cap
 * y2, B  eff
 */
bool check_eff_precisely(unsigned int x1,
			 unsigned long y1,
			 unsigned int x2,
			 unsigned long y2)
{
	unsigned int diff;
	unsigned long new_y1 = 0;

	diff = (unsigned int)div64_u64(x2 * 100, x1);
	new_y1 = (unsigned long)div64_u64(y1 * diff, 100);
	return y2 < new_y1;
}

static unsigned int find_turn_point(struct cluster_data *c1,
				    struct cluster_data *c2,
				    bool is_thermal)
{
	int i, j;
	bool changed = false;
	bool stop_going = false;
	unsigned int turn_point = 0;

	/* BLCPU */
	for (i = 0; i < c1->ppm_data.opp_nr - 1; i++) {
		changed = false;
		/* BCPU */
		for (j = c2->ppm_data.opp_nr - 1; j >= 0; j--) {
			unsigned long c1_eff, c2_eff;

			if (c2->ppm_data.ppm_tbl[j].capacity <
					c1->ppm_data.ppm_tbl[i].capacity)
				continue;

			c1_eff = is_thermal ? c1->ppm_data.ppm_tbl[i].thermal_eff
				: c1->ppm_data.ppm_tbl[i].eff;
			c2_eff = is_thermal ? c2->ppm_data.ppm_tbl[j].thermal_eff
				: c2->ppm_data.ppm_tbl[j].eff;
			if (c2_eff < c1_eff ||
					check_eff_precisely(
						c1->ppm_data.ppm_tbl[i].capacity, c1_eff,
						c2->ppm_data.ppm_tbl[j].capacity, c2_eff)) {
				turn_point = c2->ppm_data.ppm_tbl[j].capacity;
				changed = true;
				/*
				 * If lowest capacity of BCPU is more efficient than
				 * any capacity of BLCPU, we should not need to find
				 * further.
				 */
				if (j == c2->ppm_data.opp_nr - 1)
					stop_going = true;
			}
			break;
		}
		if (!changed)
			break;
		if (stop_going)
			break;
	}
	return turn_point;
}

static int update_ppm_eff(void)
{
	int opp_nr, first_cpu, cid, i;
	struct cluster_data *cluster;
	struct ppm_table *ppm_tbl;

	for(cid=0; cid<MAX_CLUSTERS; cid++) {
		cluster = &cluster_state[cid];
		first_cpu = cluster->first_cpu;

		if (!cluster->ppm_data.init)
			continue;

		/* false: gearless, true: legacy */
		opp_nr = pd_freq2opp(first_cpu, 0, true, 0) + 1;
		ppm_tbl = cluster->ppm_data.ppm_tbl;

		for(i=0; i<opp_nr; i++) {
			cluster->ppm_data.ppm_tbl[i].eff = calc_eff_hook(first_cpu,
				i, NORMAL_TEMP, ppm_tbl[i].power, ppm_tbl[i].capacity);
			cluster->ppm_data.ppm_tbl[i].thermal_eff = calc_eff_hook(first_cpu,
				i, THERMAL_TEMP, ppm_tbl[i].power, ppm_tbl[i].capacity);
		}

		/* calculate turning point */
		if (cid == AB_CLUSTER_ID) {
			struct cluster_data *prev_cluster = &cluster_state[cid - 1];
			unsigned int turn_point = 0;

			/* thermal case */
			turn_point = find_turn_point(prev_cluster, cluster, true);
			if (!turn_point)
				turn_point = prev_cluster->ppm_data.ppm_tbl[0].capacity;

			if (turn_point) {
				unsigned int val = 0;

				val = div64_u64(turn_point * 100,
					prev_cluster->ppm_data.ppm_tbl[0].capacity);
				if (val <= 100)
					prev_cluster->thermal_up_thres = val;
			}
		}
	}
	return 0;
}

static int ppm_data_init(struct cluster_data *cluster)
{
	struct cpufreq_policy *policy;
	struct ppm_table *ppm_tbl;
	struct em_perf_domain *pd;
	struct em_perf_state *ps;
	int opp_nr, first_cpu, i;
	int cid = cluster->cluster_id;

	first_cpu = cluster->first_cpu;
	policy = cpufreq_cpu_get(first_cpu);
	if (!policy) {
		pr_info("%s: cpufreq policy is not found for cpu#%d",
				TAG, first_cpu);
		return -ENOMEM;
	}

	opp_nr = get_opp_count(policy);
	ppm_tbl = kcalloc(opp_nr, sizeof(struct ppm_table), GFP_KERNEL);

	cluster->ppm_data.ppm_tbl = ppm_tbl;
	if (!cluster->ppm_data.ppm_tbl) {
		pr_info("%s: Failed to allocate ppm_table for cluster %d",
				TAG, cluster->cluster_id);
		cpufreq_cpu_put(policy);
		return -ENOMEM;
	}

	pd = em_cpu_get(first_cpu);
	if (!pd)
		return -ENOMEM;

	/* get power and capacity and calculate efficiency */
	for (i = 0; i < opp_nr; i++) {
		ps = &pd->em_table->state[opp_nr-1-i];
		ppm_tbl[i].power = ps->power;
		ppm_tbl[i].freq = ps->frequency;
		ppm_tbl[i].capacity = pd_get_opp_capacity_legacy(first_cpu, i);
		ppm_tbl[i].thermal_eff = ppm_tbl[i].eff = div64_u64(ppm_tbl[i].power, ppm_tbl[i].capacity);
	}

	cluster->ppm_data.ppm_tbl = ppm_tbl;
	cluster->ppm_data.opp_nr = opp_nr;
	cluster->ppm_data.init = 1;
	cpufreq_cpu_put(policy);

	/* calculate turning point */
	if (cid == AB_CLUSTER_ID) {
		struct cluster_data *prev_cluster = &cluster_state[cid - 1];
		unsigned int turn_point = 0;

		/* thermal case */
		turn_point = find_turn_point(prev_cluster, cluster, true);
		if (!turn_point)
			turn_point = prev_cluster->ppm_data.ppm_tbl[0].capacity;

		if (turn_point) {
			unsigned int val = 0;

			val = div64_u64(turn_point * 100,
				prev_cluster->ppm_data.ppm_tbl[0].capacity);
			if (val <= 100)
				prev_cluster->thermal_up_thres = val;
			pr_info("%s: thermal_turn_pint is %u, thermal_down_thre is change to %u",
				 TAG, turn_point, val);
		}
	}
	return 0;
}

static unsigned long core_ctl_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static int core_ctl_show(struct seq_file *m, void *v)
{
	return 0;
}

static int core_ctl_open(struct inode *inode, struct file *file)
{
	return single_open(file, core_ctl_show, inode->i_private);
}

static long core_ioctl_impl(struct file *filp,
		unsigned int cmd, unsigned long arg, void *pKM)
{
	ssize_t ret = 0;
	void __user *ubuf = (struct _CORE_CTL_PACKAGE *)arg;
	struct _CORE_CTL_PACKAGE msgKM = {0};
	bool bval;
	unsigned int val;

	switch (cmd) {
	case CORE_CTL_FORCE_PAUSE_CPU:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;

		bval = !!msgKM.is_pause;
		ret = core_ctl_force_pause_cpu(msgKM.cpu, bval);
		break;
	case CORE_CTL_SET_OFFLINE_THROTTLE_MS:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		ret = core_ctl_set_offline_throttle_ms(msgKM.cid, msgKM.throttle_ms);
		break;
	case CORE_CTL_SET_LIMIT_CPUS:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		ret = core_ctl_set_limit_cpus(msgKM.cid, msgKM.min, msgKM.max);
		break;
	case CORE_CTL_SET_NOT_PREFERRED:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;

		ret = core_ctl_set_not_preferred(msgKM.not_preferred_cpus);
		break;
	case CORE_CTL_SET_BOOST:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;

		bval = !!msgKM.boost;
		ret = core_ctl_set_boost(bval);
		break;
	case CORE_CTL_SET_UP_THRES:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		ret = core_ctl_set_up_thres(msgKM.cid, msgKM.thres);
		break;
	case CORE_CTL_ENABLE_POLICY:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;

		val = msgKM.enable_policy;
		ret = core_ctl_enable_policy(val);
		break;
	case CORE_CTL_SET_CPU_BUSY_THRES:
		if (core_ctl_copy_from_user(&msgKM, ubuf, sizeof(struct _CORE_CTL_PACKAGE)))
			return -1;
		core_ctl_set_cpu_busy_thres(msgKM.cid, msgKM.thres);
	break;
	default:
		pr_info("%s: %s %d: unknown cmd %x\n",
			TAG, __FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static long core_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	return core_ioctl_impl(filp, cmd, arg, NULL);
}

static const struct proc_ops core_ctl_Fops = {
	.proc_ioctl = core_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.proc_compat_ioctl = core_ioctl,
#endif
	.proc_open = core_ctl_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init core_ctl_init(void)
{
	int ret, cluster_nr, i, ret_error_line, hpstate_dead, hpstate_online;
	struct cpumask cluster_cpus;
	struct proc_dir_entry *pe, *parent;

	hpstate_dead = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
			"core_ctl/cpu_pause:dead",
			NULL, core_ctl_prepare_dead_cpu);
	pr_info("%s: hpstate_dead:%d", TAG, hpstate_dead);
	if (hpstate_dead == -ENOSPC) {
		pr_info("%s: fail to setup cpuhp_dead", TAG);
		ret = -ENOSPC;
		goto failed;
	}

	hpstate_online = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
			"core_ctl/cpu_pause:online",
			core_ctl_prepare_online_cpu, NULL);
	pr_info("%s: hpstate_online:%d", TAG, hpstate_online);
	if (hpstate_online == -ENOSPC) {
		pr_info("%s: fail to setup cpuhp_online", TAG);
		ret = -ENOSPC;
		goto failed;
	}

	ret = init_sched_avg();
	if (ret)
		goto failed_remove_hpstate;

	/* register traceprob */
	ret = register_trace_android_vh_scheduler_tick(core_ctl_tick, NULL);
	if (ret) {
		ret_error_line = __LINE__;
		pr_info("%s: vendor hook register failed ret %d line %d\n",
			TAG, ret, ret_error_line);
		goto failed_exit_sched_avg;
	}

	/* init cluster_data */
	cluster_nr = arch_get_nr_clusters();

	for (i = 0; i < cluster_nr; i++) {
		arch_get_cluster_cpus(&cluster_cpus, i);
		ret = cluster_init(&cluster_cpus);
		if (ret) {
			pr_info("%s: unable to create core ctl group: %d\n", TAG, ret);
			goto failed_deprob;
		}
	}

	/* init core_ctl ioctl */

	pr_info("%s: start to init core_ioctl driver\n", TAG);
	parent = proc_mkdir("cpumgr", NULL);
	pe = proc_create("core_ioctl", 0660, parent, &core_ctl_Fops);
	if (!pe) {
		pr_info("%s: Could not create /proc/cpumgr/core_ioctl.\n", TAG);
		ret = -ENOMEM;
		goto failed_deprob;
	}

	initialized = true;
	return 0;

failed_deprob:
	unregister_trace_android_vh_scheduler_tick(core_ctl_tick, NULL);
failed_exit_sched_avg:
	exit_sched_avg();
	tracepoint_synchronize_unregister();
failed_remove_hpstate:
	cpuhp_remove_state(hpstate_dead);
	cpuhp_remove_state(hpstate_online);
failed:
	return ret;
}

static void __exit core_ctl_exit(void)
{
	exit_sched_avg();
	unregister_trace_android_vh_scheduler_tick(core_ctl_tick, NULL);
	tracepoint_synchronize_unregister();
}

module_init(core_ctl_init);
module_exit(core_ctl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mediatek Inc.");
MODULE_DESCRIPTION("Mediatek core_ctl");
