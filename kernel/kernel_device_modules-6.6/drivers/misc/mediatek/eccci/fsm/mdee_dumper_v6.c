// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/timer.h>
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#include <soc/mediatek/emi.h>
#if IS_ENABLED(CONFIG_MTK_EMI)
#include <soc/mediatek/smpu.h>
#endif

#include "mdee_dumper_v6.h"
#include "ccci_config.h"
#include "ccci_fsm_sys.h"
#include "md_sys1_platform.h"
#include "modem_sys.h"
#include "ccci_fsm_internal.h"

#ifndef DB_OPT_DEFAULT
#define DB_OPT_DEFAULT    (0)	/* Dummy macro define to avoid build error */
#endif

#ifndef DB_OPT_FTRACE
#define DB_OPT_FTRACE     (0)	/* Dummy macro define to avoid build error */
#endif

static void ccci_aed_v6(struct ccci_fsm_ee *mdee, unsigned int dump_flag,
	char *aed_str, int db_opt)
{
	void *ex_log_addr = NULL;
	int ex_log_len = 0;
	void *md_dump_addr = NULL;
	int md_dump_len = 0;
	int info_str_len = 0;
	char *buff;		/*[AED_STR_LEN]; */
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	char buf_fail[] = "Fail alloc mem for exception\n";
#endif
	char *img_inf = NULL;
	struct mdee_dumper_v6 *dumper = mdee->dumper_obj;
	struct ccci_modem *md = ccci_get_modem();
	struct ccci_smem_region *mdss_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDSS_DBG);
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	struct ccci_per_md *per_md_data = ccci_get_per_md_data();
	int md_dbg_dump_flag = 0;

	if(per_md_data != NULL)
		md_dbg_dump_flag = per_md_data->md_dbg_dump_flag;
	else
		CCCI_ERROR_LOG(0, FSM, "Error: %s per_md_data is NULL\n", __func__);
#endif

	if(mdss_dbg == NULL)
		CCCI_ERROR_LOG(0, FSM, "Error: %s mdss_dbg is NULL\n", __func__);
	buff = kmalloc(AED_STR_LEN, GFP_ATOMIC);
	if (buff == NULL) {
		CCCI_ERROR_LOG(0, FSM, "Fail alloc Mem for buff!\n");
		goto err_exit1;
	}
	img_inf = ccci_get_md_info_str();
	if (img_inf == NULL)
		img_inf = "";
	info_str_len = strlen(aed_str);
	info_str_len += strlen(img_inf);

	if (info_str_len > AED_STR_LEN)
		/* Cut string length to AED_STR_LEN */
		buff[AED_STR_LEN - 1] = '\0';
	ret = snprintf(buff, AED_STR_LEN, "md1:%s%s%s",
		aed_str, mdee->ex_start_time, img_inf);
	if (ret < 0 || ret >= AED_STR_LEN) {
		CCCI_ERROR_LOG(0, FSM,
			"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
		goto err_exit1;
	}
	memset(mdee->ex_start_time, 0x0, sizeof(mdee->ex_start_time));
	/* MD ID must sync with aee_dump_ccci_debug_info() */
err_exit1:
	if ((dump_flag & CCCI_AED_DUMP_CCIF_REG) && (mdss_dbg != NULL)) {
		ex_log_addr = mdss_dbg->base_ap_view_vir;
		ex_log_len = mdss_dbg->size;
		ccci_md_dump_info(DUMP_FLAG_CCIF_REG | DUMP_FLAG_CCIF,
			mdss_dbg->base_ap_view_vir + CCCI_EE_OFFSET_CCIF_SRAM,
			CCCI_EE_SIZE_CCIF_SRAM);
	}
	if ((dump_flag & CCCI_AED_DUMP_EX_MEM) && (mdss_dbg != NULL)) {
		ex_log_addr = mdss_dbg->base_ap_view_vir;
		ex_log_len = mdss_dbg->size;
		if (md && md->hw_info && md->hw_info->md_l2sram_base) {
			md_dump_addr = md->hw_info->md_l2sram_base;
			if (ccci_get_ap_plat() == 6991)
				md_dump_len = md->hw_info->md_l2sram_size;
			else
				md_dump_len = MD_L2SRAM_SIZE;
		}
	}
	if (dump_flag & CCCI_AED_DUMP_EX_PKT) {
		ex_log_addr = (void *)dumper->ex_pl_info;
		ex_log_len = MD_HS1_FAIL_DUMP_SIZE;
	}
	if (buff == NULL) {
		fsm_sys_mdee_info_notify(aed_str);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		if (md_dump_len)
			md_cd_lock_modem_clock_src(1);
		if (md_dbg_dump_flag & (1U << MD_DBG_DUMP_SMEM))
			aed_md_exception_api(ex_log_addr, ex_log_len,
				md_dump_addr, md_dump_len, buf_fail, db_opt);
		else
			aed_md_exception_api(NULL, 0, md_dump_addr,
				md_dump_len, buf_fail, db_opt);
		if (md_dump_len)
			md_cd_lock_modem_clock_src(0);
#endif
	} else {
		fsm_sys_mdee_info_notify(aed_str);
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		if (md_dump_len)
			md_cd_lock_modem_clock_src(1);

		if (md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM))
			aed_md_exception_api(ex_log_addr, ex_log_len,
				md_dump_addr, md_dump_len, buff, db_opt);
		else
			aed_md_exception_api(NULL, 0, md_dump_addr,
				md_dump_len, buff, db_opt);
		if (md_dump_len)
			md_cd_lock_modem_clock_src(0);
#endif
		kfree(buff);
	}
}

static char mdee_more_inf_str[MD_EE_CASE_WDT + 1][64] = {
	"",
	"\nOnly SWINT case\n",
	"\nOnly EX case\n",
	"\n[Others] MD long time no response\n",
	"\n[Others] MD watchdog timeout interrupt\n"
};

static void mdee_output_debug_info_to_buf(struct ccci_fsm_ee *mdee,
	struct debug_info_t *debug_info, char *ex_info)
{
	struct ccci_mem_layout *mem_layout = NULL;
	char *ex_info_temp = NULL;
	int ret = 0;
	int val = 0;

	switch (debug_info->type) {
	case MD_EX_CLASS_ASSET:
		/* assert: file name+line number+code*3 */
		ret = snprintf(ex_info, EE_BUF_LEN_UMOLY,
			"(%s)\n[%s] file:%s line:%d\np1:0x%08x\np2:0x%08x\np3:0x%08x\n\n",
			debug_info->core_name, debug_info->name,
			debug_info->dump_assert.file_name,
			debug_info->dump_assert.line_num,
			debug_info->dump_assert.parameters[0],
			debug_info->dump_assert.parameters[1],
			debug_info->dump_assert.parameters[2]);
		CCCI_ERROR_LOG(0, FSM, "filename = %s\n",
			debug_info->dump_assert.file_name);
		CCCI_ERROR_LOG(0, FSM, "line = %d\n",
			debug_info->dump_assert.line_num);
		CCCI_ERROR_LOG(0, FSM,
			"assert para0 = 0x%08x, para1 = 0x%08x, para2 = 0x%08x\n",
			debug_info->dump_assert.parameters[0],
			debug_info->dump_assert.parameters[1],
			debug_info->dump_assert.parameters[2]);
		break;
	case MD_EX_CLASS_FATAL:
		/* fatal:  */
		ret = snprintf(ex_info, EE_BUF_LEN_UMOLY,
			 "(%s)%s\n[%s] err_code1:0x%08X err_code2:0x%08X err_code3:0x%08X\n%s%s\n%s\n",
			debug_info->core_name,
			debug_info->dump_fatal.err_sec, debug_info->name,
			debug_info->dump_fatal.err_code1,
			debug_info->dump_fatal.err_code2,
			debug_info->dump_fatal.err_code3,
			debug_info->dump_fatal.offender,
			debug_info->dump_fatal.ExStr,
			debug_info->dump_fatal.fatal_fname);
		ex_info_temp = kmalloc(EE_BUF_LEN_UMOLY, GFP_ATOMIC);
		if (ex_info_temp == NULL) {
			CCCI_ERROR_LOG(0, FSM,
				"Fail alloc Mem for ex_info_temp!\n");
			break;
		}
		val = snprintf(ex_info_temp, EE_BUF_LEN_UMOLY, "%s", ex_info);
		if (val < 0 || val >= EE_BUF_LEN_UMOLY)
			CCCI_ERROR_LOG(0, FSM,
				"%s-%d:snprintf fail,val = %d\n", __func__, __LINE__, val);
		if (debug_info->dump_fatal.err_code1 == 0x3104) {
			mem_layout = ccci_md_get_mem();
			if (mem_layout == NULL) {
				CCCI_ERROR_LOG(-1, FSM, "ccci_md_get_mem fail\n");
				kfree(ex_info_temp);
				return;
			}
			val = snprintf(ex_info, EE_BUF_LEN_UMOLY,
			"%s\n%s\n%s\nMD base = 0x%08X\n\n", ex_info_temp,
			mdee->ex_mpu_string, mdee->ex_smpu_string,
			(unsigned int)mem_layout->md_bank0.base_ap_view_phy);
			if (val < 0 || val >= EE_BUF_LEN_UMOLY)
				CCCI_ERROR_LOG(0, FSM,
					"%s-%d:snprintf fail,val = %d\n", __func__, __LINE__, val);
			memset(mdee->ex_mpu_string, 0x0, sizeof(mdee->ex_mpu_string));
			memset(mdee->ex_smpu_string, 0x0, sizeof(mdee->ex_smpu_string));
		}
		CCCI_ERROR_LOG(0, FSM,
			"fatal error code 1,2,3 = [0x%08X, 0x%08X, 0x%08X]%s\n",
			debug_info->dump_fatal.err_code1,
			debug_info->dump_fatal.err_code2,
			debug_info->dump_fatal.err_code3,
			debug_info->dump_fatal.offender);
		kfree(ex_info_temp);
		break;
	case MD_EX_CLASS_CUSTOM:
		/* fatal:  */
		ret = snprintf(ex_info, EE_BUF_LEN_UMOLY,
			 "(%s)%s\n[%s] err_code1:0x%08X err_code2:0x%08X err_code3:0x%08X\n%s%s\n%s\nP1:0x%08X\nP2:0x%08X\nP3:0x%08X\n",
			debug_info->core_name,
			debug_info->dump_fatal.err_sec, debug_info->name,
			debug_info->ex_type,
			debug_info->dump_fatal.error_address,
			debug_info->dump_fatal.error_pc,
			debug_info->dump_fatal.offender,
			debug_info->dump_fatal.ExStr,
			debug_info->dump_fatal.fatal_fname,
			debug_info->dump_fatal.err_code1,
			debug_info->dump_fatal.err_code2,
			debug_info->dump_fatal.err_code3);
		CCCI_ERROR_LOG(0, FSM,
			"fatal custom error code 1,2,3 = [0x%08X, 0x%08X, 0x%08X]%s\n",
			debug_info->dump_fatal.err_code1,
			debug_info->dump_fatal.err_code2,
			debug_info->dump_fatal.err_code3,
			debug_info->dump_fatal.offender);
		break;
	default:
		ret = snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s\n[%s]\n",
			debug_info->core_name, debug_info->name);
		break;
	}
	if (ret < 0 || ret >= EE_BUF_LEN_UMOLY) {
		CCCI_ERROR_LOG(0, FSM,
			"%s-%d:snprintf fail,ret = %d,case_id = %d\n",
			__func__, __LINE__, ret, debug_info->type);
		return;
	}

}

static void mdee_info_dump_v6(struct ccci_fsm_ee *mdee)
{
	char *ex_info = NULL; /* aed api par4 */
	char *ex_info_temp = NULL;
	int db_opt = (DB_OPT_DEFAULT | DB_OPT_FTRACE); /* aed api par5 */
	int dump_flag = 0;
	char *i_bit_ex_info = NULL;
	char buf_fail[] = "Fail alloc mem for exception\n";
	struct ccci_modem *md = ccci_get_modem();
	struct mdee_dumper_v6 *dumper = mdee->dumper_obj;
	struct debug_info_t *debug_info = &dumper->debug_info;
	struct ccci_smem_region *mdccci_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDCCCI_DBG);
	struct ccci_smem_region *mdss_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDSS_DBG);
	struct ccci_per_md *per_md_data = ccci_get_per_md_data();
	int md_dbg_dump_flag = 0;
	int ret = 0;

	if(per_md_data != NULL)
		md_dbg_dump_flag = per_md_data->md_dbg_dump_flag;
	else
		CCCI_ERROR_LOG(0, FSM, "Error: %s per_md_data is NULL\n", __func__);
	ex_info = kmalloc(AED_STR_LEN, GFP_ATOMIC);
	if (ex_info == NULL) {
		CCCI_ERROR_LOG(0, FSM, "Fail alloc Mem for ex_info!\n");
		goto err_exit;
	}
	memset(ex_info, 0, AED_STR_LEN);
	ex_info_temp = kzalloc(AED_STR_LEN, GFP_ATOMIC);
	if (ex_info_temp == NULL) {
		CCCI_ERROR_LOG(0, FSM, "Fail alloc Mem for ex_info_temp!\n");
		goto err_exit;
	}

	switch (dumper->more_info) {
	case MD_EE_CASE_ONLY_EX:
	case MD_EE_CASE_ONLY_SWINT:
		mdee_output_debug_info_to_buf(mdee, debug_info, ex_info);
		ret = snprintf(ex_info_temp, EE_BUF_LEN_UMOLY, "%s", ex_info);
		if (ret < 0 || ret >= EE_BUF_LEN_UMOLY) {
			CCCI_ERROR_LOG(0, FSM,
				"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
			goto err_exit;
		}
		ret = snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info_temp,
			mdee_more_inf_str[dumper->more_info]);
		if (ret < 0 || ret >= EE_BUF_LEN_UMOLY) {
			CCCI_ERROR_LOG(0, FSM, "%s-%d:snprintf fail,ret = %d\n",
				__func__, __LINE__, ret);
			goto err_exit;
		}
		break;
	case MD_EE_CASE_NO_RESPONSE:
		/* use strncpy, otherwise if this happens after a MD EE,
		 * the former EE info will be printed out
		 */
		db_opt |= (unsigned int)DB_OPT_FTRACE;
		fallthrough;
	case MD_EE_CASE_WDT:
		ret = snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s",
			mdee_more_inf_str[dumper->more_info]);
		if (ret < 0 || ret >= EE_BUF_LEN_UMOLY) {
			CCCI_ERROR_LOG(0, FSM, "%s-%d:snprintf fail,ret = %d\n",
				__func__, __LINE__, ret);
			goto err_exit;
		}
		break;
	default:
		mdee_output_debug_info_to_buf(mdee, debug_info, ex_info);
		break;
	}

	if (debug_info->ELM_status != NULL) {
		ret = snprintf(ex_info_temp, EE_BUF_LEN_UMOLY, "%s", ex_info);
		if (ret < 0 || ret >= EE_BUF_LEN_UMOLY) {
			CCCI_ERROR_LOG(0, FSM,
				"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
			goto err_exit;
		}
		ret = snprintf(ex_info, EE_BUF_LEN_UMOLY, "%s%s", ex_info_temp,
			debug_info->ELM_status);/* ELM status */
		if (ret < 0 || ret >= EE_BUF_LEN_UMOLY) {
			CCCI_ERROR_LOG(0, FSM,
				"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
			goto err_exit;
		}
	}

	CCCI_MEM_LOG_TAG(0, FSM, "Dump MD EX log, 0x%x, 0x%x\n",
		dumper->more_info, debug_info->par_data_source);
	if (debug_info->par_data_source == MD_EE_DATA_IN_GPD) {
		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
			dumper->ex_pl_info, MD_HS1_FAIL_DUMP_SIZE);
		/* MD will not fill in share memory
		 * before we send runtime data
		 */
		dump_flag = CCCI_AED_DUMP_EX_PKT;
	} else if (md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM)) {
		dump_flag = CCCI_AED_DUMP_EX_MEM;
		if (mdccci_dbg != NULL)
			ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
				mdccci_dbg->base_ap_view_vir, mdccci_dbg->size);
		if (mdss_dbg != NULL)
			ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
				mdss_dbg->base_ap_view_vir, mdss_dbg->size);
		if (md && md->hw_info && md->hw_info->md_l2sram_base) {
			md_cd_lock_modem_clock_src(1);
			if (ccci_get_ap_plat() == 6991)
				ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
					md->hw_info->md_l2sram_base, md->hw_info->md_l2sram_size);
			else
				ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
					md->hw_info->md_l2sram_base, MD_L2SRAM_SIZE);

			md_cd_lock_modem_clock_src(0);
		}
	}

err_exit:
	/* update here to maintain handshake stage info
	 * during exception handling
	 */
	if (debug_info->ex_type == CC_C2K_EXCEPTION)
		CCCI_NORMAL_LOG(0, FSM, "C2K EE, No need trigger DB\n");
	else if (ex_info == NULL)
		ccci_aed_v6(mdee, dump_flag, buf_fail, db_opt);
	else
		ccci_aed_v6(mdee, dump_flag, ex_info, db_opt);

	kfree(ex_info);
	kfree(ex_info_temp);
	kfree(i_bit_ex_info);

}

static void strmncopy(char *src, char *dst, int src_len, int dst_len)
{
	int temp_m, temp_n, temp_i;

	temp_m = src_len - 1;
	temp_n = dst_len - 1;
	temp_n = (temp_m > temp_n) ? temp_n : temp_m;
	for (temp_i = 0; temp_i < temp_n; temp_i++) {
		dst[temp_i] = src[temp_i];
		if (dst[temp_i] == 0x00)
			break;
	}
}

static struct ex_overview_t *md_ee_get_buf_ptr(struct ccci_fsm_ee *mdee,
	u8 *buf_type)
{
	unsigned int ccif_sram[CCCI_EE_SIZE_CCIF_SRAM/sizeof(unsigned int)]
	= { 0 };
	struct mdee_dumper_v6 *dumper = mdee->dumper_obj;
	struct ccci_smem_region *mdss_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDSS_DBG);
	struct ex_overview_t *tar_ptr = NULL;

	ccci_md_dump_info(DUMP_FLAG_CCIF, ccif_sram, 0);

	if (((dumper->more_info == MD_EE_CASE_NORMAL)
		&& (mdee->ee_info_flag&MD_EE_DUMP_IN_GPD))
		|| ((ccif_sram[1] < 0x208) && (ccif_sram[1] >= 0x200))) {
		*buf_type = MD_EE_DATA_IN_GPD;
		tar_ptr = (struct ex_overview_t *)dumper->ex_pl_info;
		CCCI_NORMAL_LOG(0, FSM,
			"parsing data from: GPD %p, %p\n",
			tar_ptr, dumper->ex_pl_info);
	} else {
		if (mdss_dbg != NULL)
			tar_ptr = (struct ex_overview_t *)mdss_dbg->base_ap_view_vir;
		*buf_type = MD_EE_DATA_IN_SMEM;
	}
	return tar_ptr;

}

static int mdee_set_core_name(char *core_name,
	struct ex_overview_t *ex_overview)
{
	int core_id;
	u8 temp_sys_inf_1, temp_sys_inf_2;
	struct ex_brief_maininfo_v6 *brief_info = NULL;
	char core_name_temp[MD_CORE_NAME_DEBUG];
	int ret = 0;

	for (core_id = 0; core_id < ex_overview->core_num; core_id++) {
		if (ex_overview->main_reson[core_id].is_offender) {
			strmncopy(ex_overview->main_reson[core_id].core_name,
			core_name,
			sizeof(ex_overview->main_reson[core_id].core_name) + 1,
			MD_CORE_NAME_DEBUG);
			break; /* just show one core name */
		}
	}

	if (core_id == 0) {
		brief_info =  &ex_overview->ex_info;
		temp_sys_inf_1 = brief_info->system_info1;
		temp_sys_inf_2 = brief_info->system_info2;
		ret = snprintf(core_name_temp,
			MD_CORE_NAME_DEBUG, "%s", core_name);
		if (ret < 0 || ret >= MD_CORE_NAME_DEBUG) {
			CCCI_ERROR_LOG(0, FSM,
				"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
		}
		ret = snprintf(core_name, MD_CORE_NAME_DEBUG,
			"%s_core%d,vpe%d,tc%d(VPE%d)", core_name_temp,
			(temp_sys_inf_1/5), (temp_sys_inf_1%5),
			temp_sys_inf_2, temp_sys_inf_1);
		if (ret < 0 || ret >= MD_CORE_NAME_DEBUG) {
			CCCI_ERROR_LOG(0, FSM,
				"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
		}
	}
	CCCI_NORMAL_LOG(0, FSM,
		"brief_info: core_name = %s", core_name);
	return core_id;
}

static char *ee_type_str_v6_23h[] = {
	[INTERRUPT_EXCEPTION] = "Fatal error(INTERRUPT)",
	[TLB_MOD_EXCEPTION] = "Fatal error(TLB_MOD)",
	[TLB_MISS_LOAD_EXCEPTION] = "Fatal error(TLB_MISS_LOAD)",
	[TLB_MISS_STORE_EXCEPTION] = "Fatal error(TLB_MISS_STORE)",
	[ADDRESS_ERROR_LOAD_EXCEPTION] = "Fatal error(ADDRESS_ERROR_LOAD)",
	[ADDRESS_ERROR_STORE_EXCEPTION] = "Fatal error(ADDRESS_ERROR_STORE)",
	[INSTR_BUS_ERROR] = "Fatal error(INSTR_BUS_ERROR)",
	[DATA_BUS_ERROR] = "Fatal error(DATA_BUS_ERROR)",
	[SYSTEM_CALL_EXCEPTION] = "Fatal error(SYSTEM_CALL)",
	[BREAKPOINT_EXCEPTION] = "Fatal error(BREAKPOINT)",
	[RESERVED_INSTRUCTION_EXCEPTION] = "Fatal error(RESERVED_INSTRUCTION)",
	[COPROCESSORS_UNUSABLE_EXCEPTION] =
		"Fatal error(COPROCESSORS_UNUSABLE)",
	[INTEGER_OVERFLOW_EXCEPTION] = "Fatal error(INTEGER_OVERFLOW)",
	[TRAP_EXCEPTION] = "Fatal error(TRAP)",
	[MSA_FLOATING_POINT_EXCEPTION] = "Fatal error(MSA_FLOATING_POINT)",
	[FLOATING_POINT_EXCEPTION] = "Fatal error(FLOATING_POINT)",
	[COPROCESSOR_2_IS_1_EXCEPTION] = "Fatal error(COPROCESSOR_2_IS_1)",
	[COR_EXTEND_UNUSABLE_EXCEPTION] = "Fatal error(COR_EXTEND_UNUSABLE)",
	[COPROCESSOR_2_EXCEPTION] = "Fatal error(COPROCESSOR_2)",
	[TLB_READ_INHIBIT_EXCEPTION] = "Fatal error(TLB_READ_INHIBIT)",
	[TLB_EXECUTE_INHIBIT_EXCEPTION] = "Fatal error(TLB_EXECUTE_INHIBIT)",
	[MSA_UNUSABLE_EXCEPTION] = "Fatal error(MSA_UNUSABLE)",
	[MDMX_EXCEPTION] = "Fatal error(MDMX)",
	[WATCH_EXCEPTION] = "Fatal error(WATCH)",
	[MCHECK_EXCEPTION] = "Fatal error(MCHECK)",
	[THREAD_EXCEPTION] = "Fatal error(THREAD)",
	[DSP_UNUSABLE_EXCEPTION] = "Fatal error(DSP_UNUSABLE)",
	[RESERVED_27_EXCEPTION] = "Fatal error(RESERVED_27)",
	[RESERVED_28_EXCEPTION] = "Fatal error(RESERVED_28)",
	[MPU_NOT_ALLOW] = "Fatal error(MPU_NOT_ALLOW)",
	[CACHE_ERROR_EXCEPTION_DBG_MODE] = "Fatal error(CACHE_ERROR_DBG_MODE)",
	[RESERVED_31_EXCEPTION] = "Fatal error(RESERVED_31)",
	[NMI_EXCEPTION] = "Fatal error(NMI)",
	[CACHE_ERROR_EXCEPTION] = "Fatal error(CACHE_ERROR)",
	[TLB_REFILL_LOAD_EXCEPTION] = "Fatal error(TLB_REFILL_LOAD)",
	[TLB_REFILL_STORE_EXCEPTION] = "Fatal error(TLB_REFILL_STORE)"
};

static char *ee_type_str_v6_30s[] = {
	/*[STACKACCESS_EXCEPTION - 0x30] = */"Fatal error(STACKACCESS)",
	/*[SYS_FATALERR_EXT_TASK_EXCEPTION - 0x30] = */"Fatal error(task)",
	/*[SYS_FATALERR_EXT_BUF_EXCEPTION - 0x30] = */"Fatal error(buf)"
};

static char *ee_type_str_v6_50s[] = {
	/*[ASSERT_FAIL_EXCEPTION - 0x50] = */"ASSERT",
	/*[ASSERT_DUMP_EXTENDED_RECORD - 0x50] = */"ASSERT",
	/*[ASSERT_FAIL_NATIVE - 0x50] = */"ASSERT",
	/*[ASSERT_CUSTOM_ADDR - 0x50] = */"Fatal error(ASSERT_CUSTOM_ADDR)",
	/*[ASSERT_CUSTOM_MODID - 0x50] = */"Fatal error(ASSERT_CUSTOM_MODID)",
	/*[ASSERT_CUSTOM_MOFID - 0x50] = */"Fatal error(ASSERT_CUSTOM_MOFID)"
};

static char *ee_type_str_v6_60s[] = {
	/*[CC_INVALID_EXCEPTION - 0x60] = */"CC_INVALID",
	/*[CC_CS_EXCEPTION- 0x60] = */"Fatal error (CC_CS)",
	/*[CC_MD32_EXCEPTION- 0x60] = */"Fatal error (CC_MD32)",
	/*[CC_C2K_EXCEPTION- 0x60] = */"Fatal error (CC_C2K)",
	/*[CC_VOLTE_EXCEPTION- 0x60] = */"Fatal error (CC_VOLTE)",
	/*[CC_USIP_EXCEPTION- 0x60] = */"Fatal error (CC_USIP)",
	/*[CC_SCQ_EXCEPTION- 0x60] = */"Fatal error (CC_SCQ)",
	/*[CC_SONIC_EXCEPTION- 0x60] = */"Fatal error (CC_SONIC)",
};

static char *ee_err_sec_str[] = {
	"",
	"BRP_LTE_ROCODE",
	"BRP_FDD_ROCODE",
	"FEC_TX_C2K_ROCODE",
	"FEC_TX_WCDMA_ROCODE",
	"FEC_TX_LTE_ROCODE",
	"FEC_RX_C2K_ROCODE",
	"FEC_RX_WCDMA_ROCODE",
	"SCQ16_LTE_ROCODE",
	"SCQ16_FDD_ROCODE",
	"SCQ16_TDD_ROCODE",
	"SCQ16_C2K_ROCODE",
	"RAKE_FDD_ROCODE",
	"RAKE_C2K_ROCODE",
};

static void md_ee_set_exp_type(struct ex_brief_maininfo_v6 *brief_info,
	int core_id, char **ex_str)
{
	u32 exp_type = brief_info->ex_type;

	if (core_id == 0) {
		if (exp_type < TLB_REFILL_MAX_NUM)
			*ex_str = ee_type_str_v6_23h[exp_type];
		else if ((exp_type < SYS_FATALERR_MAX_NUM)
				&& (exp_type >= STACKACCESS_EXCEPTION))
			*ex_str = ee_type_str_v6_30s[exp_type -
				STACKACCESS_EXCEPTION];
		else if ((exp_type < ASSERT_FAIL_MAX_NUM)
				&& (exp_type >= ASSERT_FAIL_EXCEPTION))
			*ex_str = ee_type_str_v6_50s[exp_type -
				ASSERT_FAIL_EXCEPTION];
		else if ((exp_type < CC_EXCEPTION_MAX_NUM)
				&& (exp_type >= CC_INVALID_EXCEPTION))
			*ex_str = ee_type_str_v6_60s[exp_type -
				CC_INVALID_EXCEPTION];
		else if (exp_type == EMI_MPU_VIOLATION_EXCEPTION)
			*ex_str = "Fatal error (rmpu violation)";
		else
			*ex_str = "INVALID_EXCEPTION_TYPE";
	} else {
		if (brief_info->maincontent_type == MD_EX_CLASS_ASSET)
			*ex_str = "ASSERT";
		else if (brief_info->maincontent_type == MD_EX_CLASS_FATAL)
			*ex_str = "Fatal error";
		else
			*ex_str = "INVALID_EXCEPTION_TYPE";
	}

}

static void md_ee_set_assert_para(struct ex_assert_v6 *assert_src,
	struct dump_info_assert *assert_tar)
{
	/* 1. assert file name */
	strmncopy(assert_src->filepath, assert_tar->file_name,
		sizeof(assert_src->filepath), sizeof(assert_tar->file_name));
	/* 2. assert line number */
	assert_tar->line_num = assert_src->line_number;
	/* 3. assert parameters */
	assert_tar->parameters[0] = assert_src->para1;
	assert_tar->parameters[1] = assert_src->para2;
	assert_tar->parameters[2] = assert_src->para3;
}

static void md_ee_set_fatal_para(struct ex_fatal_v6 *fatal_src,
	struct dump_info_fatal *fata_tar, unsigned int ex_type)
{
	char temp_str[32] = { 0 };
	char *fatal_fname_prefix = "MD Offending File:";
	unsigned int string_len = 0;
	int ret = 0;

	/* 1. offender string */
	if ((fatal_src->offender[0] != 0xCC)
		&& (fatal_src->offender[0] != 0)) {
		strmncopy(fatal_src->offender, temp_str,
			sizeof(fatal_src->offender), sizeof(temp_str));
		ret = snprintf(fata_tar->offender, sizeof(fata_tar->offender),
			"MD Offender:%s\n", temp_str);
		if (ret < 0 || ret >= sizeof(fata_tar->offender)) {
			CCCI_ERROR_LOG(-1, FSM,
				"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
			return;
		}
		CCCI_NORMAL_LOG(-1, FSM, "offender: %s\n",
			     fata_tar->offender);
	} else
		fata_tar->offender[0] = '\0';
	/* 2. error code of fatal error */
	fata_tar->err_code1 = fatal_src->code1;
	fata_tar->err_code2 = fatal_src->code2;
	fata_tar->err_code3 = fatal_src->code3;
	/* 3. cadefa support */
	if (fatal_src->is_cadefa_supported == 0x01)
		fata_tar->ExStr = "CaDeFa Supported\n";
	else
		fata_tar->ExStr = "";
	/*4. file name support*/
	if (ex_type == MD_EX_CLASS_CUSTOM)
		fatal_fname_prefix = "Original Offending File: ";
	if (fatal_src->is_filename_supported == 0x01) {
		ret = snprintf(fata_tar->fatal_fname, EX_BRIEF_FATALERR_SIZE,
			"%s%s", fatal_fname_prefix, fatal_src->filename);
		if (ret < 0 || ret >= EX_BRIEF_FATALERR_SIZE) {
			CCCI_ERROR_LOG(-1, FSM,
				"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
			return;
		}

		string_len = strlen(fatal_fname_prefix) +
			EX_BRIEF_FATALERR_SIZE -
			sizeof(struct ex_fatal_v6) + 1;
		if (string_len > EX_BRIEF_FATALERR_SIZE)
			CCCI_NORMAL_LOG(-1, FSM,
			"fata fname length should be ajdustment %d > %d\n",
			string_len, EX_BRIEF_FATALERR_SIZE);
		CCCI_NORMAL_LOG(-1, FSM, "%s\n", fata_tar->fatal_fname);
	} else
		fata_tar->fatal_fname[0] = '\0';
	/* 5. error section */
	if (fatal_src->error_section < ARRAY_SIZE(ee_err_sec_str))
		fata_tar->err_sec =
			ee_err_sec_str[fatal_src->error_section];
	else
		fata_tar->err_sec = "";
	fata_tar->error_address = fatal_src->error_address;
	fata_tar->error_pc = fatal_src->error_pc;

}

static void mdee_info_prepare_v6(struct ccci_fsm_ee *mdee)
{
	struct ex_overview_t *ex_overview = NULL;
	struct ex_brief_maininfo_v6 *brief_info = NULL;
	struct mdee_dumper_v6 *dumper = mdee->dumper_obj;
	struct debug_info_t *debug_info = &dumper->debug_info;
	int core_id = 0;
	int ret = 0;

	CCCI_NORMAL_LOG(0, FSM,
		"%s, ee_case(0x%x)\n", __func__, dumper->more_info);

	memset(debug_info, 0, sizeof(struct debug_info_t));
	if (dumper->more_info == MD_EE_CASE_NO_RESPONSE
		|| dumper->more_info == MD_EE_CASE_WDT)
		return;
	/* mem of parsing */
	ex_overview = md_ee_get_buf_ptr(mdee, &debug_info->par_data_source);
	if (ex_overview == NULL) {
		CCCI_ERROR_LOG(0, FSM, "Error: md_ee_get_buf_ptr returned NULL\n");
		return;
	}
	/* version: 0xABxxyyyy:
	 * xx rule version for AP parsing,
	 * yyyy for md parsing
	 */
	 // version check need to check
	if ((ex_overview->overview_verno&0xFFFF0000) != 0xAB000000
		|| (ex_overview->overview_verno&0x0000FFFF) != 6
		|| ex_overview->core_num > MD_CORE_TOTAL_NUM) {
		debug_info->type = MD_EX_CLASS_INVALID;
		debug_info->name = "INVALID_EXCEPTION_TYPE";
		ret = snprintf(debug_info->core_name, sizeof(debug_info->core_name),
			"%s", "MCU_core0,vpe0,tc0(VPE0)");
		if (ret < 0 || ret >= sizeof(debug_info->core_name)) {
			CCCI_ERROR_LOG(0, FSM,
				"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
			return;
		}
		return;
	}
	/* core_name */
	core_id = mdee_set_core_name(debug_info->core_name, ex_overview);
	/* ======== exception type ========= */
	brief_info = &ex_overview->ex_info;
	debug_info->type = brief_info->maincontent_type;
	debug_info->ex_type = brief_info->ex_type;
	md_ee_set_exp_type(brief_info, core_id, &debug_info->name);
	/* ======== exception parameters ========= */
	switch (debug_info->type) {
	case MD_EX_CLASS_ASSET:
		md_ee_set_assert_para(&brief_info->info.assert,
			&debug_info->dump_assert);
		break;
	case MD_EX_CLASS_FATAL:
	case MD_EX_CLASS_CUSTOM:
		md_ee_set_fatal_para(&brief_info->info.fatalerr,
			&debug_info->dump_fatal, debug_info->type);
		break;
	}
	/* ======== ELM status ========= */
	switch (brief_info->elm_status) {
	case 0xFF:
		debug_info->ELM_status = "\nno ELM info\n";
		break;
	case 0xAE:
		debug_info->ELM_status = "\nELM rlat:FAIL\n";
		break;
	case 0xBE:
		debug_info->ELM_status = "\nELM wlat:FAIL\n";
		break;
	case 0xDE:
		debug_info->ELM_status = "\nELM r/wlat:PASS\n";
		break;
	default:
		debug_info->ELM_status = "";
		break;
	}

}

static void mdee_dumper_v6_set_ee_pkg(struct ccci_fsm_ee *mdee,
	char *data, int len)
{
	struct mdee_dumper_v6 *dumper = mdee->dumper_obj;
	int cpy_len =
	len > MD_HS1_FAIL_DUMP_SIZE ? MD_HS1_FAIL_DUMP_SIZE : len;

	memcpy(dumper->ex_pl_info, data, cpy_len);
}

static void md_HS1_Fail_dump(char *ex_info, unsigned int len)
{
	unsigned int reg_value[2] = { 0 };
	unsigned int ccif_sram[CCCI_EE_SIZE_CCIF_SRAM/sizeof(unsigned int)]
	= { 0 };
	int ret = 0;
	u64 boot_status_val = get_expected_boot_status_val();

	ccci_md_dump_info(DUMP_MD_BOOTUP_STATUS, reg_value, 2);
	ccci_md_dump_info(DUMP_FLAG_CCIF, ccif_sram, 0);

	CCCI_MEM_LOG_TAG(0, FSM,
		"md_boot_stats0 /1 / bootuptrace:0x%X / 0x%X / 0x%X\n",
		reg_value[0], reg_value[1], ccif_sram[0]);
	if ((reg_value[0] == 0) && (ccif_sram[0] == 0)) {
		ret = snprintf(ex_info, len,
			"\n[Others] MD_BOOT_UP_FAIL(HS%d - MD poweron failed)\n"
			"boot_status0: 0x%x\nboot_status1: 0x%x\n"
			"MD Offender:DVFS\n",
			0, reg_value[0], reg_value[1]);
	} else if ((reg_value[0] == boot_status_val) ||
				(reg_value[0] == 0) ||
				(reg_value[0] >= 0x53310000 &&
				reg_value[0] <= 0x533100FF)) {
		ret = snprintf(ex_info, len,
			"\n[Others] MD_BOOT_UP_FAIL(HS%d)\n",
			1);
		ccci_md_dump_info(DUMP_FLAG_REG, NULL, 0);
		msleep(10000);
		ccci_md_dump_info(DUMP_FLAG_REG, NULL, 0);
	}  else {
		ret = snprintf(ex_info, len,
			"\n[Others] MD_BOOT_UP_FAIL(HS%d - MD bootrom failed)\n"
			"boot_status0: 0x%x\nboot_status1: 0x%x\n"
			"MD Offender:BOOTROM\n",
			0, reg_value[0], reg_value[1]);
	}
	if (ret < 0 || ret >= len) {
		CCCI_ERROR_LOG(0, FSM,
			"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
		return;
	}

}

static void mdee_dumper_v6_dump_ee_info(struct ccci_fsm_ee *mdee,
	enum MDEE_DUMP_LEVEL level, int more_info)
{
	struct mdee_dumper_v6 *dumper = mdee->dumper_obj;
	struct ccci_modem *md = ccci_get_modem();
	struct ccci_smem_region *mdccci_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDCCCI_DBG);
	struct ccci_smem_region *mdss_dbg =
		ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDSS_DBG);
	int md_state = ccci_fsm_get_md_state();
	char ex_info[EE_BUF_LEN] = {0};
	struct ccci_per_md *per_md_data =
		ccci_get_per_md_data();
	int md_dbg_dump_flag = 0;
	int ret = 0;

	if (per_md_data != NULL)
		md_dbg_dump_flag = per_md_data->md_dbg_dump_flag;
	else
		CCCI_ERROR_LOG(0, FSM, "Error: %s per_md_data is NULL\n", __func__);
	if (mdss_dbg == NULL || mdccci_dbg == NULL)
		CCCI_ERROR_LOG(0, FSM, "Error: %s mdss_dbg is %p, mdccci_dbg is %p\n",
			__func__, mdss_dbg, mdccci_dbg);
	dumper->more_info = more_info;
	if (level == MDEE_DUMP_LEVEL_BOOT_FAIL) {
		if (md_state == BOOT_WAITING_FOR_HS1) {
			md_HS1_Fail_dump(ex_info, EE_BUF_LEN);
			/* Handshake 1 fail */
			ccci_aed_v6(mdee,
			CCCI_AED_DUMP_CCIF_REG | CCCI_AED_DUMP_EX_MEM,
			ex_info, DB_OPT_DEFAULT | DB_OPT_FTRACE);
		} else if (md_state == BOOT_WAITING_FOR_HS2) {
			ret = snprintf(ex_info, EE_BUF_LEN,
				"\n[Others] MD_BOOT_UP_FAIL(HS%d)\n", 2);
			if (ret < 0 || ret >= EE_BUF_LEN) {
				CCCI_ERROR_LOG(0, FSM,
					"%s-%d:snprintf fail,ret = %d\n", __func__, __LINE__, ret);
			}
			/* Handshake 2 fail */
			CCCI_MEM_LOG_TAG(0, FSM, "Dump MD EX log\n");
			if ((md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM)) && (mdss_dbg != NULL) &&
				(mdccci_dbg != NULL)) {
				ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
					mdccci_dbg->base_ap_view_vir,
						mdccci_dbg->size);
				ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
					mdss_dbg->base_ap_view_vir,
						mdss_dbg->size);
				if (md && md->hw_info && md->hw_info->md_l2sram_base) {
					md_cd_lock_modem_clock_src(1);
					if (ccci_get_ap_plat() == 6991)
						ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
							md->hw_info->md_l2sram_base, md->hw_info->md_l2sram_size);
					else
						ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
							md->hw_info->md_l2sram_base, MD_L2SRAM_SIZE);
					md_cd_lock_modem_clock_src(0);
				}

			}

			ccci_aed_v6(mdee,
			CCCI_AED_DUMP_CCIF_REG | CCCI_AED_DUMP_EX_MEM,
			ex_info, DB_OPT_DEFAULT | DB_OPT_FTRACE);
		}
	} else if (level == MDEE_DUMP_LEVEL_STAGE1) {
		CCCI_MEM_LOG_TAG(0, FSM, "Dump MD EX log\n");
		if ((md_dbg_dump_flag & (1 << MD_DBG_DUMP_SMEM)) && (mdss_dbg != NULL) && (mdccci_dbg != NULL)) {
			ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
				mdccci_dbg->base_ap_view_vir, mdccci_dbg->size);
			ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
				mdss_dbg->base_ap_view_vir, mdss_dbg->size);
			if (md && md->hw_info && md->hw_info->md_l2sram_base) {
				md_cd_lock_modem_clock_src(1);

				if (ccci_get_ap_plat() == 6991)
					ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
						md->hw_info->md_l2sram_base, md->hw_info->md_l2sram_size);
				else
					ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
						md->hw_info->md_l2sram_base, MD_L2SRAM_SIZE);
				md_cd_lock_modem_clock_src(0);
			}

		}
		/*dump md register on no response EE*/
		if (more_info == MD_EE_CASE_NO_RESPONSE && per_md_data != NULL)
			per_md_data->md_dbg_dump_flag = MD_DBG_DUMP_ALL;
	} else if (level == MDEE_DUMP_LEVEL_STAGE2) {
		mdee_info_prepare_v6(mdee);
		mdee_info_dump_v6(mdee);
	}
}

static struct md_ee_ops mdee_ops_v6 = {
	.dump_ee_info = &mdee_dumper_v6_dump_ee_info,
	.set_ee_pkg = &mdee_dumper_v6_set_ee_pkg,
};

#if IS_ENABLED(CONFIG_MTK_EMI)
static void mdee_dumper_v6_emimpu_callback(
		const char *vio_msg)
{
	int has_write = 0;
	int md_state;
	struct ccci_fsm_ctl *ctl = ccci_fsm_entries;
	struct ccci_modem *md = ccci_get_modem();

	if (vio_msg)
		CCCI_MEM_LOG_TAG(0, FSM,
			"%s: %s\n", __func__, vio_msg);
	else {
		CCCI_ERROR_LOG(0, FSM,
			"%s: msg is null\n", __func__);
		return;
	}

	if (md) {
		md_state = ccci_fsm_get_md_state();
		if (md_state != INVALID && md_state != GATED &&
			md_state != WAITING_TO_STOP) {
			if (md->ops->dump_info)
				md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);
		}
	}

	if (ctl) {
		memset(ctl->ee_ctl.ex_mpu_string, 0x0,
			sizeof(ctl->ee_ctl.ex_mpu_string));
		has_write = snprintf(&ctl->ee_ctl.ex_mpu_string[0],
			MD_EX_MPU_STR_LEN,
			"%s\n", vio_msg);
		if (has_write <= 0)
			return;
	}
}

static void mdee_dumper_v6_smpu_callback(const char *vio_msg)
{
	int md_state;
	struct ccci_fsm_ctl *ctl = ccci_fsm_entries;
	struct ccci_modem *md = ccci_get_modem();

	if (vio_msg)
		CCCI_MEM_LOG_TAG(0, FSM, "%s: %s\n", __func__, vio_msg);
	else {
		CCCI_ERROR_LOG(0, FSM, "%s: msg is null\n", __func__);
		return;
	}

	if (md) {
		md_state = ccci_fsm_get_md_state();
		if (md_state != INVALID && md_state != GATED &&
			md_state != WAITING_TO_STOP) {
			if (md->ops->dump_info)
				md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);
		}
	}

	if (ctl) {
		memset(ctl->ee_ctl.ex_smpu_string, 0x0,
			sizeof(ctl->ee_ctl.ex_smpu_string));
		scnprintf(&ctl->ee_ctl.ex_smpu_string[0],
			MD_EX_MPU_STR_LEN, "%s\n", vio_msg);
	}
}
#endif

int mdee_dumper_v6_alloc(struct ccci_fsm_ee *mdee)
{
	struct mdee_dumper_v6 *dumper = NULL;

#if IS_ENABLED(CONFIG_MTK_EMI)
	if (mtk_emimpu_md_handling_register(&mdee_dumper_v6_emimpu_callback))
		CCCI_ERROR_LOG(0, FSM, "%s: mtk_emimpu_md_handling_register fail\n",
			__func__);
	if (mtk_smpu_md_handling_register(&mdee_dumper_v6_smpu_callback))
		CCCI_ERROR_LOG(0, FSM, "%s: mtk_smpu_md_handling_register fail\n",
			__func__);
#endif

	/* Allocate port_proxy obj and set all member zero */
	dumper = kzalloc(sizeof(struct mdee_dumper_v6), GFP_KERNEL);
	if (dumper == NULL) {
		CCCI_ERROR_LOG(0, FSM, "%s:alloc mdee_parser_v5 fail\n", __func__);
		return -1;
	}
	mdee->dumper_obj = dumper;
	mdee->ops = &mdee_ops_v6;
	return 0;
}

