/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __LOG_STORE_H__
#define __LOG_STORE_H__

#include <linux/types.h>


#define SRAM_HEADER_SIG (0x24013156)
#define DRAM_HEADER_SIG (0x5678ef90)
#define LOG_STORE_SIG (0xcdab3412)
#define LOG_EMMC_SIG (0x785690ef)
#define FLAG_DISABLE 0X44495341 // acsii-DISA
#define FLAG_ENABLE 0X454E454E // acsii-ENEN
#define FLAG_INVALID 0xdeaddead

#if IS_ENABLED(CONFIG_MTK_LOG_STORE_BOOTPROF)
#define FLAG_VERSION_0 0x76657248 /* total 2M size*/
#define FLAG_VERSION_1 0x76657231 /* bootlog 1M + logstore 4M*/
#endif
#define KEDUMP_ENABLE (1)
#define KEDUMP_DISABLE (0)

#define MAX_DRAM_COUNT	2

#define LOG_STORE_SIZE 0x40000	/*  DRAM buff 256KB*/

#define CONFIG_LONG_POWERKEY_LOG_STORE

/*  log flag */
#define BUFF_VALID      0x01
#define CAN_FREE		0x02
#define	NEED_SAVE_TO_EMMC	0x04
#define RING_BUFF		0x08
/* ring buf, if buf_full, buf_point is the start of the buf, */
/* else buf_point is the buf end, other buf is not used */
#define BUFF_FULL		0x10	/* buf is full */
/* array buf type, buf_point is the used buf end */
#define ARRAY_BUFF		0X20
#define BUFF_ALLOC_ERROR	0X40
#define BUFF_ERROR		0x80
#define BUFF_NOT_READY		0x100
#define BUFF_READY		0x200
/* pl or lk can printk the early printk information to uart cable */
#define BUFF_EARLY_PRINTK	0x400
#define	LOG_PL_LK  0x0	/* Preloader and lk log buff */

/* total 32 bytes <= u32(4 bytes) * 8 = 32 bytes */
struct pl_lk_log {
	u32 sig;            // default 0xabcd1234
	u32 buff_size;      // total buf size
	u32 off_pl;         // pl offset, sizeof(struct pl_lk_log)
	u32 sz_pl;          // preloader size
	u32 pl_flag;        // pl log flag
	u32 off_lk;         // lk offset, sizeof((struct pl_lk_log) + sz_pl
	u32 sz_lk;          // lk log size
	u32 lk_flag;        // lk log flag
};

/* total 52 bytes <= u32(4 bytes) * 7 + u64(8 bytes) * 3 = 52 bytes */
struct dram_buf_header {
	u32 sig;
	u32 flag;
	u64 buf_addr;
	u32 buf_size;
	u32 buf_offsize;
	u32 buf_point;
	u64 klog_addr;
	u32 klog_size;
	u64 atf_log_addr;
	u32 atf_log_len;
} __packed;

/* total 256 bytes */
struct sram_log_header {
	u32 sig;
	u32 reboot_count;
	u32 save_to_emmc;
	struct dram_buf_header dram_buf;        // 52 bytes
	struct pl_lk_log dram_curlog_header;    // 32 bytes
	u64 gz_log_addr;
	u32 gz_log_len;
	u32 reserve[37];                        // reserve 37 * 4 char size(148 bytes)
	/* reserve[0] sram_log record log size */
	/* reserve[1] save block size for kernel use*/
	/* reserve[2] pmic save boot phase enable/disable */
	/* reserve[3] save history boot phase */
	/* reserve[4] save pl/lk log size/point */
	/* reserve[5] expdb size version(0:2M,1:4M+1M) */
} __packed;
#define SRAM_RECORD_LOG_SIZE 0X00
#define SRAM_BLOCK_SIZE 0x01
#define SRAM_PMIC_BOOT_PHASE 0x02
#define SRAM_HISTORY_BOOT_PHASE 0x03
#if IS_ENABLED(CONFIG_MTK_LOG_STORE_BOOTPROF)
#define SRAM_PLLK_SIZE 0x04
#define SRAM_EXPDB_VER 0x05
#endif

/* emmc last block struct */
struct log_emmc_header {
	u32 sig;
	u32 offset;
	//u32 uart_flag;
	u32 reserve_flag[11];
	/* [0] used to save uart flag */
	/* [1] used to save emmc_log index */
	/* [2] used to save printk ratalimit  flag */
	/* [3] used to save kedump contrl flag */
	/* [4] used to save boot step */
	/* [5] expdb size version(0:2M,1:4M+1M)*/
	/* [6] boot prof offset*/
};

enum EMMC_STORE_FLAG_TYPE {
	UART_LOG = 0x00,
	LOG_INDEX = 0X01,
	PRINTK_RATELIMIT = 0X02,
	KEDUMP_CTL = 0x03,
	BOOT_STEP = 0x04,
	EXPDB_SIZE_VER = 0x05,
	BOOT_PROF_OFFSET = 0x06,
	EMMC_STORE_FLAG_TYPE_NR,
};

#define BOOT_PHASE_MASK	0xf		// b1111
#define NOW_BOOT_PHASE_SHIFT 0x0
#define LAST_BOOT_PHASE_SHIFT 0x4
#define PMIC_BOOT_PHASE_SHIFT 0x8
#define PMIC_LAST_BOOT_PHASE_SHIFT 0Xc

#define HEADER_INDEX_MAX 0x20
#define BOOT_TYPE_UFS 0x2

/* emmc store log */
struct emmc_log {
	u32 type;
	u32 start;
	u32 end;
};

struct mem_desc_ls {
	unsigned int addr;
	unsigned int size;
};

#define LOG_PLLK 0x01
#define LOG_PL 0x02
#define LOG_KERNEL 0x03
#define LOG_ATF 0x04
#define LOG_GZ 0x05
#define LOG_LAST_KERNEL 0x06
#define BOOT_PHASE_PL 0x01
#define BOOT_PHASE_LK 0x02
#define BOOT_PHASE_KERNEL 0x03
#define BOOT_PHASE_ANDROID 0x04
#define BOOT_PHASE_PL_COLD_REBOOT 0X05
#define BOOT_PHASE_SUSPEND 0x06
#define BOOT_PHASE_RESUME 0X07
#define BOOT_PHASE_PRE_SUSPEND 0x08
#define BOOT_PHASE_EXIT_RESUME 0X09


#if IS_ENABLED(CONFIG_MTK_DRAM_LOG_STORE)
bool get_pmic_interface(void);
u32 set_pmic_boot_phase(u32 boot_phase);
u32 get_pmic_boot_phase(void);
void store_log_to_emmc_enable(bool value);

#if IS_ENABLED(CONFIG_MTK_LOG_STORE_BOOTPROF)
/* log store write partition info*/
struct log_store_partition {
	struct block_device *bdev;
	unsigned int block_size;
	loff_t part_size;
	sector_t logstore_offset;     /* the offset of logstore in expdb */
	sector_t logindex_offset;     /* the offset of logindex in expdb */
	sector_t bootlog_offset;
	u32 bootlog_size;
	u32 logstore_size;
	char *bootbuff;
	u32 log_offset;
	u32 store_offset;
};

int set_emmc_config(int type, int value);
int read_emmc_config(struct log_emmc_header *log_header);
void close_monitor_thread(void);
#endif

void set_boot_phase(u32 step);
u32 get_last_boot_phase(void);
void log_store_bootup(void);
int logstore_reset(struct notifier_block *nb, unsigned long action, void *data);
int log_store_late_init(void);
void store_printk_buff(void);
void disable_early_log(void);
int dt_get_log_store(struct mem_desc_ls *data);
void *get_sram_header(void);
int log_store_sram_init(void);
#else
static inline bool get_pmic_interface(void)
{
	return true;
}

static inline u32 set_pmic_boot_phase(u32 boot_phase)
{
	return 0;
}

static inline u32 get_pmic_boot_phase(void)
{
	return 0;
}

static inline void store_log_to_emmc_enable(bool value)
{
}

#if IS_ENABLED(CONFIG_MTK_LOG_STORE_BOOTPROF)
static inline int set_emmc_config(int type, int value)
{
	return 0;
}

static inline int read_emmc_config(struct log_emmc_header *log_header)
{
	return 0;
}

static inline void close_monitor_thread(void)
{
}

#endif

static inline void set_boot_phase(u32 step)
{
}

static inline u32 get_last_boot_phase(void)
{
	return 0;
}

static inline int log_store_late_init(void)
{
	return 0;
}

static inline int logstore_reset(struct notifier_block *nb, unsigned long action, void *data)
{
	return 0;
}

static inline void  log_store_bootup(void)
{
}

static inline void store_printk_buff(void)
{
}

static inline void disable_early_log(void)
{
}

static inline int dt_get_log_store(struct mem_desc_ls *data)
{
	return 0;
}

static inline void *get_sram_header(void)
{
	return NULL;
}

static inline int log_store_sram_init(void)
{
	return 0;
}

#endif

#endif

