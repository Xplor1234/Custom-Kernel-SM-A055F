#ifndef CTS_CORE_H
#define CTS_CORE_H

#include "cts_config.h"
#include <asm/unaligned.h>
#include<linux/hardware_info.h>
#include <asm/setup.h>
#include <linux/input/tp_notifier.h>
// +S96818AA3-281,caoxin2.wt,add,20240927,add sec_tsp node
#include <linux/input/sec_cmd.h>

#define SEC_TSP_FACTORY_TEST
// -S96818AA3-281,caoxin2.wt,add,20240927,add sec_tsp node

extern char wt_saved_command_line[COMMAND_LINE_SIZE];
extern int cts_lcm_name;
#if IS_ENABLED(CONFIG_WT_PROJECT_S96818AA1) || IS_ENABLED(CONFIG_WT_PROJECT_S96818BA1)
extern bool n28_gesture_status;
#endif

enum cts_dev_hw_reg {
#if defined(CONFIG_CTS_ICTYPE_ICNL9922C) ||\
    defined(CONFIG_CTS_ICTYPE_ICNL9951)
    CTS_DEV_HW_REG_HARDWARE_ID     = 0x70000u,
    CTS_DEV_HW_REG_CLOCK_GATING    = 0x70004u,
    CTS_DEV_HW_REG_RESET_CONFIG    = 0x70008u,
    CTS_DEV_HW_REG_BOOT_MODE       = 0x70010u,
    CTS_DEV_HW_REG_CURRENT_MODE    = 0x70011u,
    CTS_DEV_HW_REG_DDI_REG_CTRL    = 0x7002Cu,
    CTS_DEV_HW_REG_CLK_DIV_CFG     = 0x70033u,
    CTS_DEV_HW_REG_HPPROG          = 0x70074u,
    CTS_DEV_HW_REG_SET_RESET       = 0x700F8u,
    CTS_DEV_HW_REG_HW_STATUS       = 0x7401Bu,
    CTS_DEV_HW_REG_I2C_CFG         = 0x77001u,
#else
    CTS_DEV_HW_REG_HARDWARE_ID     = 0x30000u,
    CTS_DEV_HW_REG_CLOCK_GATING    = 0x30004u,
    CTS_DEV_HW_REG_RESET_CONFIG    = 0x30008u,
    CTS_DEV_HW_REG_BOOT_MODE       = 0x30010u,
    CTS_DEV_HW_REG_CURRENT_MODE    = 0x30011u,
    CTS_DEV_HW_REG_DDI_REG_CTRL    = 0x3002Cu,
    CTS_DEV_HW_REG_CLK_DIV_CFG     = 0x30033u,
    CTS_DEV_HW_REG_HPPROG          = 0x30074u,
    CTS_DEV_HW_REG_SET_RESET       = 0x300F8u,
    CTS_DEV_HW_REG_HW_STATUS       = 0x3401Bu,
    CTS_DEV_HW_REG_I2C_CFG         = 0x37001u,
#endif
};

enum cts_dev_boot_mode {
    CTS_DEV_BOOT_MODE_IDLE          = 0,
    CTS_DEV_BOOT_MODE_FLASH         = 1,
    CTS_DEV_BOOT_MODE_I2C_PRG_9911C = 2,
    CTS_DEV_BOOT_MODE_TCH_PRG_9916  = 2,
    CTS_DEV_BOOT_MODE_SRAM          = 3,
    CTS_DEV_BOOT_MODE_DDI_PRG       = 4,
    CTS_DEV_BOOT_MODE_SPI_PRG_9911C = 5,
    CTS_DEV_BOOT_MODE_MASK          = 7,
};

/** I2C addresses(7bits), transfer size and bitrate */
#define CTS_DEV_NORMAL_MODE_I2CADDR         (0x48)
#define CTS_DEV_PROGRAM_MODE_I2CADDR        (0x30)
#define CTS_DEV_NORMAL_MODE_ADDR_WIDTH      (2)
#define CTS_DEV_PROGRAM_MODE_ADDR_WIDTH     (3)
#define CTS_DEV_NORMAL_MODE_SPIADDR         (0xF0)
#define CTS_DEV_PROGRAM_MODE_SPIADDR        (0x60)

/** Chipone firmware register addresses under normal mode */
enum cts_device_fw_reg {
    CTS_DEVICE_FW_REG_WORK_MODE = 0x0000,
    CTS_DEVICE_FW_REG_SYS_BUSY = 0x0001,
    CTS_DEVICE_FW_REG_DATA_READY = 0x0002,
    CTS_DEVICE_FW_REG_CMD = 0x0004,
    CTS_DEVICE_FW_REG_POWER_MODE = 0x0005,
    CTS_DEVICE_FW_REG_FW_LIB_MAIN_VERSION = 0x0009,
    CTS_DEVICE_FW_REG_CHIP_TYPE = 0x000A,
    CTS_DEVICE_FW_REG_VERSION = 0x000C,
    CTS_DEVICE_FW_REG_DDI_VERSION = 0x0010,
    CTS_DEVICE_FW_REG_GET_WORK_MODE = 0x003F,
    CTS_DEVICE_FW_REG_FW_LIB_SUB_VERSION = 0x0047,
    CTS_DEVICE_FW_REG_COMPENSATE_CAP_READY = 0x004E,

    CTS_DEVICE_FW_REG_TOUCH_INFO = 0x1000,
    CTS_DEVICE_FW_REG_RAW_DATA = 0x2000,
    CTS_DEVICE_FW_REG_DIFF_DATA = 0x3000,
    CTS_DEVICE_FW_REG_GESTURE_INFO = 0x7000,

    CTS_DEVICE_FW_REG_PANEL_PARAM = 0x8000,
    CTS_DEVICE_FW_REG_NUM_TX = 0x8007,
    CTS_DEVICE_FW_REG_NUM_RX = 0x8008,
    CTS_DEVICE_FW_REG_INT_KEEP_TIME = 0x8047,
    CTS_DEVICE_FW_REG_RAWDATA_TARGET = 0x8049,
    CTS_DEVICE_FW_REG_X_RESOLUTION = 0x8090,
    CTS_DEVICE_FW_REG_Y_RESOLUTION = 0x8092,
    CTS_DEVICE_FW_REG_SWAP_AXES = 0x8094,
    CTS_DEVICE_FW_REG_GLOVE_MODE = 0x8095,
    CTS_DEVICE_FW_REG_TEST_WITH_DISPLAY_ON = 0x80A3,
    CTS_DEVICE_FW_REG_INT_MODE = 0x80D8,
    CTS_DEVICE_FW_REG_EARJACK_DETECT_SUPP = 0x8113,
    CTS_DEVICE_FW_REG_AUTO_COMPENSATE_EN = 0x8114,
    CTS_DEVICE_FW_REG_ESD_PROTECTION = 0x8156,
    CTS_DEVICE_FW_REG_FLAG_BITS = 0x8158,

    CTS_DEVICE_FW_REG_COMPENSATE_CAP = 0xA000,
    CTS_DEVICE_FW_REG_DEBUG_INTF = 0xF000,
};

/** Hardware IDs, read from hardware id register */
enum cts_dev_hwid {
    CTS_DEV_HWID_ICNL9911  = 0x990100u,
    CTS_DEV_HWID_ICNL9911S = 0x990110u,
    CTS_DEV_HWID_ICNL9911C = 0x991110u,
    CTS_DEV_HWID_ICNL9916  = 0x990160u,
    CTS_DEV_HWID_ICNL9916C = 0x99C160u,
    CTS_DEV_HWID_ICNL9922  = 0x990220u,
    CTS_DEV_HWID_ICNL9922C = 0x99C220u,
    CTS_DEV_HWID_ICNL9951  = 0x990510u,

    CTS_DEV_HWID_ANY       = 0,
    CTS_DEV_HWID_INVALID   = 0xFFFFFFFFu,
};

/* Firmware IDs, read from firmware register @ref CTS_DEV_FW_REG_CHIP_TYPE
 * under normal mode
 */
enum cts_dev_fwid {
    CTS_DEV_FWID_ICNL9911  = 0x9911u,
    CTS_DEV_FWID_ICNL9911S = 0x9964u,
    CTS_DEV_FWID_ICNL9911C = 0x9954u,
    CTS_DEV_FWID_ICNL9916  = 0x9916u,
    CTS_DEV_FWID_ICNL9916C = 0x9959u,
    CTS_DEV_FWID_ICNL9922  = 0x9922u,
    CTS_DEV_FWID_ICNL9922C = 0x9965u,
    CTS_DEV_FWID_ICNL9951  = 0x9951u,

    CTS_DEV_FWID_ANY       = 0u,
    CTS_DEV_FWID_INVALID   = 0xFFFFu
};

/** Commands written to firmware register @ref CTS_DEVICE_FW_REG_CMD under normal mode */
enum cts_firmware_cmd {
    CTS_CMD_RESET = 1,
    CTS_CMD_SUSPEND = 2,
    CTS_CMD_ENTER_WRITE_PARA_TO_FLASH_MODE = 3,
    CTS_CMD_WRITE_PARA_TO_FLASH = 4,
    CTS_CMD_WRTITE_INT_HIGH = 5,
    CTS_CMD_WRTITE_INT_LOW = 6,
    CTS_CMD_RELASE_INT_TEST = 7,
    CTS_CMD_RECOVERY_TX_VOL = 0x10,
    CTS_CMD_DEC_TX_VOL_1 = 0x11,
    CTS_CMD_DEC_TX_VOL_2 = 0x12,
    CTS_CMD_DEC_TX_VOL_3 = 0x13,
    CTS_CMD_DEC_TX_VOL_4 = 0x14,
    CTS_CMD_DEC_TX_VOL_5 = 0x15,
    CTS_CMD_DEC_TX_VOL_6 = 0x16,
    CTS_CMD_ENABLE_READ_RAWDATA = 0x20,
    CTS_CMD_DISABLE_READ_RAWDATA = 0x21,
    CTS_CMD_SUSPEND_WITH_GESTURE = 0x40,
    CTS_CMD_QUIT_GESTURE_MONITOR = 0x41,
    CTS_CMD_CHARGER_ATTACHED = 0x55,
    CTS_CMD_EARJACK_ATTACHED = 0x57,
    CTS_CMD_EARJACK_DETACHED = 0x58,
    CTS_CMD_CHARGER_DETACHED = 0x66,
    CTS_CMD_ENABLE_FW_LOG_REDIRECT = 0x86,
    CTS_CMD_DISABLE_FW_LOG_REDIRECT = 0x87,
    CTS_CMD_ENABLE_READ_CNEG = 0x88,
    CTS_CMD_DISABLE_READ_CNEG = 0x89,
    CTS_CMD_FW_LOG_SHOW_FINISH = 0xE0,

#ifdef CONFIG_CTS_TP_PROXIMITY
    CTS_CMD_PROXIMITY_STATUS = 0x80,
    /* For vivo code, make a distinction between palm func and proximity func */
    CTS_CMD_PROXIMITY_NEAR_VIVO = 0x30,
    CTS_CMD_PROXIMITY_FAR_VIVO = 0x31,
#endif
};

#pragma pack(1)
/** Touch message read back from chip */
struct cts_device_touch_msg {
    u8 id;
    __le16 x;
    __le16 y;
    u8 pressure;
    u8 event;

#define CTS_DEVICE_TOUCH_EVENT_NONE         (0)
#define CTS_DEVICE_TOUCH_EVENT_DOWN         (1)
#define CTS_DEVICE_TOUCH_EVENT_MOVE         (2)
#define CTS_DEVICE_TOUCH_EVENT_STAY         (3)
#define CTS_DEVICE_TOUCH_EVENT_UP           (4)

};

struct cts_firmware_status {
    uint16_t charger:1;
    uint16_t proximity:1;
    uint16_t earjack:1;
    uint16_t knuckle:1;
    uint16_t glove:1;
    uint16_t pocket:1;
    uint16_t game:1;
    uint16_t reserved:1;
    uint16_t high_temperature:1;
    uint16_t low_temperature:1;
    uint16_t bend_mode:1;
    uint16_t palm_status:1;
    uint16_t noise_mode:1;
    uint16_t base_trace:1;
    uint16_t water_mode:1;
    uint16_t ground:1;
};
/** Touch debug messages read back from chip [40 bytes] */
struct cts_touch_debug_msg {
    u8 protocol_version;
    u8 fw_version;
    u32 reset_flag;
    u16 frame_index;
    u16 firmware_status;

    u8 proximity;
    u8 work_mode;
    u8 power_mode;
    u8 curr_freq;
    u8 esd_0a_status;
    u8 fw_esd_status;
    u8 landscape_mode;
    u16 curr_freq_noise;
    u16 max_diff;
    u8 max_diff_row;
    u8 max_diff_col;
    u16 max_neg_diff;
    u8 max_neg_diff_row;
    u8 max_neg_diff_col;
    u8 data_method;
    u8 data_type;
    u8 reserved[11];
};

/** Touch information read back from chip */
struct cts_device_touch_info {
    u8 vkey_state;
    u8 num_msg;

    struct cts_device_touch_msg msgs[CFG_CTS_MAX_TOUCH_NUM];
    struct cts_touch_debug_msg debug_msg;
};

/** Gesture trace point read back from chip */
struct cts_device_gesture_point {
    __le16 x;
    __le16 y;
};

/** Gesture information read back from chip */
/* total size 112 bytes */
struct cts_device_gesture_info {
    u8 gesture_id;
#define CTS_GESTURE_UP                  (0x11)
#define CTS_GESTURE_C                   (0x12)
#define CTS_GESTURE_O                   (0x13)
#define CTS_GESTURE_M                   (0x14)
#define CTS_GESTURE_W                   (0x15)
#define CTS_GESTURE_E                   (0x16)
#define CTS_GESTURE_S                   (0x17)
#define CTS_GESTURE_B                   (0x18)
#define CTS_GESTURE_T                   (0x19)
#define CTS_GESTURE_H                   (0x1A)
#define CTS_GESTURE_F                   (0x1B)
#define CTS_GESTURE_X                   (0x1C)
#define CTS_GESTURE_Z                   (0x1D)
#define CTS_GESTURE_V                   (0x1E)
#define CTS_GESTURE_D_TAP               (0x50)
#define CTS_GESTURE_TAP                 (0x7F)

    u8 num_points;

#define CTS_CHIP_MAX_GESTURE_TRACE_POINT    (27u)
    struct cts_device_gesture_point points[CTS_CHIP_MAX_GESTURE_TRACE_POINT];
    u8 reserved[2];

};

struct cts_tcs_cmd {
    uint16_t cmd_id : 8;
    uint16_t class_id : 5;
    uint16_t is_write : 1;
    uint16_t is_read : 1;
    uint16_t base_flag : 1;
};
#pragma pack()

struct cts_device;

enum cts_crc_type {
    CTS_CRC16 = 1,
    CTS_CRC32 = 2,
};

/** Chip hardware data, will never change */
struct cts_device_hwdata {
    const char *name;
    u32 hwid;
    u16 fwid;
    u8 num_row;
    u8 num_col;
    u32 sram_size;

    /* Address width under program mode */
    u8 program_addr_width;

    const struct cts_sfctrl *sfctrl;
    int (*enable_access_ddi_reg)(struct cts_device *cts_dev, bool enable);
};

enum int_data_type {
    INT_DATA_TYPE_NONE = 0,
    INT_DATA_TYPE_RAWDATA = BIT(0),
    INT_DATA_TYPE_MANUAL_DIFF = BIT(1),
    INT_DATA_TYPE_REAL_DIFF = BIT(2),
    INT_DATA_TYPE_NOISE_DIFF = BIT(3),
    INT_DATA_TYPE_BASEDATA = BIT(4),
    INT_DATA_TYPE_CNEGDATA = BIT(5),
    INT_DATA_TYPE_MASK = 0x3F,
};

enum int_data_method {
    INT_DATA_METHOD_NONE = 0,
    INT_DATA_METHOD_HOST = 1,
    INT_DATA_METHOD_POLLING = 2,
    INT_DATA_METHOD_DEBUG = 3,
    INT_DATA_METHOD_CNT = 4,
};

/** Chip firmware data */
struct cts_device_fwdata {
    u16 version;
    u16 res_x;
    u16 res_y;
    u8 rows;
    u8 cols;
    bool flip_x;
    bool flip_y;
    bool swap_axes;
    u8 ddi_version;
    u8 int_mode;
    u8 esd_method;
    u16 lib_version;
    u16 int_keep_time;
    u16 rawdata_target;
    bool has_int_data;
    u8 int_data_method;
    u16 int_data_types;
    size_t int_data_size;
};

/** Chip runtime data */
struct cts_device_rtdata {
    u8 slave_addr;
    int addr_width;
    bool program_mode;
    bool has_flash;

    bool suspended;
    bool updating;
    bool testing;
    bool update_work_run;

    bool gesture_wakeup_enabled;
    bool charger_exist;
    bool fw_log_redirect_enabled;
    bool glove_mode_enabled;

    u8 esd_count;
    u16 firmware_status;

    struct cts_device_touch_info touch_info;
    struct cts_device_gesture_info gesture_info;

#ifdef CONFIG_CTS_TP_PROXIMITY
    bool proximity_status;
    int proximity_num;
#endif

    u16 tcscmd[ALIGN(PAGE_SIZE + 10, 4)];
    int tcscmd_len;
    u16 curr_cmd;
    int curr_len;
};

struct cts_device {
    struct cts_platform_data *pdata;

    const struct cts_device_hwdata *hwdata;
    struct cts_device_fwdata fwdata;
    struct cts_device_rtdata rtdata;
    const struct cts_flash *flash;
    bool enabled;
#ifdef CFG_CTS_FW_UPDATE_FILE_LOAD
    char config_fw_name[CFG_CTS_FW_FILE_NAME_MAX_LEN + 1];
#endif
    u8 int_data[ALIGN(CFG_CTS_INT_DATA_MAX_SIZE + 10, 4)];
};

struct cts_platform_data;

struct chipone_ts_data {
#ifdef CONFIG_CTS_I2C_HOST
    struct i2c_client *i2c_client;
#else
    struct spi_device *spi_client;
#endif
    struct device *device;
    struct cts_device cts_dev;
    struct cts_platform_data *pdata;
    struct workqueue_struct *workqueue;
    struct delayed_work fw_upgrade_work;
    struct work_struct ts_resume_work;
    struct work_struct update_plug_status_work;
    int usb_plug_status;
#ifdef CONFIG_CTS_CHARGER_DETECT
    void *charger_detect_data;
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    void *earjack_detect_data;
#endif

#ifdef CONFIG_CTS_ESD_PROTECTION
    struct workqueue_struct *esd_workqueue;
    struct delayed_work esd_work;
    bool esd_enabled;
    int esd_check_fail_cnt;
#endif

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
    struct workqueue_struct *heart_workqueue;
    struct delayed_work heart_work;
#endif

#ifdef CONFIG_CTS_LEGACY_TOOL
    struct proc_dir_entry *procfs_entry;
#endif

    void *oem_data;

    bool force_reflash;
    struct kobject *suspend_kobj;
// +S96818AA3-281,caoxin2.wt,add,20240927,add sec_tsp node
#ifdef SEC_TSP_FACTORY_TEST
    struct sec_cmd_data sec;
#endif
// -S96818AA3-281,caoxin2.wt,add,20240927,add sec_tsp node
    struct completion pm_completion;
    bool pm_suspend;
    struct wakeup_source *ws;
};

/* static inline u32 get_unaligned_le24(const void *p)
{
    const u8 *puc = (const u8 *)p;
    return (puc[0] | (puc[1] << 8) | (puc[2] << 16));
}

static inline u32 get_unaligned_be24(const void *p)
{
    const u8 *puc = (const u8 *)p;
    return (puc[2] | (puc[1] << 8) | (puc[0] << 16));
}

static inline void put_unaligned_be24(u32 v, void *p)
{
    u8 *puc = (u8 *) p;

    puc[0] = (v >> 16) & 0xFF;
    puc[1] = (v >> 8) & 0xFF;
    puc[2] = (v >> 0) & 0xFF;
} */

#define wrap(max, x)        ((max) - 1 - (x))

extern void cts_lock_device(const struct cts_device *cts_dev);
extern void cts_unlock_device(const struct cts_device *cts_dev);

extern int cts_dev_readb(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay);
extern int cts_dev_writeb(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay);

extern int cts_sram_writeb_retry(const struct cts_device *cts_dev,
        u32 addr, u8 b, int retry, int delay);
extern int cts_sram_writew_retry(const struct cts_device *cts_dev,
        u32 addr, u16 w, int retry, int delay);
extern int cts_sram_writel_retry(const struct cts_device *cts_dev,
        u32 addr, u32 l, int retry, int delay);
extern int cts_sram_writesb_retry(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, int retry, int delay);
extern int cts_sram_writesb_check_crc_retry(const struct cts_device *cts_dev,
        u32 addr, const void *src, size_t len, u32 crc, int retry);

extern int cts_sram_readb_retry(const struct cts_device *cts_dev,
        u32 addr, u8 *b, int retry, int delay);
extern int cts_sram_readw_retry(const struct cts_device *cts_dev,
        u32 addr, u16 *w, int retry, int delay);
extern int cts_sram_readl_retry(const struct cts_device *cts_dev,
        u32 addr, u32 *l, int retry, int delay);
extern int cts_sram_readsb_retry(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay);

extern int cts_fw_reg_writeb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 b, int retry, int delay);
extern int cts_fw_reg_writew_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 w, int retry, int delay);
extern int cts_fw_reg_writel_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 l, int retry, int delay);
extern int cts_fw_reg_writesb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, const void *src, size_t len, int retry, int delay);

extern int cts_fw_reg_readb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 *b, int retry, int delay);
extern int cts_fw_reg_readw_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 *w, int retry, int delay);
extern int cts_fw_reg_readl_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 *l, int retry, int delay);
extern int cts_fw_reg_readsb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len,
        int retry, int delay);
extern int cts_fw_reg_readsb_retry_delay_idle(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int retry, int delay, int idle);

extern int cts_hw_reg_writeb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 b, int retry, int delay);
extern int cts_hw_reg_writew_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 w, int retry, int delay);
extern int cts_hw_reg_writel_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 l, int retry, int delay);
extern int cts_hw_reg_writesb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, const void *src, size_t len, int retry, int delay);

extern int cts_hw_reg_readb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u8 *b, int retry, int delay);
extern int cts_hw_reg_readw_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u16 *w, int retry, int delay);
extern int cts_hw_reg_readl_retry(const struct cts_device *cts_dev,
        u32 reg_addr, u32 *l, int retry, int delay);
extern int cts_hw_reg_readsb_retry(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int retry, int delay);

static inline int cts_hw_reg_readb(const struct cts_device *cts_dev,
        u32 reg_addr, u8 *b)
{
    return cts_hw_reg_readb_retry(cts_dev, reg_addr, b, 1, 0);
}

static inline int cts_hw_reg_writeb(const struct cts_device *cts_dev,
        u32 reg_addr, u8 b)
{
    return cts_hw_reg_writeb_retry(cts_dev, reg_addr, b, 1, 0);
}

static inline int cts_hw_reg_writew(const struct cts_device *cts_dev,
        u32 reg_addr, u16 w)
{
    return cts_hw_reg_writew_retry(cts_dev, reg_addr, w, 1, 0);
}

static inline int cts_fw_reg_writeb(const struct cts_device *cts_dev,
        u32 reg_addr, u8 b)
{
    return cts_fw_reg_writeb_retry(cts_dev, reg_addr, b, 1, 0);
}

static inline int cts_fw_reg_writew(const struct cts_device *cts_dev,
        u32 reg_addr, u16 w)
{
    return cts_fw_reg_writew_retry(cts_dev, reg_addr, w, 1, 0);
}

static inline int cts_fw_reg_writel(const struct cts_device *cts_dev,
        u32 reg_addr, u32 l)
{
    return cts_fw_reg_writel_retry(cts_dev, reg_addr, l, 1, 0);
}

static inline int cts_fw_reg_writesb(const struct cts_device *cts_dev,
        u32 reg_addr, const void *src, size_t len)
{
    return cts_fw_reg_writesb_retry(cts_dev, reg_addr, src, len, 1, 0);
}

static inline int cts_fw_reg_readb(const struct cts_device *cts_dev,
        u32 reg_addr, u8 *b)
{
    return cts_fw_reg_readb_retry(cts_dev, reg_addr, b, 1, 0);
}

static inline int cts_fw_reg_readw(const struct cts_device *cts_dev,
        u32 reg_addr, u16 *w)
{
    return cts_fw_reg_readw_retry(cts_dev, reg_addr, w, 1, 0);
}

static inline int cts_fw_reg_readl(const struct cts_device *cts_dev,
        u32 reg_addr, u32 *l)
{
    return cts_fw_reg_readl_retry(cts_dev, reg_addr, l, 1, 0);
}

static inline int cts_fw_reg_readsb(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len)
{
    return cts_fw_reg_readsb_retry(cts_dev, reg_addr, dst, len, 1, 0);
}

static inline int cts_fw_reg_readsb_delay_idle(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len, int idle)
{
    return cts_fw_reg_readsb_retry_delay_idle(cts_dev, reg_addr, dst, len,
            1, 0, idle);
}

static inline int cts_hw_reg_writeb_relaxed(const struct cts_device *cts_dev,
        u32 reg_addr, u8 b)
{
    return cts_hw_reg_writeb_retry(cts_dev, reg_addr, b, 1, 0);
}

static inline int cts_hw_reg_writew_relaxed(const struct cts_device *cts_dev,
        u32 reg_addr, u16 w)
{
    return cts_hw_reg_writew_retry(cts_dev, reg_addr, w, 1, 0);
}

static inline int cts_hw_reg_writel_relaxed(const struct cts_device *cts_dev,
        u32 reg_addr, u32 l)
{
    return cts_hw_reg_writel_retry(cts_dev, reg_addr, l, 1, 0);
}

static inline int cts_hw_reg_writesb(const struct cts_device *cts_dev,
        u32 reg_addr, const void *src, size_t len)
{
    return cts_hw_reg_writesb_retry(cts_dev, reg_addr, src, len, 1, 0);
}

static inline int cts_hw_reg_readb_relaxed(const struct cts_device *cts_dev,
        u32 reg_addr, u8 *b)
{
    return cts_hw_reg_readb_retry(cts_dev, reg_addr, b, 1, 0);
}

static inline int cts_hw_reg_readw_relaxed(const struct cts_device *cts_dev,
        u32 reg_addr, u16 *w)
{
    return cts_hw_reg_readw_retry(cts_dev, reg_addr, w, 1, 0);
}

static inline int cts_hw_reg_readl_relaxed(const struct cts_device *cts_dev,
        u32 reg_addr, u32 *l)
{
    return cts_hw_reg_readl_retry(cts_dev, reg_addr, l, 1, 0);
}

static inline int cts_hw_reg_readsb(const struct cts_device *cts_dev,
        u32 reg_addr, void *dst, size_t len)
{
    return cts_hw_reg_readsb_retry(cts_dev, reg_addr, dst, len, 1, 0);
}

static inline int cts_sram_writeb(const struct cts_device *cts_dev,
        u32 addr, u8 b)
{
    return cts_sram_writeb_retry(cts_dev, addr, b, 1, 0);
}

static inline int cts_sram_writew(const struct cts_device *cts_dev,
        u32 addr, u16 w)
{
    return cts_sram_writew_retry(cts_dev, addr, w, 1, 0);
}

static inline int cts_sram_writel(const struct cts_device *cts_dev,
        u32 addr, u32 l)
{
    return cts_sram_writel_retry(cts_dev, addr, l, 1, 0);
}

static inline int cts_sram_writesb(const struct cts_device *cts_dev, u32 addr,
        const void *src, size_t len)
{
    return cts_sram_writesb_retry(cts_dev, addr, src, len, 1, 0);
}

static inline int cts_sram_readb(const struct cts_device *cts_dev,
        u32 addr, u8 *b)
{
    return cts_sram_readb_retry(cts_dev, addr, b, 1, 0);
}

static inline int cts_sram_readw(const struct cts_device *cts_dev,
        u32 addr, u16 *w)
{
    return cts_sram_readw_retry(cts_dev, addr, w, 1, 0);
}

static inline int cts_sram_readl(const struct cts_device *cts_dev,
        u32 addr, u32 *l)
{
    return cts_sram_readl_retry(cts_dev, addr, l, 1, 0);
}

static inline int cts_sram_readsb(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len)
{
    return cts_sram_readsb_retry(cts_dev, addr, dst, len, 1, 0);
}

#ifdef CONFIG_CTS_I2C_HOST
static inline void cts_set_program_addr(struct cts_device *cts_dev)
{
    cts_dev->rtdata.slave_addr = CTS_DEV_PROGRAM_MODE_I2CADDR;
    cts_dev->rtdata.program_mode = true;
    cts_dev->rtdata.addr_width = CTS_DEV_PROGRAM_MODE_ADDR_WIDTH;
}

static inline void cts_set_normal_addr(struct cts_device *cts_dev)
{
    cts_dev->rtdata.slave_addr = CTS_DEV_NORMAL_MODE_I2CADDR;
    cts_dev->rtdata.program_mode = false;
    cts_dev->rtdata.addr_width = CTS_DEV_NORMAL_MODE_ADDR_WIDTH;
}
#else
static inline void cts_set_program_addr(struct cts_device *cts_dev)
{
    cts_dev->rtdata.slave_addr = CTS_DEV_PROGRAM_MODE_SPIADDR;
    cts_dev->rtdata.program_mode = true;
    cts_dev->rtdata.addr_width = CTS_DEV_PROGRAM_MODE_ADDR_WIDTH;
}

static inline void cts_set_normal_addr(struct cts_device *cts_dev)
{
    cts_dev->rtdata.slave_addr = CTS_DEV_NORMAL_MODE_SPIADDR;
    cts_dev->rtdata.program_mode = false;
    cts_dev->rtdata.addr_width = CTS_DEV_NORMAL_MODE_ADDR_WIDTH;
}
#endif

extern int cts_irq_handler(struct cts_device *cts_dev);
extern void cts_firmware_upgrade_work(struct work_struct *work);

extern bool cts_is_device_suspended(const struct cts_device *cts_dev);
extern int cts_suspend_device(struct cts_device *cts_dev);
extern int cts_resume_device(struct cts_device *cts_dev);

extern bool cts_is_device_program_mode(const struct cts_device *cts_dev);
extern int cts_enter_program_mode(struct cts_device *cts_dev);
extern int cts_enter_normal_mode(struct cts_device *cts_dev);

extern int cts_probe_device(struct cts_device *cts_dev);
extern int cts_send_command(const struct cts_device *cts_dev, u8 cmd);

extern int cts_get_hwid(struct cts_device *cts_dev, u32 *hwid);
extern int cts_read_sram_normal_mode(const struct cts_device *cts_dev,
        u32 addr, void *dst, size_t len, int retry, int delay);

#ifdef CFG_CTS_GESTURE
extern void cts_enable_gesture_wakeup(struct cts_device *cts_dev);
extern void cts_disable_gesture_wakeup(struct cts_device *cts_dev);
extern bool cts_is_gesture_wakeup_enabled(const struct cts_device *cts_dev);
extern int cts_get_gesture_info(struct cts_device *cts_dev, void *gesture_info);
#endif /* CFG_CTS_GESTURE */

extern int cts_set_int_data_types(struct cts_device *cts_dev, u16 types);
extern int cts_set_int_data_method(struct cts_device *cts_dev, u8 method);

#ifdef CONFIG_CTS_ESD_PROTECTION
extern void cts_init_esd_protection(struct chipone_ts_data *cts_data);
extern void cts_enable_esd_protection(struct chipone_ts_data *cts_data);
extern void cts_disable_esd_protection(struct chipone_ts_data *cts_data);
extern void cts_deinit_esd_protection(struct chipone_ts_data *cts_data);
#else /* CONFIG_CTS_ESD_PROTECTION */
static inline void cts_init_esd_protection(struct chipone_ts_data *cts_data)
{
}

static inline void cts_enable_esd_protection(struct chipone_ts_data *cts_data)
{
}

static inline void cts_disable_esd_protection(struct chipone_ts_data *cts_data)
{
}

static inline void cts_deinit_esd_protection(struct chipone_ts_data *cts_data)
{
}
#endif /* CONFIG_CTS_ESD_PROTECTION  */

#ifdef CFG_CTS_HEARTBEAT_MECHANISM
void cts_show_touch_debug_msg(void *debug);
void cts_heartbeat_mechanism_work(struct work_struct *work);
void cts_enable_heartbeat_mechanism(struct chipone_ts_data *cts_data);
void cts_disable_heartbeat_mechanism(struct chipone_ts_data *cts_data);
#endif

#ifdef CONFIG_CTS_GLOVE
extern int cts_enter_glove_mode(struct cts_device *cts_dev);
extern int cts_exit_glove_mode(struct cts_device *cts_dev);
int cts_is_glove_enabled(struct cts_device *cts_dev);
extern int cts_tcs_set_glove_mode(struct cts_device *cts_dev, u8 enable);
#else
static inline int cts_enter_glove_mode(struct cts_device *cts_dev)
{
    return 0;
}

static inline int cts_exit_glove_mode(struct cts_device *cts_dev)
{
    return 0;
}

static inline int cts_is_glove_enabled(struct cts_device *cts_dev)
{
    return 0;
}
#endif

#ifdef CONFIG_CTS_CHARGER_DETECT
extern bool cts_is_charger_exist(struct cts_device *cts_dev);
extern int cts_set_dev_charger_attached(struct cts_device *cts_dev,
        bool attached);
#else /* CONFIG_CTS_CHARGER_DETECT */
static inline bool cts_is_charger_exist(struct cts_device *cts_dev)
{
    return false;
}

static inline int cts_set_dev_charger_attached(struct cts_device *cts_dev,
        bool attached)
{
    return 0;
}
#endif /* CONFIG_CTS_CHARGER_DETECT */

#ifdef CONFIG_CTS_EARJACK_DETECT
extern bool cts_is_earjack_exist(struct cts_device *cts_dev);
extern int cts_set_dev_earjack_attached(struct cts_device *cts_dev, bool attached);
#else /* CONFIG_CTS_EARJACK_DETECT */
static inline bool cts_is_earjack_exist(struct cts_device *cts_dev)
{
    return false;
}
static inline int cts_set_dev_earjack_attached(struct cts_device *cts_dev, bool attached)
{
    return 0;
}
#endif /* CONFIG_CTS_EARJACK_DETECT */

#ifdef CONFIG_CTS_LEGACY_TOOL
extern int cts_tool_init(struct chipone_ts_data *cts_data);
extern void cts_tool_deinit(struct chipone_ts_data *data);
#else /* CONFIG_CTS_LEGACY_TOOL */
static inline int cts_tool_init(struct chipone_ts_data *cts_data)
{
    return 0;
}

static inline void cts_tool_deinit(struct chipone_ts_data *data)
{
}
#endif /* CONFIG_CTS_LEGACY_TOOL */

extern bool cts_is_device_enabled(const struct cts_device *cts_dev);
extern int cts_start_device(struct cts_device *cts_dev);
extern int cts_stop_device(struct cts_device *cts_dev);

#ifdef CFG_CTS_FW_LOG_REDIRECT
extern int cts_enable_fw_log_redirect(struct cts_device *cts_dev);
extern int cts_disable_fw_log_redirect(struct cts_device *cts_dev);
extern bool cts_is_fw_log_redirect(struct cts_device *cts_dev);
extern int cts_fw_log_show_finish(struct cts_device *cts_dev);
#endif

#ifdef CFG_CTS_UPDATE_CRCCHECK
extern int cts_sram_writesb_boot_crc_retry(const struct cts_device *cts_dev,
        size_t len, u32 crc, int retry);
#endif

extern const char *cts_dev_boot_mode2str(u8 boot_mode);
extern bool cts_is_fwid_valid(u16 fwid);

extern int cts_reset_device(struct cts_device *cts_dev);

extern int kstrtobool(const char *s, bool *res);
#endif /* CTS_CORE_H */
