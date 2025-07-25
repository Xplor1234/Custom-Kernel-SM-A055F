// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */
#include <linux/kobject.h>
#include "ccci_fsm_internal.h"
#include "ccci_fsm_sys.h"
#include "port_rpc.h"
#include "modem_sys.h"
#include "ccci_config.h"

#define CCCI_KOBJ_NAME "md"

struct mdee_info_collect mdee_collect;

void fsm_sys_mdee_info_notify(const char *buf)
{
	spin_lock(&mdee_collect.mdee_info_lock);
	memset(mdee_collect.mdee_info, 0x0, AED_STR_LEN);
	scnprintf(mdee_collect.mdee_info, AED_STR_LEN, "%s", buf);
	spin_unlock(&mdee_collect.mdee_info_lock);
}

static struct kobject fsm_kobj;

struct ccci_attribute {
	struct attribute attr;
	ssize_t (*show)(char *buf);
	ssize_t (*store)(const char *buf, size_t count);
};

#define CCCI_ATTR(_name, _mode, _show, _store)			\
static struct ccci_attribute ccci_attr_##_name = {		\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.show = _show,						\
	.store = _store,					\
}

static void fsm_obj_release(struct kobject *kobj)
{
	CCCI_NORMAL_LOG(-1, FSM, "release fsm kobject\n");
}

static ssize_t ccci_attr_show(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	ssize_t len = 0;

	struct ccci_attribute *a
		= container_of(attr, struct ccci_attribute, attr);

	if (a->show)
		len = a->show(buf);

	return len;
}

static ssize_t ccci_attr_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	ssize_t len = 0;

	struct ccci_attribute *a
		= container_of(attr, struct ccci_attribute, attr);

	if (a->store)
		len = a->store(buf, count);

	return len;
}

static const struct sysfs_ops fsm_sysfs_ops = {
	.show = ccci_attr_show,
	.store = ccci_attr_store,
};

static ssize_t ccci_mdee_info_show(char *buf)
{
	int curr = 0;

	spin_lock(&mdee_collect.mdee_info_lock);
	curr = scnprintf(buf, AED_STR_LEN, "%s\n", mdee_collect.mdee_info);
	spin_unlock(&mdee_collect.mdee_info_lock);

	return curr;
}

#if IS_ENABLED(CONFIG_MTK_ECCCI_DEBUG_LOG)
CCCI_ATTR(ecid, 0444, &port_rpc_ecid_show, NULL);
#endif

CCCI_ATTR(mdee, 0444, &ccci_mdee_info_show, NULL);

/* Sys -- Add to group */
static struct attribute *ccci_default_attrs[] = {
	&ccci_attr_mdee.attr,
#if IS_ENABLED(CONFIG_MTK_ECCCI_DEBUG_LOG)
	&ccci_attr_ecid.attr,
#endif
	NULL
};
ATTRIBUTE_GROUPS(ccci_default);

static struct kobj_type fsm_ktype = {
	.release = fsm_obj_release,
	.sysfs_ops = &fsm_sysfs_ops,
	.default_groups = ccci_default_groups,
};

int fsm_sys_init(void)
{
	int ret = 0;

	spin_lock_init(&mdee_collect.mdee_info_lock);

	ret = kobject_init_and_add(&fsm_kobj, &fsm_ktype,
			kernel_kobj, CCCI_KOBJ_NAME);
	if (ret < 0) {
		kobject_put(&fsm_kobj);
		CCCI_ERROR_LOG(-1, FSM, "fail to add fsm kobject\n");
		return ret;
	}
#ifdef MTK_TC10_FEATURE_MD_EE_INFO
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	ret = mrdump_mini_add_extra_file((unsigned long)mdee_collect.mdee_info,
			0, AED_STR_LEN, "MD_EE_INFO");
	if (ret)
		CCCI_ERROR_LOG(0, FSM, "%s: MD_EE_INFO add fail\n", __func__);
#endif
#endif
	return ret;
}

