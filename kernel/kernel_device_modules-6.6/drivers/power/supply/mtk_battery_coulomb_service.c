// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include "mtk_battery.h"

void wake_up_gauge_coulomb(struct mtk_battery *gm)
{
	unsigned long flags = 0;
	struct mtk_coulomb_service *cs;

	cs = &gm->cs;

	if (cs == NULL || cs->init == false) {
		bm_debug(gm, "%s %s error %d %d\n",
			cs->name,
			__func__,
			cs == NULL,
			cs->init == false);
		return;
	}

	bm_debug(gm, "%s %s %d %d\n",
		cs->name,
		__func__,
		cs->wlock->active,
		cs->coulomb_thread_timeout);

	mutex_lock(&cs->hw_coulomb_lock);
	gauge_set_property(gm, GAUGE_PROP_COULOMB_HT_INTERRUPT, 300);
	gauge_set_property(gm, GAUGE_PROP_COULOMB_LT_INTERRUPT, 300);
	mutex_unlock(&cs->hw_coulomb_lock);
	spin_lock_irqsave(&cs->slock, flags);
	if (cs->wlock->active == 0)
		__pm_stay_awake(cs->wlock);
	spin_unlock_irqrestore(&cs->slock, flags);

	cs->coulomb_thread_timeout = true;
	wake_up(&cs->wait_que);
	bm_debug(gm, "[%s] over\n", __func__);
}

void gauge_coulomb_consumer_init(struct gauge_consumer *coulomb,
	struct device *dev, char *name)
{
	coulomb->name = name;
	INIT_LIST_HEAD(&coulomb->list);
	coulomb->dev = dev;
}

void gauge_coulomb_dump_list(struct mtk_battery *gm)
{
	struct list_head *pos;
	struct list_head *phead;
	struct gauge_consumer *ptr;
	int car = 0;
	struct mtk_coulomb_service *cs;

	cs = &gm->cs;
	if (cs->init == false)
		return;
	bm_debug(gm, "%s %s %d %d\n",
		cs->name,
		__func__,
		cs->wlock->active,
		cs->coulomb_thread_timeout);

	phead = &cs->coulomb_head_plus;
	mutex_lock(&cs->coulomb_lock);
	gauge_get_property(gm, GAUGE_PROP_COULOMB, &car);
	if (list_empty(phead) != true) {
		bm_debug(gm, "dump plus list start\n");
		list_for_each(pos, phead) {
			ptr = container_of(pos, struct gauge_consumer, list);
			bm_debug(gm,
				"%s +dump list name:%s start:%ld end:%ld car:%d int:%d\n",
				cs->name,
				ptr->name,
			ptr->start, ptr->end, car, ptr->variable);
		}
	}

	phead = &cs->coulomb_head_minus;
	if (list_empty(phead) != true) {
		bm_debug(gm, "dump minus list start\n");
		list_for_each(pos, phead) {
			ptr = container_of(pos, struct gauge_consumer, list);
			bm_debug(gm,
				"%s -dump list name:%s start:%ld end:%ld car:%d int:%d\n",
				cs->name,
				ptr->name,
			ptr->start, ptr->end, car, ptr->variable);
		}
	}
	mutex_unlock(&cs->coulomb_lock);
}

void gauge_coulomb_before_reset(struct mtk_battery *gm)
{
	struct mtk_coulomb_service *cs;
	int val = 0;

	cs = &gm->cs;

	if (cs->init == false) {
		bm_err(gm, "[%s %s]gauge_coulomb service is not rdy\n",
			cs->name,
			__func__);
		return;
	}
	mutex_lock(&cs->coulomb_lock);
	mutex_lock(&cs->hw_coulomb_lock);
	gauge_set_property(gm, GAUGE_PROP_COULOMB_HT_INTERRUPT, 0);
	gauge_set_property(gm, GAUGE_PROP_COULOMB_LT_INTERRUPT, 0);
	mutex_unlock(&cs->hw_coulomb_lock);
	mutex_unlock(&cs->coulomb_lock);

	gauge_get_property(gm, GAUGE_PROP_COULOMB, &val);
	cs->reset_coulomb = val;
	bm_err(gm, "%s %s car=%ld\n",
		cs->name,
		__func__,
		cs->reset_coulomb);
	gauge_coulomb_dump_list(gm);
}

void gauge_coulomb_after_reset(struct mtk_battery *gm)
{
	struct list_head *pos;
	struct list_head *phead;
	struct gauge_consumer *ptr;
	unsigned long now;
	unsigned long duraction;
	struct mtk_coulomb_service *cs;

	cs = &gm->cs;

	if (cs->init == false)
		return;
	bm_err(gm, "%s %s\n", cs->name, __func__);
	now = cs->reset_coulomb;
	mutex_lock(&cs->coulomb_lock);

	/* check plus list */
	phead = &cs->coulomb_head_plus;
	list_for_each(pos, phead) {
		ptr = container_of(pos, struct gauge_consumer, list);

		ptr->start = 0;
		duraction = ptr->end - now;
		ptr->end = duraction;
		ptr->variable = duraction;
		bm_debug(gm, "[%s %s]+ %s %ld %ld %d\n",
			cs->name,
			__func__,
			ptr->name,
		ptr->start, ptr->end, ptr->variable);
	}

	/* check minus list */
	phead = &cs->coulomb_head_minus;
	list_for_each(pos, phead) {
		ptr = container_of(pos, struct gauge_consumer, list);

		ptr->start = 0;
		duraction = ptr->end - now;
		ptr->end = duraction;
		ptr->variable = duraction;
		bm_debug(gm, "[%s %s]- %s %ld %ld %d\n",
			cs->name,
			__func__,
			ptr->name,
		ptr->start, ptr->end, ptr->variable);
	}

	mutex_unlock(&cs->coulomb_lock);
	gauge_coulomb_dump_list(gm);
	wake_up_gauge_coulomb(gm);
}

void gauge_coulomb_start(struct mtk_battery *gm,
	struct gauge_consumer *coulomb, int car)
{
	struct list_head *pos;
	struct list_head *phead;
	struct gauge_consumer *ptr = NULL;
	int hw_car, now_car;
	bool wake = false;
	int car_now;
	int val = 0;
	struct mtk_coulomb_service *cs;

	cs = &gm->cs;

	if (car == 0)
		return;

	if (cs->init == false) {
		cs = NULL;
		return;
	}

	mutex_lock(&cs->coulomb_lock);
	gauge_get_property(gm, GAUGE_PROP_COULOMB, &val);
	car_now = val;
	/* del from old list */
	if (list_empty(&coulomb->list) != true) {
		bm_debug(gm, "%s coulomb_start del name:%s s:%ld e:%ld v:%d car:%d\n",
		cs->name,
		coulomb->name,
		coulomb->start, coulomb->end, coulomb->variable, car_now);
		list_del_init(&coulomb->list);
	}

	coulomb->start = car_now;
	coulomb->end = coulomb->start + car;
	coulomb->variable = car;
	now_car = coulomb->start;

	if (car > 0)
		phead = &cs->coulomb_head_plus;
	else
		phead = &cs->coulomb_head_minus;

	/* add node to list */
	list_for_each(pos, phead) {
		ptr = container_of(pos, struct gauge_consumer, list);
		if (car > 0) {
			if (coulomb->end < ptr->end)
				break;
		} else
			if (coulomb->end > ptr->end)
				break;
	}
	list_add(&coulomb->list, pos->prev);

	if (car > 0) {
		list_for_each(pos, phead) {
			ptr = container_of(pos, struct gauge_consumer, list);
			if (ptr->end - now_car <= 0)
				wake = true;
			else
				break;
		}
		hw_car = ptr->end - now_car;
		mutex_lock(&cs->hw_coulomb_lock);
		gauge_set_property(gm, GAUGE_PROP_COULOMB_HT_INTERRUPT, hw_car);
		mutex_unlock(&cs->hw_coulomb_lock);
	} else {
		list_for_each(pos, phead) {
			ptr = container_of(pos, struct gauge_consumer, list);
			if (ptr->end - now_car >= 0)
				wake = true;
			else
				break;
		}
		hw_car = now_car - ptr->end;
		mutex_lock(&cs->hw_coulomb_lock);
		gauge_set_property(gm, GAUGE_PROP_COULOMB_LT_INTERRUPT, hw_car);
		mutex_unlock(&cs->hw_coulomb_lock);
	}
	mutex_unlock(&cs->coulomb_lock);

	if (wake == true)
		wake_up_gauge_coulomb(gm);

	bm_debug(gm, "%s %s dev:%s name:%s s:%ld e:%ld v:%d car:%d w:%d\n",
	cs->name, __func__,
	dev_name(coulomb->dev), coulomb->name, coulomb->start, coulomb->end,
	coulomb->variable, car, wake);
}

void gauge_coulomb_stop(struct mtk_battery *gm, struct gauge_consumer *coulomb)
{
	struct mtk_coulomb_service *cs;

	cs = &gm->cs;

	if (cs->init == false) {
		cs = NULL;
		return;
	}

	bm_debug(gm, "%s %s name:%s %ld %ld %d\n",
	cs->name,
	__func__,
	coulomb->name, coulomb->start, coulomb->end,
	coulomb->variable);

	mutex_lock(&cs->coulomb_lock);
	list_del_init(&coulomb->list);
	mutex_unlock(&cs->coulomb_lock);

}

static void gauge_coulomb_int_handler(struct mtk_coulomb_service *cs)
{
	int car = 0, hw_car;
	struct list_head *pos;
	struct list_head *phead;
	struct gauge_consumer *ptr = NULL;
	struct mtk_battery *gm =
			container_of(cs, struct mtk_battery, cs);

	gauge_get_property(cs->gm, GAUGE_PROP_COULOMB, &car);
	bm_debug(gm, "[%s]%s car:%d preCar:%d\n",
		cs->name, __func__,
		car, cs->pre_coulomb);

	if (list_empty(&cs->coulomb_head_plus) != true) {
		pos = cs->coulomb_head_plus.next;
		phead = &cs->coulomb_head_plus;
		for (pos = phead->next; pos != phead;) {
			struct list_head *ptmp;

			ptr = container_of(pos, struct gauge_consumer, list);
			if (ptr->end <= car) {
				ptmp = pos;
				pos = pos->next;
				list_del_init(ptmp);
				bm_debug(gm,
					"[%s %s]+ %s s:%ld e:%ld car:%d %d int:%d timeout\n",
					cs->name,
					__func__,
					ptr->name,
					ptr->start, ptr->end, car,
					cs->pre_coulomb, ptr->variable);
				if (ptr->callback) {
					mutex_unlock(&cs->coulomb_lock);
					ptr->callback(cs->gm, ptr);
					mutex_lock(&cs->coulomb_lock);
					pos = cs->coulomb_head_plus.next;
				}
			} else
				break;
		}

		if (list_empty(&cs->coulomb_head_plus) != true) {
			pos = cs->coulomb_head_plus.next;
			ptr = container_of(pos, struct gauge_consumer, list);
			hw_car = ptr->end - car;
			bm_debug(gm,
				"[%s %s]+ %s %ld %ld %d now:%d dif:%d\n",
				cs->name,
				__func__,
					ptr->name,
					ptr->start, ptr->end,
					ptr->variable, car, hw_car);
			mutex_lock(&cs->hw_coulomb_lock);
			gauge_set_property(cs->gm, GAUGE_PROP_COULOMB_HT_INTERRUPT,
				hw_car);
			mutex_unlock(&cs->hw_coulomb_lock);
		} else
			bm_debug(gm, "%s + list is empty\n", cs->name);
	} else
		bm_debug(gm, "%s + list is empty\n", cs->name);

	if (list_empty(&cs->coulomb_head_minus) != true) {
		pos = cs->coulomb_head_minus.next;
		phead = &cs->coulomb_head_minus;
		for (pos = phead->next; pos != phead;) {
			struct list_head *ptmp;

			ptr = container_of(pos, struct gauge_consumer, list);
			if (ptr->end >= car) {
				ptmp = pos;
				pos = pos->next;
				list_del_init(ptmp);
				bm_debug(gm,
					"[%s %s]- %s s:%ld e:%ld car:%d %d int:%d timeout\n",
					cs->name,
					__func__,
					ptr->name,
					ptr->start, ptr->end,
					car, cs->pre_coulomb, ptr->variable);
				if (ptr->callback) {
					mutex_unlock(&cs->coulomb_lock);
					ptr->callback(cs->gm, ptr);
					mutex_lock(&cs->coulomb_lock);
					pos = cs->coulomb_head_minus.next;
				}

			} else
				break;
		}

		if (list_empty(&cs->coulomb_head_minus) != true) {
			pos = cs->coulomb_head_minus.next;
			ptr = container_of(pos, struct gauge_consumer, list);
			hw_car = car - ptr->end;
			bm_debug(gm,
				"[%s %s]- %s %ld %ld %d now:%d dif:%d\n",
				cs->name,
				__func__,
				ptr->name,
				ptr->start, ptr->end,
				ptr->variable, car, hw_car);
			mutex_lock(&cs->hw_coulomb_lock);
			gauge_set_property(cs->gm, GAUGE_PROP_COULOMB_LT_INTERRUPT,
				hw_car);
			mutex_unlock(&cs->hw_coulomb_lock);
		} else
			bm_debug(gm, "%s - list is empty\n", cs->name);
	} else
		bm_debug(gm, "%s - list is empty\n", cs->name);

	cs->pre_coulomb = car;
}

static int gauge_coulomb_thread(void *arg)
{
	struct mtk_coulomb_service *cs = (struct mtk_coulomb_service *)arg;
	unsigned long flags = 0;
	ktime_t start, end, duraction;
	int ret = 0;
	struct mtk_battery *gm =
		container_of(cs, struct mtk_battery, cs);

	bm_debug(gm, "[%s]%s:=>\n", cs->name, __func__);
	while (1) {
		ret = wait_event_interruptible(cs->wait_que,
			cs->coulomb_thread_timeout == true &&
			!atomic_read(&cs->in_sleep));

		if (atomic_read(&cs->in_sleep) || ret < 0) {
			__pm_relax(cs->wlock);
			continue;
		}

		cs->coulomb_thread_timeout = false;
		start = ktime_get_boottime();

		mutex_lock(&cs->coulomb_lock);
		gauge_coulomb_int_handler(cs);
		mutex_unlock(&cs->coulomb_lock);

		spin_lock_irqsave(&cs->slock, flags);
		__pm_relax(cs->wlock);
		spin_unlock_irqrestore(&cs->slock, flags);

		end = ktime_get_boottime();
		duraction = end - start;

		bm_debug(gm, "%s time:%d ms\n", __func__,
			(int)(div_s64(duraction, 1000000)));
	}

	return 0;
}

static irqreturn_t coulomb_irq(int irq, void *data)
{
	struct mtk_battery *gm = data;
	if (gm->is_probe_done == false || gm->bm == NULL) {
		bm_err(gm, "[%s]battery probe is not rdy:%d\n",
			__func__, gm->is_probe_done);
		return IRQ_HANDLED;
	}

	if (fg_interrupt_check(gm) == false)
		return IRQ_HANDLED;

	disable_gauge_irq(gm->gauge, COULOMB_H_IRQ);
	disable_gauge_irq(gm->gauge, COULOMB_L_IRQ);
	wake_up_bat_irq_controller(&gm->irq_ctrl, COULOMB_FLAG);

	return IRQ_HANDLED;
}

static int system_pm_notify(struct notifier_block *nb,
			    unsigned long mode, void *_unused)
{
	struct mtk_coulomb_service *cs =
			container_of(nb, struct mtk_coulomb_service, pm_nb);
	struct mtk_battery *gm =
			container_of(cs, struct mtk_battery, cs);

	bm_err(gm, "%s %s mode1:%lu\n", cs->name, __func__, mode);
	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:
		if (!mutex_trylock(&cs->coulomb_lock)) {
			bm_err(gm, "%s %s NOTIFY_BAD:%lu\n", cs->name, __func__, mode);
			return NOTIFY_BAD;
		}
		atomic_set(&cs->in_sleep, 1);
		mutex_unlock(&cs->coulomb_lock);
		bm_err(gm, "%s %s normal:%lu\n", cs->name, __func__, mode);
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		atomic_set(&cs->in_sleep, 0);
		if (cs->init)
			wake_up(&cs->wait_que);
		bm_err(gm, "%s %s %d %lu\n", cs->name, __func__, cs->init, mode);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

void gauge_coulomb_service_init(struct mtk_battery *gm)
{
	int val = 0;
	struct mtk_coulomb_service *cs;
	int ret = 0;

	bm_debug(gm, "[%s] into\n", __func__);
	cs = &gm->cs;
	cs->gm = gm;
	ret = snprintf(cs->name, 20, "%s gct", gm->gauge->name);
	if (ret < 0)
		bm_err(gm, "[%s] something wrong\n", __func__);


	INIT_LIST_HEAD(&cs->coulomb_head_minus);
	INIT_LIST_HEAD(&cs->coulomb_head_plus);
	mutex_init(&cs->coulomb_lock);
	mutex_init(&cs->hw_coulomb_lock);
	spin_lock_init(&cs->slock);
	cs->wlock = wakeup_source_register(NULL, "gauge coulomb wakelock");
	init_waitqueue_head(&cs->wait_que);
	atomic_set(&cs->in_sleep, 0);
	kthread_run(gauge_coulomb_thread, cs, "%s", cs->name);

	cs->pm_nb.notifier_call = system_pm_notify;
	ret = register_pm_notifier(&cs->pm_nb);
	if (ret)
		bm_err(gm, "failed to register system pm notify\n");

	ret = devm_request_threaded_irq(&gm->gauge->pdev->dev,
	gm->gauge->irq_no[COULOMB_H_IRQ],
	NULL, coulomb_irq,
	IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
	"mtk_gauge_coulomb_high",
	gm);
	//disable_irq_nosync(gm->gauge->coulomb_h_irq);
	if (ret)
		bm_err(gm, "failed to request coulomb h irq\n");

	ret = devm_request_threaded_irq(&gm->gauge->pdev->dev,
	gm->gauge->irq_no[COULOMB_L_IRQ],
	NULL, coulomb_irq,
	IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
	"mtk_gauge_coulomb_low",
	gm);
	//disable_irq_nosync(gm->gauge->coulomb_l_irq);
	if (ret)
		bm_err(gm, "failed to request coulomb l irq\n");

	gauge_get_property(gm, GAUGE_PROP_COULOMB, &val);
	cs->pre_coulomb = val;
	cs->init = true;
}
