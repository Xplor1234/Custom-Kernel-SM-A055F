// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/atomic.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/pstore.h>
#include <linux/seq_file.h>
#include <linux/sched/clock.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#if IS_ENABLED(CONFIG_SEC_DEBUG)
#include <linux/sec_debug.h>
#endif

#include <mt-plat/aee.h>
#include "mboot_params_internal.h"
#include "mrdump_helper.h"
#include "mrdump_mini.h"
#include "mrdump_private.h"

#define MBOOT_PARAMS_HEADER_STR_LEN 1024

static int mtk_cpu_num;

#define ARRAY_LEN_4 4
#define ARRAY_LEN_8 8

#define FUNC_NAME_LEN 64

static int mboot_params_init_done;
static unsigned int old_wdt_status;
static int mboot_params_clear;

static char *mbootlog_buf;
static u32 mbootlog_buf_len = SZ_128K;
static u32 mbootlog_first_idx;
static u32 mbootlog_size;

/*
 *  This group of API call by sub-driver module to report reboot reasons
 *  aee_rr_* stand for previous reboot reason
 */
struct last_reboot_reason {
	uint32_t fiq_step;
	/* 0xaeedeadX: X=1 (HWT), X=2 (KE), X=3 (nested panic) */
	uint32_t exp_type;
	uint32_t kick;
	uint32_t check;
	uint64_t kaslr_offset;
	uint64_t oops_in_progress_addr;

	uint64_t wdk_ktime;
	uint64_t wdk_systimer_cnt;

	uint32_t mtk_cpuidle_footprint[AEE_MTK_CPU_NUMS];
	uint32_t mcdi_footprint[AEE_MTK_CPU_NUMS];
	uint32_t clk_data[ARRAY_LEN_8];

	uint8_t gpu_dvfs_vgpu;
	uint8_t gpu_dvfs_oppidx;
	uint8_t gpu_dvfs_status;
	int8_t gpu_dvfs_power_count;

	uint64_t ptp_vboot;
	uint64_t ptp_gpu_volt;
	uint64_t ptp_gpu_volt_1;
	uint64_t ptp_gpu_volt_2;
	uint64_t ptp_gpu_volt_3;
	uint64_t ptp_temp;
	uint8_t ptp_status;

	unsigned long last_init_func;
	char last_init_func_name[FUNC_NAME_LEN];
	char last_shutdown_device[FUNC_NAME_LEN];
	uint32_t hang_detect_timeout_count;
	uint32_t gz_irq;
};

struct reboot_reason_pl {
	u32 wdt_status;
	u32 data[];
};

struct mboot_params_buffer {
	uint32_t sig;
	/* for size comptible */
	uint32_t off_pl;
	uint32_t off_lpl;	/* last preloader: struct reboot_reason_pl */
	uint32_t sz_pl;
	uint32_t off_lk;
	uint32_t off_llk;	/* last lk: struct reboot_reason_lk */
	uint32_t sz_lk;
	uint32_t padding[3];
	uint32_t sz_buffer;
	uint32_t off_linux;	/* struct last_reboot_reason */
	uint32_t filling[4];
};

#define REBOOT_REASON_SIG (0x43474244)	/* DBRR */
static int FIQ_log_size = sizeof(struct mboot_params_buffer);

static struct mboot_params_buffer *mboot_params_buffer;
static struct mboot_params_buffer *mboot_params_old;
static struct mboot_params_buffer *mboot_params_buffer_pa;

static DEFINE_SPINLOCK(mboot_params_lock);

static atomic_t mp_in_fiq = ATOMIC_INIT(0);

static void mboot_params_init_val(void);
static void aee_rr_mboot_params_proc_init(void);

#include "desc/desc_s.h"
void aee_rr_get_desc_info(unsigned long *addr, unsigned long *size,
		unsigned long *start)
{
	if (!addr || !size || !start)
		return;
	*addr = IDESC_ADDR;
	*size = IDESC_SIZE;
	*start = IDESC_START_POS;
}

#ifdef __aarch64__
static void *_memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = dest;
	const char *s = src;

	while (count--)
		*tmp++ = *s++;
	return dest;
}

#ifdef memcpy
#undef memcpy
#endif
#define memcpy _memcpy
#endif

#define LAST_RR_SEC_VAL(header, sect, type, item) \
	(header->off_##sect ? \
	((type *)((void *)header + header->off_##sect))->item : 0)
#define LAST_RRR_BUF_VAL(buf, rr_item)	\
	LAST_RR_SEC_VAL(buf, linux, struct last_reboot_reason, rr_item)
#define LAST_RRPL_BUF_VAL(buf, rr_item)	\
	LAST_RR_SEC_VAL(buf, pl, struct reboot_reason_pl, rr_item)
#define LAST_RRR_VAL(rr_item)	\
	LAST_RR_SEC_VAL(mboot_params_old, linux, struct last_reboot_reason, \
			rr_item)
#define LAST_RRPL_VAL(rr_item)	\
	LAST_RR_SEC_VAL(mboot_params_old, pl, struct reboot_reason_pl, rr_item)

void get_mbootlog_buffer(unsigned long *addr,
		unsigned long *size, unsigned long *start)
{
	*addr = (unsigned long)mbootlog_buf;
	*size = mbootlog_buf_len;
	if (mbootlog_size >= mbootlog_buf_len)
		*start = (unsigned long)&mbootlog_first_idx;
}

void sram_log_save(const char *msg, int count)
{
	int rem;

	if (!mbootlog_buf)
		return;

#if IS_ENABLED(CONFIG_SEC_DEBUG)
	pr_info("[mbootlog] %s", msg);
#endif

	/* count >= buffer_size, full the buffer */
	if (count >= mbootlog_buf_len) {
		memcpy(mbootlog_buf, msg + (count - mbootlog_buf_len),
				mbootlog_buf_len);
		mbootlog_first_idx = 0;
		mbootlog_size = mbootlog_buf_len;
	} else if (count > (mbootlog_buf_len - mbootlog_first_idx)) {
		/* count > last buffer, full them and fill the head buffer */
		rem = mbootlog_buf_len - mbootlog_first_idx;
		memcpy(mbootlog_buf + mbootlog_first_idx, msg, rem);
		memcpy(mbootlog_buf, msg + rem, count - rem);
		mbootlog_first_idx = count - rem;
		mbootlog_size = mbootlog_buf_len;
	} else {
		/* count <=  last buffer, fill in free buffer */
		memcpy(mbootlog_buf + mbootlog_first_idx, msg, count);
		mbootlog_first_idx += count;
		mbootlog_size += count;
		if (mbootlog_first_idx >= mbootlog_buf_len)
			mbootlog_first_idx = 0;
		if (mbootlog_size > mbootlog_buf_len)
			mbootlog_size = mbootlog_buf_len;
	}
}

void aee_disable_mboot_params_write(void)
{
	atomic_set(&mp_in_fiq, 1);
}

void aee_sram_fiq_log(const char *msg)
{
	unsigned int count = strlen(msg);
	int delay = 100;

	if (!mbootlog_buf)
		return;

	if (FIQ_log_size + count > mbootlog_buf_len)
		return;

	atomic_set(&mp_in_fiq, 1);

	while ((delay > 0) && (spin_is_locked(&mboot_params_lock))) {
		udelay(1);
		delay--;
	}

	sram_log_save(msg, count);
	FIQ_log_size += count;
}
EXPORT_SYMBOL(aee_sram_fiq_log);

static void mboot_params_write(struct console *console, const char *s,
		unsigned int count)
{
	unsigned long flags;

	if (!mbootlog_buf)
		return;

	if (atomic_read(&mp_in_fiq))
		return;

	spin_lock_irqsave(&mboot_params_lock, flags);

	sram_log_save(s, count);

	spin_unlock_irqrestore(&mboot_params_lock, flags);
}

static struct console mboot_params = {
	.name = "ram",
	.write = mboot_params_write,
	.flags = CON_PRINTBUFFER | CON_ENABLED | CON_ANYTIME,
	.index = -1,
};

void aee_sram_printk(const char *fmt, ...)
{
	unsigned long long t;
	unsigned long nanosec_rem;
	va_list args;
	int r, tlen;
	char sram_printk_buf[256];

	if (!mbootlog_buf)
		return;

	va_start(args, fmt);

	preempt_disable();
	t = cpu_clock(raw_smp_processor_id());
	nanosec_rem = do_div(t, 1000000000);
	tlen = sprintf(sram_printk_buf, ">%5lu.%06lu< ", (unsigned long)t,
			nanosec_rem / 1000);
	if (tlen < 0) {
		preempt_enable();
		return;
	}

	r = vscnprintf(sram_printk_buf + tlen, sizeof(sram_printk_buf) - tlen,
			fmt, args);

	mboot_params_write(NULL, sram_printk_buf, r + tlen);
	preempt_enable();
	va_end(args);
}
EXPORT_SYMBOL(aee_sram_printk);

void mboot_params_enable_console(int enabled)
{
	if (enabled)
		mboot_params.flags |= CON_ENABLED;
	else
		mboot_params.flags &= ~CON_ENABLED;
}

static int mboot_params_check_header(struct mboot_params_buffer *buffer)
{
	if (!buffer || (buffer->sz_buffer != mboot_params_buffer->sz_buffer)
		|| buffer->off_pl > buffer->sz_buffer
		|| buffer->off_lk > buffer->sz_buffer
		|| buffer->off_linux > buffer->sz_buffer
		|| buffer->off_pl + ALIGN(buffer->sz_pl, 64) != buffer->off_lpl
		|| buffer->off_lk + ALIGN(buffer->sz_lk, 64)
			!= buffer->off_llk) {
		pr_notice("mboot_params: ilegal header.");
		return -1;
	} else
		return 0;
}

static void aee_rr_show_in_log(void)
{
	if (mboot_params_check_header(mboot_params_old))
		pr_notice("mboot_params: no valid data\n");
	else {
		pr_notice("mboot_params: last init function: 0x%lx\n",
				LAST_RRR_VAL(last_init_func));
	}
}

static int __init mboot_params_save_old(struct mboot_params_buffer *buffer,
		size_t buffer_size)
{
	mboot_params_old = kmalloc(buffer_size, GFP_KERNEL);
	if (!mboot_params_old)
		return -1;
	memcpy(mboot_params_old, buffer, buffer_size);
	aee_rr_show_in_log();
	return 0;
}

static int __init mboot_params_init(struct mboot_params_buffer *buffer,
		size_t buffer_size)
{
	mboot_params_buffer = buffer;
	buffer->sz_buffer = buffer_size;

	if (buffer->sig != REBOOT_REASON_SIG  ||
			mboot_params_check_header(buffer)) {
		memset_io((void *)buffer, 0, buffer_size);
		buffer->sig = REBOOT_REASON_SIG;
		mboot_params_clear = 1;
	} else {
		old_wdt_status = LAST_RRPL_BUF_VAL(buffer, wdt_status);
	}
	if (mboot_params_save_old(buffer, buffer_size))
		pr_notice("mboot_params: failed to creat old buffer\n");
	if (buffer->sz_lk != 0 && buffer->off_lk + ALIGN(buffer->sz_lk, 64) ==
			buffer->off_llk)
		buffer->off_linux = buffer->off_llk + ALIGN(buffer->sz_lk, 64);
	else
		/* OTA:leave enough space for pl/lk */
		buffer->off_linux = 512;
	buffer->sz_buffer = buffer_size;
	memset_io((void *)buffer + buffer->off_linux, 0,
			buffer_size - buffer->off_linux);
	mboot_params_init_desc(buffer->off_linux);
#ifndef CONFIG_PSTORE
	register_console(&mboot_params);
#endif
	mboot_params_init_val();
	mbootlog_buf = kzalloc(SZ_128K, GFP_KERNEL);
	if (!mbootlog_buf)
		pr_notice("mboot_params: mbootlog_buf(SYS_LAST_KMSG) invalid");
#ifdef MODULE
	aee_rr_mboot_params_proc_init();
#endif
	mboot_params_init_done = 1;
	return 0;
}

struct mem_desc_t {
	unsigned int start;
	unsigned int size;
	unsigned int def_type;
	unsigned int offset;
};

#ifdef CONFIG_OF
static int __init dt_get_mboot_params(struct mem_desc_t *data)
{
	struct mem_desc_t *sram;
	struct device_node *np_chosen;

	np_chosen = of_find_node_by_path("/chosen");
	if (!np_chosen)
		np_chosen = of_find_node_by_path("/chosen@0");

	sram = (struct mem_desc_t *)of_get_property(np_chosen,
						    "ram_console",
						    NULL);
	if (sram) {
		pr_notice("mboot_params:[DT] 0x%x@0x%x, 0x%x(0x%x)\n",
				sram->size, sram->start,
				sram->def_type, sram->offset);
		*data = *sram;
		return 1;
	}
	return 0;
}
#endif

enum MBOOT_PARAMS_DEF_TYPE {
	MBOOT_PARAMS_DEF_UNKNOWN = 0,
	MBOOT_PARAMS_DEF_SRAM,
	MBOOT_PARAMS_DEF_DRAM,
};

#define MEM_MAGIC1 0x61646472 /* "addr" */
#define MEM_MAGIC2 0x73697a65 /* "size" */
struct mboot_params_memory_info {
	u32 magic1;
	u32 sram_plat_dbg_info_addr;
	u32 sram_plat_dbg_info_size;
	u32 sram_log_store_addr;
	u32 sram_log_store_size;
	u32 mrdump_addr;
	u32 mrdump_size;
	u32 dram_addr;
	u32 dram_size;
	u32 pstore_addr;
	u32 pstore_size;
	u32 pstore_console_size;
	u32 pstore_pmsg_size;
	u32 mrdump_mini_header_addr;
	u32 mrdump_mini_header_size;
	u32 magic2;
};

static void mboot_params_fatal(const char *str)
{
	pr_info("mboot_params: FATAL:%s\n", str);
}

static phys_addr_t mboot_params_reserve_memory(void)
{
	struct device_node *rmem_node;
	struct reserved_mem *rmem;

	/* Get reserved memory */
	rmem_node = of_find_compatible_node(NULL, NULL, DEBUG_COMPATIBLE);
	if (!rmem_node) {
		pr_info("[mboot_params] no node for reserved memory\n");
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(rmem_node);
	if (!rmem) {
		pr_info("[mboot_params] cannot lookup reserved memory\n");
		return -EINVAL;
	}

	return rmem->base + MBOOT_PARAMS_DRAM_OFF;
}

static void mboot_params_parse_memory_info(struct mem_desc_t *sram,
		struct mboot_params_memory_info *p_memory_info)
{
	struct mboot_params_memory_info *memory_info;
	u32 magic1, magic2;
	u32 mrdump_addr, mrdump_size;
	u32 dram_addr, dram_size;
	u32 mini_addr, mini_size;

	if (sram->offset > sram->size) {
		memory_info = ioremap_wc((sram->start + sram->offset),
				sizeof(struct mboot_params_memory_info));
		if (!memory_info) {
			pr_info("mboot_params: [DT] offset:0x%x not map\n",
					sram->offset);
			mboot_params_fatal("memory_info not map");
			return;
		}
		magic1 = memory_info->magic1;
		magic2 = memory_info->magic2;
		mrdump_addr = memory_info->mrdump_addr;
		mrdump_size = memory_info->mrdump_size;
		dram_addr = memory_info->dram_addr;
		dram_size = memory_info->dram_size;
		mini_addr = memory_info->mrdump_mini_header_addr;
		mini_size = memory_info->mrdump_mini_header_size;

		if (magic1 == MEM_MAGIC1 && magic2 == MEM_MAGIC2) {
			mrdump_mini_set_addr_size(mini_addr, mini_size);
			pr_notice("mboot_params: [DT] 0x%x@0x%x\n",
					mini_size, mini_addr);
			memcpy(p_memory_info, memory_info,
				sizeof(struct mboot_params_memory_info));
		} else {
			pr_info("[DT] self (0x%x@0x%x)-0x%x@0x%x\n",
					magic1, magic2,
					dram_size, dram_addr);
			pr_info("[DT] mrdump 0x%x@0x%x-0x%x@0x%x\n",
					mini_size, mini_addr,
					mrdump_size, mrdump_addr);
			mboot_params_fatal("illegal magic number");
		}
	} else {
		pr_info("mboot_params: [DT] offset:0x%x illegal\n",
			sram->offset);
		mboot_params_fatal("illegal offset");
	}
}

static int __init mboot_params_early_init(void)
{
	struct mboot_params_buffer *bufp = NULL;
	size_t buffer_size = 0;
#ifdef CONFIG_OF
	struct mem_desc_t sram = { 0 };
	struct mboot_params_memory_info memory_info_data = {0};
	unsigned int start, size;

	if (dt_get_mboot_params(&sram)) {
		mboot_params_parse_memory_info(&sram, &memory_info_data);
		if (sram.def_type == MBOOT_PARAMS_DEF_SRAM) {
			pr_info("mboot_params: using sram:0x%x\n", sram.start);
			start = sram.start;
			size  = sram.size;
			bufp = ioremap_wc(sram.start, sram.size);
		} else if (sram.def_type == MBOOT_PARAMS_DEF_DRAM) {
			start = mboot_params_reserve_memory();
			size = MBOOT_PARAMS_DRAM_SIZE;
			pr_info("mboot_params: using dram:0x%x(0x%x)\n",
				start, size);
			bufp = remap_lowmem(start, size);
		} else {
			pr_info("mboot_params: unknown def type:%d\n",
					sram.def_type);
			mboot_params_fatal("unknown def type");
			return -ENODEV;
		}
		/* unsigned long conversion:
		 * make size equals to pointer size
		 * to avoid build error as below for aarch64 case
		 * (error: cast to 'struct mboot_params_buffer *' from
		 * smaller integer type 'unsigned int'
		 * [-Werror,-Wint-to-pointer-cast])
		 */
		mboot_params_buffer_pa =
			(struct mboot_params_buffer *)(unsigned long)start;
		if (bufp) {
			buffer_size = size;
			if (bufp->sig != REBOOT_REASON_SIG) {
				pr_info("mboot_params: illegal sig:0x%x\n",
						bufp->sig);
				mboot_params_fatal("illegal sig");
			}
		} else {
			pr_info("mboot_params: ioremap failed, [0x%x, 0x%x]\n",
					start, size);
			mboot_params_fatal("ioremap failed");
		}
	} else {
		mboot_params_fatal("OF not found property in dts");
	}
#else
	pr_notice("mboot_params: CONFIG_OF not set\n")
#endif

	pr_notice("mboot_params: buffer start: 0x%lx, size: 0x%zx\n",
			(unsigned long)bufp, buffer_size);
	mtk_cpu_num = num_present_cpus();
	if (bufp)
		return mboot_params_init(bufp, buffer_size);
	else
		return -ENODEV;
}

#ifdef MODULE
int __init mrdump_module_init_mboot_params(void)
{
	return mboot_params_early_init();
}
#else
console_initcall(mboot_params_early_init);
#endif

/* aee sram flags save */
#define RR_BASE(stage)	\
	((void *)mboot_params_buffer + mboot_params_buffer->off_##stage)
#define RR_LINUX ((struct last_reboot_reason *)RR_BASE(linux))
#define RR_BASE_PA(stage)	\
	((void *)mboot_params_buffer_pa + mboot_params_buffer->off_##stage)
#define RR_LINUX_PA ((struct last_reboot_reason *)RR_BASE_PA(linux))

/*NOTICE: You should check if mboot_params is null before call these macros*/
#define LAST_RR_SET(rr_item, value) (RR_LINUX->rr_item = value)

#define LAST_RR_SET_WITH_ID(rr_item, id, value) (RR_LINUX->rr_item[id] = value)

#define LAST_RR_VAL(rr_item)				\
	(mboot_params_buffer ? RR_LINUX->rr_item : 0)

#define LAST_RR_MEMCPY(rr_item, str, len)				\
	(strlcpy(RR_LINUX->rr_item, str, len))

#define LAST_RR_MEMCPY_WITH_ID(rr_item, id, str, len)			\
	(strlcpy(RR_LINUX->rr_item[id], str, len))

#if IS_ENABLED(CONFIG_SEC_DEBUG)

#if IS_ENABLED(CONFIG_WT_PROJECT_S96818AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96818BA1)
#define SECDBG_RR_BASE 0x1100E0 /* "addr" */
#elif IS_ENABLED(CONFIG_WT_PROJECT_S96901AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96902AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96901WA1)
#define SECDBG_RR_BASE 0x11FFE0 /* "addr" */
#endif

struct secdbg_reset_reason {
	uint32_t is_upload;
	uint32_t upload_reason;
	uint32_t is_power_reset;
	uint32_t power_reset_reason;
#if IS_ENABLED(CONFIG_SEC_DUMP_SINK)	
	uint32_t reboot_magic;
#endif
	uint32_t reserved[3];
};

static struct secdbg_reset_reason *secdbg_rr;
int secdbg_rr_init(void)
{
 		secdbg_rr = ioremap_wc(SECDBG_RR_BASE,
 				sizeof(struct secdbg_reset_reason));

    if (!secdbg_rr) {
        pr_info("Remap secdbg_rr_addr failed\n");
        return -EIO;
    }

    return 0;
}

void secdbg_set_upload_magic(uint32_t upload_magic)
{
	secdbg_rr->is_upload = upload_magic;
}
EXPORT_SYMBOL(secdbg_set_upload_magic);

void secdbg_set_power_flag(uint32_t power_flag)
{
	secdbg_rr->is_power_reset = power_flag;
}
EXPORT_SYMBOL(secdbg_set_power_flag);

void secdbg_set_upload_reason(uint32_t upload_reason)
{
  	secdbg_rr->upload_reason = upload_reason;
}
EXPORT_SYMBOL(secdbg_set_upload_reason);

void secdbg_set_power_reset_reason(uint32_t reset_reason)
{
  	secdbg_rr->power_reset_reason = reset_reason;
}
EXPORT_SYMBOL(secdbg_set_power_reset_reason);

void sec_upload_cause(void *buf)
{
	secdbg_set_upload_magic(UPLOAD_MAGIC_UPLOAD);
	
	if (!strncmp(buf, "User Fault", 10))
		secdbg_set_upload_reason(UPLOAD_CAUSE_USER_FAULT);
	else if (!strncmp(buf, "Hard Reset Hook", 15))
		secdbg_set_upload_reason(UPLOAD_CAUSE_FORCED_UPLOAD);		
	else if (!strncmp(buf, "Crash Key", 9))
		secdbg_set_upload_reason(UPLOAD_CAUSE_FORCED_UPLOAD);
	else if (!strncmp(buf, "User Crash Key", 14))
		secdbg_set_upload_reason(UPLOAD_CAUSE_USER_FORCED_UPLOAD);
	else if (!strncmp(buf, "CP Crash", 8))
		secdbg_set_upload_reason(UPLOAD_CAUSE_CP_ERROR_FATAL);
	else if (!strncmp(buf, "Watchdog", 8))
		secdbg_set_upload_reason(UPLOAD_CAUSE_WATCHDOG);
	else
		secdbg_set_upload_reason(UPLOAD_CAUSE_KERNEL_PANIC);
}
EXPORT_SYMBOL(sec_upload_cause);
#endif

static void mboot_params_init_val(void)
{
#if IS_ENABLED(CONFIG_SEC_DEBUG)
  	secdbg_rr_init();
	
  	/* Initialize flags for SEC_DEBUG */
  	secdbg_set_upload_magic(UPLOAD_MAGIC_UPLOAD); 
  	secdbg_set_upload_reason(UPLOAD_CAUSE_INIT); 
  	secdbg_set_power_flag(SEC_POWER_OFF); 
  	secdbg_set_power_reset_reason(SEC_RESET_REASON_INIT); 
  	
  	pr_info("secdbg_rr->is_upload=%x\n", secdbg_rr->is_upload);
  	pr_info("secdbg_rr->upload_reason=%x\n", secdbg_rr->upload_reason);
  	pr_info("secdbg_rr->is_power_reset=%x\n", secdbg_rr->is_power_reset);
  	pr_info("secdbg_rr->power_reset_reason=%x\n", secdbg_rr->power_reset_reason);  	  	  
  
  	pr_info("SEC Debug RR setting completed\n");
#endif

#if defined(CONFIG_RANDOMIZE_BASE) && defined(CONFIG_ARM64)
	LAST_RR_SET(kaslr_offset, 0xecab1e);
#else
	LAST_RR_SET(kaslr_offset, 0xd15ab1e);
#endif
	LAST_RR_SET(oops_in_progress_addr,
		(unsigned long)(&oops_in_progress));
}

void aee_rr_rec_fiq_step(u8 step)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(fiq_step, step);
}

int aee_rr_curr_fiq_step(void)
{
	return LAST_RR_VAL(fiq_step);
}

void aee_rr_rec_exp_type(unsigned int type)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	if (!LAST_RR_VAL(exp_type) && type < 16)
		LAST_RR_SET(exp_type, MBOOT_PARAMS_EXP_TYPE_MAGIC | type);
}

unsigned int aee_rr_curr_exp_type(void)
{
	unsigned int exp_type = LAST_RR_VAL(exp_type);

	return MBOOT_PARAMS_EXP_TYPE_DEC(exp_type);
}

void aee_rr_rec_kick(uint32_t kick_bit)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(kick, kick_bit);
}
EXPORT_SYMBOL(aee_rr_rec_kick);

void aee_rr_rec_check(uint32_t check_bit)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(check, check_bit);
}
EXPORT_SYMBOL(aee_rr_rec_check);

void aee_rr_rec_kaslr_offset(uint64_t offset)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(kaslr_offset, offset);
}

void aee_rr_rec_wdk_ktime(u64 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(wdk_ktime, val);
}
EXPORT_SYMBOL(aee_rr_rec_wdk_ktime);

void aee_rr_rec_wdk_systimer_cnt(u64 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(wdk_systimer_cnt, val);
}
EXPORT_SYMBOL(aee_rr_rec_wdk_systimer_cnt);

void aee_rr_rec_clk(int id, u32 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	if (id >= 0 && id < ARRAY_LEN_8)
		LAST_RR_SET_WITH_ID(clk_data, id, val);
	else
		pr_notice("%s invalid id= %d\n", __func__, id);
}

void aee_rr_rec_mcdi_val(int id, u32 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	if (id >= 0 && id < num_possible_cpus())
		LAST_RR_SET_WITH_ID(mcdi_footprint, id, val);
	else
		pr_notice("%s invalid id= %d\n", __func__, id);
}

unsigned long *aee_rr_rec_mtk_cpuidle_footprint_va(void)
{
	if (mboot_params_buffer)
		return (unsigned long *)&RR_LINUX->mtk_cpuidle_footprint;
	else
		return NULL;
}

unsigned long *aee_rr_rec_mtk_cpuidle_footprint_pa(void)
{
	if (mboot_params_buffer_pa)
		return (unsigned long *)&RR_LINUX_PA->mtk_cpuidle_footprint;
	else
		return NULL;
}

void aee_rr_rec_gpu_dvfs_vgpu(u8 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(gpu_dvfs_vgpu, val);
}
EXPORT_SYMBOL(aee_rr_rec_gpu_dvfs_vgpu);

u8 aee_rr_curr_gpu_dvfs_vgpu(void)
{
	return LAST_RR_VAL(gpu_dvfs_vgpu);
}
EXPORT_SYMBOL(aee_rr_curr_gpu_dvfs_vgpu);

void aee_rr_rec_gpu_dvfs_oppidx(u8 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(gpu_dvfs_oppidx, val);
}
EXPORT_SYMBOL(aee_rr_rec_gpu_dvfs_oppidx);

void aee_rr_rec_gpu_dvfs_status(u8 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(gpu_dvfs_status, val);
}
EXPORT_SYMBOL(aee_rr_rec_gpu_dvfs_status);

u8 aee_rr_curr_gpu_dvfs_status(void)
{
	return LAST_RR_VAL(gpu_dvfs_status);
}
EXPORT_SYMBOL(aee_rr_curr_gpu_dvfs_status);

void aee_rr_rec_gpu_dvfs_power_count(int val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(gpu_dvfs_power_count, val);
}
EXPORT_SYMBOL(aee_rr_rec_gpu_dvfs_power_count);

void aee_rr_rec_ptp_vboot(u64 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(ptp_vboot, val);
}
EXPORT_SYMBOL(aee_rr_rec_ptp_vboot);

void aee_rr_rec_ptp_gpu_volt(u64 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(ptp_gpu_volt, val);
}
EXPORT_SYMBOL(aee_rr_rec_ptp_gpu_volt);

void aee_rr_rec_ptp_gpu_volt_1(u64 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(ptp_gpu_volt_1, val);
}
EXPORT_SYMBOL(aee_rr_rec_ptp_gpu_volt_1);

void aee_rr_rec_ptp_gpu_volt_2(u64 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(ptp_gpu_volt_2, val);
}
EXPORT_SYMBOL(aee_rr_rec_ptp_gpu_volt_2);

void aee_rr_rec_ptp_gpu_volt_3(u64 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(ptp_gpu_volt_3, val);
}
EXPORT_SYMBOL(aee_rr_rec_ptp_gpu_volt_3);

void aee_rr_rec_ptp_temp(u64 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(ptp_temp, val);
}
EXPORT_SYMBOL(aee_rr_rec_ptp_temp);

void aee_rr_rec_ptp_status(u8 val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(ptp_status, val);
}
EXPORT_SYMBOL(aee_rr_rec_ptp_status);

u64 aee_rr_curr_ptp_gpu_volt(void)
{
	return LAST_RR_VAL(ptp_gpu_volt);
}
EXPORT_SYMBOL(aee_rr_curr_ptp_gpu_volt);

u64 aee_rr_curr_ptp_gpu_volt_1(void)
{
	return LAST_RR_VAL(ptp_gpu_volt_1);
}
EXPORT_SYMBOL(aee_rr_curr_ptp_gpu_volt_1);

u64 aee_rr_curr_ptp_gpu_volt_2(void)
{
	return LAST_RR_VAL(ptp_gpu_volt_2);
}
EXPORT_SYMBOL(aee_rr_curr_ptp_gpu_volt_2);

u64 aee_rr_curr_ptp_gpu_volt_3(void)
{
	return LAST_RR_VAL(ptp_gpu_volt_3);
}
EXPORT_SYMBOL(aee_rr_curr_ptp_gpu_volt_3);

u64 aee_rr_curr_ptp_temp(void)
{
	return LAST_RR_VAL(ptp_temp);
}
EXPORT_SYMBOL(aee_rr_curr_ptp_temp);

u8 aee_rr_curr_ptp_status(void)
{
	return LAST_RR_VAL(ptp_status);
}
EXPORT_SYMBOL(aee_rr_curr_ptp_status);

void aee_rr_rec_last_init_func(unsigned long val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	if (LAST_RR_VAL(last_init_func) == ~(unsigned long)(0))
		return;
	LAST_RR_SET(last_init_func, val);
}

void aee_rr_rec_last_init_func_name(const char *str)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_MEMCPY(last_init_func_name, str, FUNC_NAME_LEN-1);
}

void aee_rr_rec_last_shutdown_device(const char *str)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_MEMCPY(last_shutdown_device, str, FUNC_NAME_LEN-1);
}

void aee_rr_rec_hang_detect_timeout_count(unsigned int val)
{
	if (!mboot_params_init_done || !mboot_params_buffer)
		return;
	LAST_RR_SET(hang_detect_timeout_count, val);
}
EXPORT_SYMBOL(aee_rr_rec_hang_detect_timeout_count);

unsigned long *aee_rr_rec_gz_irq_pa(void)
{
	if (mboot_params_buffer_pa)
		return (unsigned long *)&RR_LINUX_PA->gz_irq;
	else
		return NULL;
}

typedef void (*last_rr_show_t) (struct seq_file *m);
typedef void (*last_rr_show_cpu_t) (struct seq_file *m, int cpu);

void aee_rr_show_wdt_status(struct seq_file *m)
{
	unsigned int wdt_status;
	struct mboot_params_buffer *buffer = mboot_params_old;

	if (!buffer->off_pl || buffer->off_pl + ALIGN(buffer->sz_pl, 64)
			!= buffer->off_lpl) {
		/* workaround for compatibility to old preloader & lk (OTA) */
		wdt_status = *((unsigned char *)buffer + 12);
	} else
		wdt_status = LAST_RRPL_VAL(wdt_status);
	seq_printf(m, "WDT status: %d", wdt_status);
}

void aee_rr_show_fiq_step(struct seq_file *m)
{
	seq_printf(m, " fiq step: %u ", LAST_RRR_VAL(fiq_step));
}

void aee_rr_show_exp_type(struct seq_file *m)
{
	unsigned int exp_type = LAST_RRR_VAL(exp_type);

	seq_printf(m, " exception type: %u\n",
		   MBOOT_PARAMS_EXP_TYPE_DEC(exp_type));
}

void aee_rr_show_kick_check(struct seq_file *m)
{
	uint32_t kick_bit = LAST_RRR_VAL(kick);
	uint32_t check_bit = LAST_RRR_VAL(check);

	seq_printf(m, "kick=0x%x,check=0x%x\n", kick_bit, check_bit);
}

void aee_rr_show_kaslr_offset(struct seq_file *m)
{
	uint64_t kaslr_offset = LAST_RRR_VAL(kaslr_offset);

	seq_printf(m, "Kernel Offset: 0x%llx\n", kaslr_offset);
}

void aee_rr_show_oops_in_progress_addr(struct seq_file *m)
{
	uint64_t oops_in_progress_addr;

	oops_in_progress_addr = LAST_RRR_VAL(oops_in_progress_addr);
	seq_printf(m, "&oops_in_progress: 0x%llx\n",
		       oops_in_progress_addr);
}

void aee_rr_show_wdk_ktime(struct seq_file *m)
{
	uint64_t ktime = LAST_RRR_VAL(wdk_ktime);

	seq_printf(m, "wdk_ktime=%lld\n", ktime);
}

void aee_rr_show_wdk_systimer_cnt(struct seq_file *m)
{
	uint64_t systimer_cnt = LAST_RRR_VAL(wdk_systimer_cnt);

	seq_printf(m, "wdk_systimer_cnt=%lld\n", systimer_cnt);
}

void aee_rr_show_mtk_cpuidle_footprint(struct seq_file *m, int cpu)
{
	seq_printf(m, "  mtk_cpuidle_footprint: 0x%x\n",
			LAST_RRR_VAL(mtk_cpuidle_footprint[cpu]));
}

void aee_rr_show_mcdi_footprint(struct seq_file *m, int cpu)
{
	seq_printf(m, "  mcdi footprint: 0x%x\n",
			LAST_RRR_VAL(mcdi_footprint[cpu]));
}

void aee_rr_show_clk(struct seq_file *m)
{
	int i = 0;

	for (i = 0; i < ARRAY_LEN_8; i++)
		seq_printf(m, "clk_data: 0x%x\n", LAST_RRR_VAL(clk_data[i]));
}

void aee_rr_show_gpu_dvfs_vgpu(struct seq_file *m)
{
	seq_printf(m, "gpu_dvfs_vgpu: 0x%x\n", LAST_RRR_VAL(gpu_dvfs_vgpu));
}

void aee_rr_show_gpu_dvfs_oppidx(struct seq_file *m)
{
	seq_printf(m, "gpu_dvfs_oppidx: 0x%x\n", LAST_RRR_VAL(gpu_dvfs_oppidx));
}

void aee_rr_show_gpu_dvfs_status(struct seq_file *m)
{
	seq_printf(m, "gpu_dvfs_status: 0x%x\n", LAST_RRR_VAL(gpu_dvfs_status));
}

void aee_rr_show_gpu_dvfs_power_count(struct seq_file *m)
{
	seq_printf(m, "gpu_dvfs_power_count: %d\n",
		LAST_RRR_VAL(gpu_dvfs_power_count));
}

void aee_rr_show_ptp_vboot(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_bank_[%d]_vboot = 0x%llx\n", i,
			(LAST_RRR_VAL(ptp_vboot) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_gpu_volt(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_gpu_volt[%d] = %llx\n", i,
			   (LAST_RRR_VAL(ptp_gpu_volt) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_gpu_volt_1(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_gpu_volt_1[%d] = %llx\n", i,
			   (LAST_RRR_VAL(ptp_gpu_volt_1) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_gpu_volt_2(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_gpu_volt_2[%d] = %llx\n", i,
			   (LAST_RRR_VAL(ptp_gpu_volt_2) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_gpu_volt_3(struct seq_file *m)
{
	int i;

	for (i = 0; i < 8; i++)
		seq_printf(m, "ptp_gpu_volt_3[%d] = %llx\n", i,
			   (LAST_RRR_VAL(ptp_gpu_volt_3) >> (i * 8)) & 0xFF);
}

void aee_rr_show_ptp_temp(struct seq_file *m)
{
	seq_printf(m, "ptp_temp: big = %llx\n", LAST_RRR_VAL(ptp_temp) & 0xFF);
	seq_printf(m, "ptp_temp: GPU = %llx\n",
			(LAST_RRR_VAL(ptp_temp) >> 8) & 0xFF);
	seq_printf(m, "ptp_temp: little = %llx\n",
			(LAST_RRR_VAL(ptp_temp) >> 16) & 0xFF);
}

void aee_rr_show_ptp_status(struct seq_file *m)
{
	seq_printf(m, "ptp_status: 0x%x\n", LAST_RRR_VAL(ptp_status));
}

void aee_rr_show_last_init_func(struct seq_file *m)
{
	seq_printf(m, "last init function: 0x%lx\n",
			LAST_RRR_VAL(last_init_func));
}

void aee_rr_show_last_shutdown_device(struct seq_file *m)
{
	seq_printf(m, "last_shutdown_device: %s\n",
			LAST_RRR_VAL(last_shutdown_device));
}

void aee_rr_show_last_init_func_name(struct seq_file *m)
{
	seq_printf(m, "last_init_func_name: %s\n",
			LAST_RRR_VAL(last_init_func_name));
}

void aee_rr_show_hang_detect_timeout_count(struct seq_file *m)
{
	seq_printf(m, "hang detect time out: 0x%x\n",
			LAST_RRR_VAL(hang_detect_timeout_count));
}

void aee_rr_show_gz_irq(struct seq_file *m)
{
	seq_printf(m, "GZ IRQ: 0x%x\n", LAST_RRR_VAL(gz_irq));
}

int __weak mt_reg_dump(char *buf)
{
	return 1;
}

void aee_rr_show_last_pc(struct seq_file *m)
{
	char *reg_buf = kmalloc(4096, GFP_KERNEL);

	if (reg_buf) {
		if (!mt_reg_dump(reg_buf))
			seq_printf(m, "%s\n", reg_buf);
		kfree(reg_buf);
	}
}

int __weak mt_lastbus_dump(char *buf)
{
	return 1;
}

void aee_rr_show_last_bus(struct seq_file *m)
{
	char *reg_buf = kmalloc(4096, GFP_KERNEL);

	if (reg_buf) {
		if (!mt_lastbus_dump(reg_buf))
			seq_printf(m, "%s\n", reg_buf);
		kfree(reg_buf);
	}
}


last_rr_show_t aee_rr_show[] = {
	aee_rr_show_wdt_status,
	aee_rr_show_fiq_step,
	aee_rr_show_exp_type,
	aee_rr_show_kick_check,
	aee_rr_show_kaslr_offset,
	aee_rr_show_oops_in_progress_addr,
	aee_rr_show_wdk_ktime,
	aee_rr_show_wdk_systimer_cnt,
	aee_rr_show_last_pc,
	aee_rr_show_last_bus,

	aee_rr_show_clk,
	aee_rr_show_gpu_dvfs_vgpu,
	aee_rr_show_gpu_dvfs_oppidx,
	aee_rr_show_gpu_dvfs_status,
	aee_rr_show_gpu_dvfs_power_count,
	aee_rr_show_ptp_vboot,
	aee_rr_show_ptp_gpu_volt,
	aee_rr_show_ptp_gpu_volt_2,
	aee_rr_show_ptp_gpu_volt_3,
	aee_rr_show_ptp_temp,
	aee_rr_show_ptp_status,
	aee_rr_show_hang_detect_timeout_count,
	aee_rr_show_gz_irq,
	aee_rr_show_last_init_func,
	aee_rr_show_last_init_func_name,
	aee_rr_show_last_shutdown_device
};

last_rr_show_cpu_t aee_rr_show_cpu[] = {
	aee_rr_show_mtk_cpuidle_footprint,
	aee_rr_show_mcdi_footprint
};

last_rr_show_t aee_rr_last_xxx[] = {
	aee_rr_show_last_pc,
	aee_rr_show_last_bus
};

static int aee_rr_reboot_reason_show(struct seq_file *m, void *v)
{
	int i, cpu;

	if (mboot_params_check_header(mboot_params_old)) {
		seq_puts(m, "NO VALID DATA.\n");
		seq_printf(m, "%s, old status is %u.\n", mboot_params_clear ?
				"Clear" : "Not Clear", old_wdt_status);
		seq_puts(m, "Only try to dump last_XXX.\n");
		for (i = 0; i < ARRAY_SIZE(aee_rr_last_xxx); i++)
			aee_rr_last_xxx[i] (m);
		return 0;
	}
	for (i = 0; i < ARRAY_SIZE(aee_rr_show); i++)
		aee_rr_show[i] (m);

	for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
		seq_printf(m, "CPU %d\n", cpu);
		for (i = 0; i < ARRAY_SIZE(aee_rr_show_cpu); i++)
			aee_rr_show_cpu[i] (m, cpu);
	}
	return 0;
}

static int aee_rr_mboot_params_proc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, aee_rr_reboot_reason_show, NULL);
}

static const struct proc_ops aee_rr_mboot_params_proc_fops = {
	.proc_open = aee_rr_mboot_params_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static void aee_rr_mboot_params_proc_init(void)
{
	struct proc_dir_entry *aee_rr_file;

	proc_mkdir("aed", NULL);

	aee_rr_file = proc_create("aed/reboot-reason", 0440, NULL,
			&aee_rr_mboot_params_proc_fops);
	if (!aee_rr_file)
		pr_notice("%s: Can't create rr proc entry\n", __func__);
}

#ifndef MODULE
static int __init mboot_params_proc_init(void)
{
	/* aed-main use /proc/aed/ too */
	aee_rr_mboot_params_proc_init();
	return 0;
}
arch_initcall(mboot_params_proc_init);
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek AED Driver");
MODULE_AUTHOR("MediaTek Inc.");
