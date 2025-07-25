// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature : sysfs-nodes
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */

#ifndef __UFS_SEC_SYSFS_H__
#define __UFS_SEC_SYSFS_H__

#include "ufs-sec-feature.h"

#include <linux/sec_class.h>
#include <linux/sec_debug.h>

/* SEC error info : begin */
/* UFSHCD UIC layer error flags : in ufshcd.c */
enum {
	UFSHCD_UIC_DL_PA_INIT_ERROR = (1 << 0), /* Data link layer error */
	UFSHCD_UIC_DL_NAC_RECEIVED_ERROR = (1 << 1), /* Data link layer error */
	UFSHCD_UIC_DL_TCx_REPLAY_ERROR = (1 << 2), /* Data link layer error */
	UFSHCD_UIC_NL_ERROR = (1 << 3), /* Network layer error */
	UFSHCD_UIC_TL_ERROR = (1 << 4), /* Transport Layer error */
	UFSHCD_UIC_DME_ERROR = (1 << 5), /* DME error */
	UFSHCD_UIC_PA_GENERIC_ERROR = (1 << 6), /* Generic PA error */
};

struct SEC_UFS_op_cnt {
	unsigned int HW_RESET_cnt;
	unsigned int link_startup_cnt;
	unsigned int Hibern8_enter_cnt;
	unsigned int Hibern8_exit_cnt;
	unsigned int AH8_err_cnt;
	unsigned int op_err;
};

struct SEC_UFS_UIC_cmd_cnt {
	u8 DME_GET_err;
	u8 DME_SET_err;
	u8 DME_PEER_GET_err;
	u8 DME_PEER_SET_err;
	u8 DME_POWERON_err;
	u8 DME_POWEROFF_err;
	u8 DME_ENABLE_err;
	u8 DME_RESET_err;
	u8 DME_END_PT_RST_err;
	u8 DME_LINK_STARTUP_err;
	u8 DME_HIBER_ENTER_err;
	u8 DME_HIBER_EXIT_err;
	u8 DME_TEST_MODE_err;
	unsigned int UIC_cmd_err;
};

struct SEC_UFS_UIC_err_cnt {
	u8 PAERR_cnt;
	u8 DL_PA_INIT_ERR_cnt;
	u8 DL_NAC_RCVD_ERR_cnt;
	u8 DL_TC_REPLAY_ERR_cnt;
	u8 DL_FC_PROTECT_ERR_cnt;
	u8 NLERR_cnt;
	u8 TLERR_cnt;
	u8 DMEERR_cnt;
	unsigned int DLERR_cnt;
	unsigned int UIC_err;
	unsigned int PAERR_linereset;
	unsigned int PAERR_lane[3];
};

struct SEC_UFS_Fatal_err_cnt {
	u8 DFE; // Device_Fatal
	u8 CFE; // Controller_Fatal
	u8 SBFE; // System_Bus_Fatal
	u8 CEFE; // Crypto_Engine_Fatal
	u8 LLE; // Link Lost
	unsigned int Fatal_err;
};

struct SEC_UFS_UTP_cnt {
	u8 UTMR_query_task_cnt;
	u8 UTMR_abort_task_cnt;
	u8 UTMR_logical_reset_cnt;
	u8 UTR_read_err;
	u8 UTR_write_err;
	u8 UTR_sync_cache_err;
	u8 UTR_unmap_err;
	u8 UTR_etc_err;
	unsigned int UTP_err;
};

struct SEC_UFS_QUERY_cnt {
	u8 NOP_err;
	u8 R_Desc_err;
	u8 W_Desc_err;
	u8 R_Attr_err;
	u8 W_Attr_err;
	u8 R_Flag_err;
	u8 Set_Flag_err;
	u8 Clear_Flag_err;
	u8 Toggle_Flag_err;
	unsigned int Query_err;
};

struct SEC_SCSI_SENSE_cnt {
	unsigned int scsi_medium_err;
	unsigned int scsi_hw_err;
};

struct ufs_sec_err_info {
	struct SEC_UFS_op_cnt op_cnt;
	struct SEC_UFS_UIC_cmd_cnt UIC_cmd_cnt;
	struct SEC_UFS_UIC_err_cnt UIC_err_cnt;
	struct SEC_UFS_Fatal_err_cnt Fatal_err_cnt;
	struct SEC_UFS_UTP_cnt UTP_cnt;
	struct SEC_UFS_QUERY_cnt Query_cnt;
	struct SEC_SCSI_SENSE_cnt sense_cnt;
};

#define get_vdi_member(member) ufs_sec_features.vdi->member
#define get_err_member(member) ufs_sec_features.ufs_err->member

#define SEC_UFS_DATA_ATTR_RO(name, fmt, args...)                                         \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr, char *buf) \
{                                                                                        \
	return sprintf(buf, fmt, args);                                                  \
}                                                                                        \
static DEVICE_ATTR_RO(name)

#define SEC_UFS_ERR_CNT_INC(count, max) ((count) += ((count) < (max)) ? 1 : 0)

#define SEC_UFS_OP_ERR_CNT_INC(member, max) ({	\
		struct SEC_UFS_op_cnt *op_cnt = &get_err_member(op_cnt);	\
										\
		SEC_UFS_ERR_CNT_INC(op_cnt->member, max);	\
		SEC_UFS_ERR_CNT_INC(op_cnt->op_err, UINT_MAX);	\
	})

#define SEC_UFS_ERR_CNT_ADD(count, value, max) \
		((count) += (count < max) ? (((count + value) < (max)) ? value : (max - count)) : 0)
/* SEC error info : end */

void ufs_sec_add_sysfs_nodes(struct ufs_hba *hba);
void ufs_sec_remove_sysfs_nodes(struct ufs_hba *hba);

extern struct ufs_sec_feature_info ufs_sec_features;

#endif
