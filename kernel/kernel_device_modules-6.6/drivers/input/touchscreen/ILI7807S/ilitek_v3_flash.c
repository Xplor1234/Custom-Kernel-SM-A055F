/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "ilitek_v3.h"
#include "ilitek_v3_flash.h"

#define UPDATE_PASS		0
#define UPDATE_FAIL		-1
#define TIMEOUT_SECTOR		500
#define TIMEOUT_PAGE		3500
#define TIMEOUT_PROGRAM		50
#define TIMEOUT_PROTECT 	100

static struct touch_fw_data {
	u8 block_number;
	u32 start_addr;
	u32 end_addr;
	u32 new_fw_cb;
	u32 new_mp_fw_cb;
	int delay_after_upgrade;
	bool isCRC;
	bool isboot;
	bool is80k;
	int hex_tag;
} tfd;

static struct flash_block_info {
	char *name;
	u32 start;
	u32 end;
	u32 len;
	u32 mem_start;
	u32 fix_mem_start;
	u8 mode;
} fbi[FW_BLOCK_INFO_NUM], fbi_b1[FW_BLOCK_INFO_B1_NUM];

static u8 fw_flash_tmp[4*K + 7];
static u8 *pfw;
static u8 *CTPM_FW;

static u32 HexToDec(char *phex, s32 len)
{
	u32 ret = 0, temp = 0, i;
	s32 shift = (len - 1) * 4;

	for (i = 0; i < len; shift -= 4, i++) {
		if ((phex[i] >= '0') && (phex[i] <= '9'))
			temp = phex[i] - '0';
		else if ((phex[i] >= 'a') && (phex[i] <= 'f'))
			temp = (phex[i] - 'a') + 10;
		else if ((phex[i] >= 'A') && (phex[i] <= 'F'))
			temp = (phex[i] - 'A') + 10;

		ret |= (temp << shift);
	}
	return ret;
}

static int CalculateCRC32(u32 start_addr, u32 len, u8 *pfw)
{
	int i = 0, j = 0;
	int crc_poly = 0x04C11DB7;
	int tmp_crc = 0xFFFFFFFF;

	for (i = start_addr; i < start_addr + len; i++) {
		tmp_crc ^= (pfw[i] << 24);

		for (j = 0; j < 8; j++) {
			if ((tmp_crc & 0x80000000) != 0)
				tmp_crc = tmp_crc << 1 ^ crc_poly;
			else
				tmp_crc = tmp_crc << 1;
		}
	}
	return tmp_crc;
}

static int ilitek_tddi_fw_iram_read(u8 *buf, u32 start, int len)
{
	int limit = 4*K;
	int addr = 0, loop = 0, tmp_len = len, cnt = 0;
	u8 cmd[4] = {0};

	if (!buf) {
		ILI_ERR("buf is null\n");
		return -ENOMEM;
	}

	if (len % limit)
		loop = (len / limit) + 1;
	else
		loop = len / limit;

	for (cnt = 0, addr = start; cnt < loop; cnt++, addr += limit) {
		tmp_len = len - cnt * limit;
		ilits->fw_update_stat = ((len - tmp_len) * 100) / len;
		ILI_DBG("Reading iram data .... %d%c", ilits->fw_update_stat, '%');

		if (tmp_len > limit)
			tmp_len = limit;

		cmd[0] = 0x25;
		cmd[3] = (char)((addr & 0x00FF0000) >> 16);
		cmd[2] = (char)((addr & 0x0000FF00) >> 8);
		cmd[1] = (char)((addr & 0x000000FF));

		if (ilits->wrapper(cmd, 4, NULL, 0, OFF, OFF) < 0) {
			ILI_ERR("Failed to write iram data\n");
			return -ENODEV;
		}

		if (ilits->wrapper(NULL, 0, buf + cnt * limit, tmp_len, OFF, OFF) < 0) {
			ILI_ERR("Failed to Read iram data\n");
			return -ENODEV;
		}

	}
	return 0;
}

int ili_fw_dump_iram_data(u32 start, u32 end, bool save, bool mcu)
{
#if GENERIC_KERNEL_IMAGE
	int tmp = debug_en;
#else
	struct file *f = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)
	mm_segment_t old_fs;
#endif
	loff_t pos = 0;
#endif
	int i, ret, len;
	u8 *fw_buf = NULL;

	if (ili_ice_mode_ctrl(ENABLE, mcu) < 0) {
		ILI_ERR("Enable ice mode failed\n");
		ret = -1;
		goto out;
	}

	len = end - start + 1;

	if (len > MAX_HEX_FILE_SIZE) {
		ILI_ERR("len is larger than buffer, abort\n");
		ret = -EINVAL;
		goto out;
	}

	fw_buf = kzalloc(MAX_HEX_FILE_SIZE, GFP_KERNEL);
	if (ERR_ALLOC_MEM(fw_buf)) {
		ILI_ERR("Failed to allocate update_buf\n");
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < MAX_HEX_FILE_SIZE; i++)
		fw_buf[i] = 0xFF;

	ret = ilitek_tddi_fw_iram_read(fw_buf, start, len);
	if (ret < 0)
		goto out;

#if GENERIC_KERNEL_IMAGE
	ILI_ERR("GKI version not allow drivers to use filp_open\n");
	ret = -1;
	debug_en = DEBUG_ALL;
	ili_dump_data(fw_buf, 8, len, 0, "IRAM");
	debug_en = tmp;
#else
	f = filp_open(DUMP_IRAM_PATH, O_WRONLY | O_CREAT | O_TRUNC, 644);
	if (ERR_ALLOC_MEM(f)) {
		ILI_ERR("Failed to open the file at %ld.\n", PTR_ERR(f));
		ret = -1;
		goto out;
	}
	pos = 0;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	vfs_write(f, fw_buf, len, &pos);
	set_fs(old_fs);
#else
	kernel_write(f, fw_buf, len, &pos);
#endif
	filp_close(f, NULL);
	ILI_INFO("Save iram data to %s\n", DUMP_IRAM_PATH);
#endif
out:
	ili_ice_mode_ctrl(DISABLE, mcu);
	ILI_INFO("Dump IRAM %s\n", (ret < 0) ? "FAIL" : "SUCCESS");
	ipio_kfree((void **)&fw_buf);
	return ret;
}

static int ilitek_tddi_flash_poll_busy(int timer)
{
	int ret = UPDATE_PASS, retry = timer;
	u8 cmd = 0x5;
	u32 temp = 0;

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
		ILI_ERR("Pull cs low failed\n");

	if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, cmd, 1) < 0)
		ILI_ERR("Write 0x5 cmd failed\n");

	do {
		if (ili_ice_mode_write(FLASH2_ADDR, 0xFF, 1) < 0)
			ILI_ERR("Write dummy failed\n");

		mdelay(1);

		if (ili_ice_mode_read(FLASH4_ADDR, &temp, sizeof(u8)) < 0)
			ILI_ERR("Read flash busy error\n");

		if ((temp & 0x3) == 0)
			break;
	} while (--retry >= 0);

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
		ILI_ERR("Pull cs high failed\n");

	if (retry <= 0) {
		ILI_ERR("Flash polling busy timeout ! tmp = %x\n", temp);
		ret = UPDATE_FAIL;
	}

	return ret;
}

void ili_flash_clear_dma(void)
{
	if (ili_ice_mode_bit_mask_write(FLASH4_ADDR, FLASH4_reg_flash_dma_trigger_en, (0 << 24)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%d, addr=0x%x failed\n", FLASH4_reg_flash_dma_trigger_en, (0 << 24), FLASH4_ADDR);

	if (ili_ice_mode_bit_mask_write(FLASH0_ADDR, FLASH0_reg_rx_dual, (0 << 24)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%d, addr=0x%x failed\n", FLASH0_reg_rx_dual, (0 << 24), FLASH0_ADDR);

	if (ili_ice_mode_write(FLASH3_reg_rcv_cnt, 0x00, 1) < 0)
		ILI_ERR("Write 0x0 at %x failed\n", FLASH3_reg_rcv_cnt);

	if (ili_ice_mode_write(FLASH4_reg_rcv_data, 0xFF, 1) < 0)
		ILI_ERR("Write 0xFF at %x failed\n", FLASH4_reg_rcv_data);
}

void ili_flash_clear_dma_cascade(void)
{
	if (ili_ice_mode_bit_mask_write_cascade(FLASH4_ADDR, FLASH4_reg_flash_dma_trigger_en, (0 << 24), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%d, addr=0x%x failed\n", FLASH4_reg_flash_dma_trigger_en, (0 << 24), FLASH4_ADDR);

	if (ili_ice_mode_bit_mask_write_cascade(FLASH0_ADDR, FLASH0_reg_rx_dual, (0 << 24), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%d, addr=0x%x failed\n", FLASH0_reg_rx_dual, (0 << 24), FLASH0_ADDR);

	if (ili_ice_mode_write_by_mode(FLASH3_reg_rcv_cnt, 0x00, 1, BOTH) < 0)
		ILI_ERR("Cascade write 0x0 at %x failed\n", FLASH3_reg_rcv_cnt);

	if (ili_ice_mode_write_by_mode(FLASH4_reg_rcv_data, 0xFF, 1, BOTH) < 0)
		ILI_ERR("Cascade write 0xFF at %x failed\n", FLASH4_reg_rcv_data);
}

static int ilitek_tddi_flash_read_int_flag(void)
{
	int retry = 100;
	u32 data = 0;

	do {
		if (ili_ice_mode_read(INTR1_ADDR, &data, sizeof(u32)) < 0)
			ILI_ERR("Read flash int flag error\n");

		ILI_DBG("int flag = %x\n", data);
		if ((data & BIT(25)) == BIT(25))
			break;
		mdelay(2);
	} while (--retry >= 0);

	if (retry <= 0) {
		ILI_ERR("Read Flash INT flag timeout !, flag = 0x%x\n", data);
		return -1;
	}
	return 0;
}

void ili_flash_dma_write(u32 start, u32 end, u32 len)
{
	if (ili_ice_mode_bit_mask_write(FLASH0_ADDR, FLASH0_reg_preclk_sel | BIT(20) | BIT(21) | BIT(22) | BIT(23), (2 << 16)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%d, addr=0x%x failed\n", FLASH0_reg_preclk_sel | BIT(20) | BIT(21) | BIT(22) | BIT(23), (2 << 16), FLASH0_ADDR);

	if (ili_ice_mode_write(FLASH0_reg_flash_csb, 0x00, 1) < 0)
		ILI_ERR("Pull cs low failed\n");

	if (ili_ice_mode_write(FLASH1_reg_flash_key1, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_reg_tx_data, 0x0b, 1) < 0)
		ILI_ERR("Write 0x0b at %x failed\n", FLASH2_reg_tx_data);

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write 0xb timeout \n");
		return;
	}

	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, BIT(25)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_flash_int_flag, BIT(25), INTR1_ADDR);

	if (ili_ice_mode_write(FLASH2_reg_tx_data, (start & 0xFF0000) >> 16, 1) < 0)
		ILI_ERR("Write %x at %x failed\n", (start & 0xFF0000) >> 16, FLASH2_reg_tx_data);

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write addr1 timeout\n");
		return;
	}

	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, BIT(25)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_flash_int_flag, BIT(25), INTR1_ADDR);

	if (ili_ice_mode_write(FLASH2_reg_tx_data, (start & 0x00FF00) >> 8, 1) < 0)
		ILI_ERR("Write %x at %x failed\n", (start & 0x00FF00) >> 8, FLASH2_reg_tx_data);

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write addr2 timeout\n");
		return;
	}

	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, BIT(25)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_flash_int_flag, BIT(25), INTR1_ADDR);

	if (ili_ice_mode_write(FLASH2_reg_tx_data, (start & 0x0000FF), 1) < 0)
		ILI_ERR("Write %x at %x failed\n", (start & 0x0000FF), FLASH2_reg_tx_data);

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write addr3 timeout\n");
		return;
	}

	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, BIT(25)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_flash_int_flag, BIT(25), INTR1_ADDR);

	if (ili_ice_mode_bit_mask_write(FLASH0_ADDR, FLASH0_reg_rx_dual, (0 << 24)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%d, addr=0x%x failed\n", FLASH0_reg_rx_dual, (0 << 24), FLASH0_ADDR);

	if (ili_ice_mode_write(FLASH2_reg_tx_data, 0x00, 1) < 0)
		ILI_ERR("Write dummy failed\n");

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write dummy timeout\n");
		return;
	}

	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, BIT(25)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_flash_int_flag, BIT(25), INTR1_ADDR);

	if (ili_ice_mode_write(FLASH3_reg_rcv_cnt, len, 4) < 0)
		ILI_ERR("Write length failed\n");
}

void ili_flash_dma_write_cascade(u32 start,  u32 end, u32 len)
{
	if (ili_ice_mode_bit_mask_write_cascade(FLASH0_ADDR, FLASH0_reg_preclk_sel, (2 << 16), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%d, addr=0x%x failed\n", FLASH0_reg_preclk_sel, (2 << 16), FLASH0_ADDR);

	if (ili_ice_mode_write_by_mode(FLASH0_reg_flash_csb, 0x00, 1, BOTH) < 0)
		ILI_ERR("Cascade pull cs low failed\n");

	if (ili_ice_mode_write_by_mode(FLASH1_reg_flash_key1, 0x66aa55, 3, BOTH) < 0)
		ILI_ERR("Cascade write key failed\n");

	if (ili_ice_mode_write(FLASH2_reg_tx_data, 0x3b, 1) < 0)
		ILI_ERR("Write 0x3b at %x failed\n", FLASH2_reg_tx_data);

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write 0x3b timeout \n");
		return;
	}

	if (ili_ice_mode_bit_mask_write_cascade(INTR1_ADDR, INTR1_reg_flash_int_flag, BIT(25), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_flash_int_flag, BIT(25), INTR1_ADDR);

	if (ili_ice_mode_write(FLASH2_reg_tx_data, (start & 0xFF0000) >> 16, 1) < 0)
		ILI_ERR("Write %x at %x failed\n", (start & 0xFF0000) >> 16, FLASH2_reg_tx_data);

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write addr1 timeout\n");
		return;
	}

	if (ili_ice_mode_bit_mask_write_cascade(INTR1_ADDR, INTR1_reg_flash_int_flag, BIT(25), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_flash_int_flag, BIT(25), INTR1_ADDR);

	if (ili_ice_mode_write(FLASH2_reg_tx_data, (start & 0x00FF00) >> 8, 1) < 0)
		ILI_ERR("Write %x at %x failed\n", (start & 0x00FF00) >> 8, FLASH2_reg_tx_data);

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write addr2 timeout\n");
		return;
	}

	if (ili_ice_mode_bit_mask_write_cascade(INTR1_ADDR, INTR1_reg_flash_int_flag, BIT(25), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_flash_int_flag, BIT(25), INTR1_ADDR);

	if (ili_ice_mode_write(FLASH2_reg_tx_data, (start & 0x0000FF), 1) < 0)
		ILI_ERR("Write %x at %x failed\n", (start & 0x0000FF), FLASH2_reg_tx_data);

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write addr3 timeout\n");
		return;
	}

	if (ili_ice_mode_bit_mask_write_cascade(INTR1_ADDR, INTR1_reg_flash_int_flag, BIT(25), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_flash_int_flag, BIT(25), INTR1_ADDR);

	if (ili_ice_mode_bit_mask_write_cascade(FLASH0_ADDR, FLASH0_reg_rx_dual, BIT(24), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", FLASH0_reg_rx_dual, BIT(24), FLASH0_ADDR);

	/* Set slave to qual mode */
	if (ili_ice_mode_write_by_mode(FLASH0_dual_mode, 0x80, 1, SLAVE) < 0)
		ILI_ERR("Slave write 0x80 at %x failed\n", FLASH0_dual_mode);

	if (ili_ice_mode_write(FLASH2_reg_tx_data, 0x00, 1) < 0)
		ILI_ERR("Write 0x00 at %x failed\n", FLASH2_reg_tx_data);

	if (ilitek_tddi_flash_read_int_flag() < 0) {
		ILI_ERR("Write dummy timeout\n");
		return;
	}

	if (ili_ice_mode_bit_mask_write_cascade(INTR1_ADDR, INTR1_reg_flash_int_flag, BIT(25), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_flash_int_flag, BIT(25), INTR1_ADDR);

	if (ili_ice_mode_write_by_mode(FLASH3_reg_rcv_cnt, len, 4, BOTH) < 0)
		ILI_ERR("Cascade write length failed\n");

	/* Set cascade load flash by fw trigger */
	if (ili_ice_mode_bit_mask_write_cascade(FLASH_CASCADE_ADDR, FLASH_reg_cas_fw_trigger_en, BIT(7), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", FLASH_reg_cas_fw_trigger_en, BIT(7), FLASH_CASCADE_ADDR);
}

static void ilitek_tddi_flash_write_enable(void)
{
	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
		ILI_ERR("Pull CS low failed\n");

	if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, 0x6, 1) < 0)
		ILI_ERR("Write Write_En failed\n");

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
		ILI_ERR("Pull CS high failed\n");
}

int ili_fw_read_hw_crc(u32 start, u32 end, u32 *flash_crc)
{
	int retry = 100;
	u32 busy = 0;
	u32 write_len = end;

	if (write_len > ilits->chip->max_count) {
		ILI_ERR("The length (%x) written into firmware is greater than max count (%x)\n",
			write_len, ilits->chip->max_count);
		return -1;
	}

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
		ILI_ERR("Pull CS low failed\n");

	if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, 0x3b, 1) < 0)
		ILI_ERR("Write 0x3b failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, (start & 0xFF0000) >> 16, 1) < 0)
		ILI_ERR("Write address failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, (start & 0x00FF00) >> 8, 1) < 0)
		ILI_ERR("Write address failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, (start & 0x0000FF), 1) < 0)
		ILI_ERR("Write address failed\n");

	if (ili_ice_mode_write(0x041003, 0x01, 1) < 0)
		ILI_ERR("Write enable Dio_Rx_dual failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, 0xFF, 1) < 0)
		ILI_ERR("Write dummy failed\n");

	if (ili_ice_mode_write(0x04100C, write_len, 3) < 0)
		ILI_ERR("Write Set Receive count failed\n");

	if (ili_ice_mode_write(0x048007, 0x02, 1) < 0)
		ILI_ERR("Write clearing int flag failed\n");

	if (ili_ice_mode_write(0x041016, 0x00, 1) < 0)
		ILI_ERR("Write 0x0 at 0x041016 failed\n");

	if (ili_ice_mode_write(0x041016, 0x01, 1) < 0)
		ILI_ERR("Write Checksum_En failed\n");

	if (ili_ice_mode_write(FLASH4_ADDR, 0xFF, 1) < 0)
		ILI_ERR("Write start to receive failed\n");

	do {
		if (ili_ice_mode_read(0x048007, &busy, sizeof(u8)) < 0)
			ILI_ERR("Read busy error\n");

		ILI_DBG("busy = %x\n", busy);
		if (((busy >> 1) & 0x01) == 0x01)
			break;
		mdelay(2);
	} while (--retry >= 0);

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
		ILI_ERR("Write CS high failed\n");

	if (retry <= 0) {
		ILI_ERR("Read HW CRC timeout !, busy = 0x%x\n", busy);
		return -1;
	}

	if (ili_ice_mode_write(0x041003, 0x0, 1) < 0)
		ILI_ERR("Write disable dio_Rx_dual failed\n");

	if (ili_ice_mode_read(0x04101C, flash_crc, sizeof(u32)) < 0) {
		ILI_ERR("Read hw crc error\n");
		return -1;
	}

	return 0;
}

static int ilitek_tddi_fw_read_flash_data(u32 start, u32 end, u8 *data, int len)
{
	u32 i, j, index = 0;
	u32 tmp;

	if (end - start > len) {
		ILI_ERR("the length (%d) reading crc is over than len(%d)\n", end - start, len);
		return -1;
	}

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
		ILI_ERR("Write cs low failed\n");

	if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, 0x03, 1) < 0)
		ILI_ERR("Write 0x3 failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, (start & 0xFF0000) >> 16, 1) < 0)
		ILI_ERR("Write address failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, (start & 0x00FF00) >> 8, 1) < 0)
		ILI_ERR("Write address failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, (start & 0x0000FF), 1) < 0)
		ILI_ERR("Write address failed\n");

	for (i = start, j = 0; i <= end; i++, j++) {
		if (ili_ice_mode_write(FLASH2_ADDR, 0xFF, 1) < 0)
			ILI_ERR("Write dummy failed\n");

		if (ili_ice_mode_read(FLASH4_ADDR, &tmp, sizeof(u8)) < 0)
			ILI_ERR("Read flash data error!\n");

		data[index] = tmp;
		index++;
		ilits->fw_update_stat = (j * 100) / len;
		ILI_DBG("Reading flash data .... %d%c", ilits->fw_update_stat, '%');
	}

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
		ILI_ERR("Write cs high failed\n");

	return 0;
}

int ili_fw_dump_flash_data(u32 start, u32 end, bool user, bool mcu)
{
	u32 start_addr, end_addr;
	int ret, length;
	u8 *buf = NULL;
#if GENERIC_KERNEL_IMAGE
	int tmp = debug_en;
#else
	struct file *f = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)
	mm_segment_t old_fs;
#endif
	loff_t pos = 0;
	f = filp_open(DUMP_FLASH_PATH, O_WRONLY | O_CREAT | O_TRUNC, 644);
	if (ERR_ALLOC_MEM(f)) {
		ILI_ERR("Failed to open the file at %ld.\n", PTR_ERR(f));
		ret = -1;
		goto out;
	}
#endif
	ret = ili_ice_mode_ctrl(ENABLE, mcu);
	if (ret < 0)
		goto out;

	if (user) {
		start_addr = 0x0;
		end_addr = 0x1FFFF;
	} else {
		start_addr = start;
		end_addr = end;
	}

	length = end_addr - start_addr + 1;
	ILI_INFO("len = %d\n", length);

	buf = vmalloc(length * sizeof(u8));
	if (ERR_ALLOC_MEM(buf)) {
		ILI_ERR("Failed to allocate buf memory, %ld\n", PTR_ERR(buf));
		ret = -1;
		goto out;
	}

	ret = ilitek_tddi_fw_read_flash_data(start_addr, end_addr, buf, length);
	if (ret < 0)
		goto out;

#if GENERIC_KERNEL_IMAGE
	ILI_ERR("GKI version not allow drivers to use filp_open, dump flash data\n");
	debug_en = DEBUG_ALL;
	ili_dump_data(buf, 8, length, 0, "flash");
	debug_en = tmp;
#else
	pos = 0;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	vfs_write(f, buf, length, &pos);
	set_fs(old_fs);
#else
	kernel_write(f, buf, length, &pos);
#endif
#endif
out:
#if (!GENERIC_KERNEL_IMAGE)
	filp_close(f, NULL);
#endif
	ipio_vfree((void **)&buf);
	ili_ice_mode_ctrl(DISABLE, mcu);
	ILI_INFO("Dump flash %s\n", (ret < 0) ? "FAIL" : "SUCCESS");
	return ret;
}

void ilitek_tddi_flash_read(u32 *data, u8 cmd, int len)
{
	u32 rxbuf[4] = {0};
	int i = 0;

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)/* CS Low */
		ILI_ERR("Write cs low failed\n");
	if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, cmd, 1) < 0)
		ILI_ERR("Write read command failed\n");
	for (i = 0; i < len; i++) {
		if (ili_ice_mode_write(FLASH2_ADDR, 0xFF, 1) < 0)
			ILI_ERR("Write read command failed\n");
		ili_ice_mode_read(FLASH4_ADDR, data, 1);
		rxbuf[i] = *data;
	}

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
		ILI_ERR("Write cs high failed\n");

	*data = 0;
	if (len == 1)
		*data = rxbuf[0];
	else if (len == 2)
		*data = (rxbuf[0] | rxbuf[1] << 8);
	else if (len == 3)
		*data = (rxbuf[0] | rxbuf[1] << 8 | rxbuf[2] << 16);
	else
		*data = (rxbuf[0] | rxbuf[1] << 8 | rxbuf[2] << 16 | rxbuf[3] << 24);
	ILI_DBG("flash read data = 0X%X\n", *data);
}

void ilitek_tddi_flash_write(u8 cmd, u8 *sendbuf,int len)
{
	int i = 0;
	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)/* CS Low */
		ILI_ERR("Write cs low failed\n");

	if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, cmd, 1) < 0)
		ILI_ERR("Write read command failed\n");

	for (i = 0; i < len; i++) {
		if (ili_ice_mode_write(FLASH2_ADDR, sendbuf[i], 1) < 0)
			ILI_ERR("Write sendbuf failed\n");
	}

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0) /* CS high */
		ILI_ERR("Write cs high failed\n");
}

int ilitek_tddi_flash_protect(bool enable, bool mcu)
{
	int ret = 0, i = 0, w_count = 0, offsetIndex = 0, fun_ret = UPDATE_PASS;
	u32 flash_uid = 0, data = 0, ckreaddata = 0, ck_mask = 0;
	u8 bitPosition = 0, setBitValue = 0, ckSRP1 = 0, rdSRP1 = 0;
	u8 w_buf[4] = { 0 };
	bool ice = atomic_read(&ilits->ice_stat);

	ILI_INFO("%s flash protection\n", enable ? "Enable" : "Disable");

	if (!ice) {
		if (ilits->cascade_info_block.nNum != 0)
			ret = ili_ice_mode_ctrl_by_mode(ENABLE, OFF, BOTH);
		else
			ret = ili_ice_mode_ctrl(ENABLE, mcu);

		if (ret < 0) {
			ILI_ERR("Enable ice mode failed\n");
			fun_ret = UPDATE_FAIL;
			goto out;
		}
	}

	flash_uid = (ilits->flash_mid << 16) + ilits->flash_devid;

	if (flash_uid == 0x0) {
		ILI_ERR("flash_uid error, get flash info again!\n");
		ili_fw_read_flash_info(OFF);
		flash_uid = (ilits->flash_mid << 16) + ilits->flash_devid;
	}

	ILI_INFO("flash(0x%X) is %s\n", flash_uid, ilits->isSupportFlash ? "supported" : "not supported");

	if (ilits->isSupportFlash) {

		ILI_INFO("flash index is %d\n", ilits->supportFlashIndex);

		/* read flash data */
		offsetIndex = 0;
		for (i = 0; i < flash_protect_list[ilits->supportFlashIndex].readOPCount; i++) {
			u32 tmp = 0;
			u8 cmd = flash_protect_list[ilits->supportFlashIndex].readOperator[i * 2];
			int readlen = flash_protect_list[ilits->supportFlashIndex].readOperator[(i * 2) + 1];
			ilitek_tddi_flash_read(&tmp, cmd, readlen);
			data |= tmp << (offsetIndex * 8);
			offsetIndex += readlen;
		}

		ILI_DBG("data = 0x%X\n", data);
		if (enable) {
			/* set flash all protect bit */
			for (i = 0; i < flash_protect_list[ilits->supportFlashIndex].protectAllOPCount; i++) {
				bitPosition = flash_protect_list[ilits->supportFlashIndex].protectAllOperator[i * 2];
				setBitValue = flash_protect_list[ilits->supportFlashIndex].protectAllOperator[(i * 2) + 1];

				if (setBitValue == 1)
					data |= (0x1 << bitPosition);
				else
					data &= ~(0x1 << bitPosition);
				ck_mask |= (0x1 << bitPosition);
			}
		} else {
			/* set flash non-protect bit */
			for (i = 0; i < flash_protect_list[ilits->supportFlashIndex].resetOPCount; i++) {
				bitPosition = flash_protect_list[ilits->supportFlashIndex].resetOperator[i * 2];
				setBitValue = flash_protect_list[ilits->supportFlashIndex].resetOperator[(i * 2) + 1];

				if (setBitValue == 1)
					data |= (0x1 << bitPosition);
				else
					data &= ~(0x1 << bitPosition);
				ck_mask |= (0x1 << bitPosition);
			}
		}

		for (i = 0; i < 4; i++)
			w_buf[i] = (data >> (8 * i)) & 0xFF;

		ILI_DBG("write data = 0x%X\n", data);

		/* check flash is locked */
		if (flash_protect_list[ilits->supportFlashIndex].preventOTPCount > 0) {
			bitPosition = flash_protect_list[ilits->supportFlashIndex].preventOTPOperator[0];
			rdSRP1 = (data >> bitPosition) & 0x01 ;
			ckSRP1 = flash_protect_list[ilits->supportFlashIndex].preventOTPOperator[1];
			if (rdSRP1 != ckSRP1) {
				ILI_ERR("SRP1(%d) = %d error!\n", ckSRP1, rdSRP1);
				fun_ret = UPDATE_FAIL;
				goto out;
			}
		}

		for (i = 0; i < flash_protect_list[ilits->supportFlashIndex].writeOPCount; i++) {
			u8 cmd = flash_protect_list[ilits->supportFlashIndex].writeOperator[i * 2];
			int writelen = flash_protect_list[ilits->supportFlashIndex].writeOperator[(i * 2) + 1];

			ilitek_tddi_flash_write_enable();

			ilitek_tddi_flash_write(cmd, &w_buf[w_count], writelen);

			if (ilitek_tddi_flash_poll_busy(TIMEOUT_PROTECT) < 0)
				ILI_ERR("polling busy fail\n");
			w_count += writelen;
		}

		/* make sure the flash is protected  */
		ckreaddata = 0;

		offsetIndex = 0;
		for (i = 0; i < flash_protect_list[ilits->supportFlashIndex].readOPCount; i++) {
			u32 tmp = 0;
			u8 cmd = flash_protect_list[ilits->supportFlashIndex].readOperator[i * 2];
			int readlen = flash_protect_list[ilits->supportFlashIndex].readOperator[(i * 2) + 1];
			ilitek_tddi_flash_read(&tmp, cmd, readlen);
			ckreaddata |= tmp << (offsetIndex * 8);
			offsetIndex += readlen;
		}

		ILI_DBG("data = 0x%X, ckreaddata = 0x%X, ck_mask = 0x%X\n", data, ckreaddata, ck_mask);

		if ((ckreaddata & ck_mask) != (data & ck_mask)) {
			ILI_ERR("protect flash fail, data = 0x%X, ckreaddata = 0x%X, ck_mask = 0x%X\n", data, ckreaddata, ck_mask);
			fun_ret = UPDATE_FAIL;
		}

	} else {
		/*It only tries to unprotect flash in the unsupported list.*/
		if (enable) {
			ILI_ERR("no need to protect flash\n");
			goto out;
		}

		ilitek_tddi_flash_write_enable();

		if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)/* CS Low */
			ILI_ERR("Write cs low failed\n");
		if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
			ILI_ERR("Write key failed\n");
		if (ili_ice_mode_write(FLASH2_ADDR, 0x01, 1) < 0)
			ILI_ERR("Write Start_Write failed\n");

		if (ili_ice_mode_write(FLASH2_ADDR, 0x00, 1) < 0)
				ILI_ERR("Write Un-Protect Low byte failed\n");
		if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
			ILI_ERR("Write cs high failed\n");
		mdelay(30);
	}
out:
	if (!ice) {
		if (ilits->cascade_info_block.nNum != 0)
			ret = ili_ice_mode_ctrl_by_mode(DISABLE, OFF, BOTH);
		else
			ret = ili_ice_mode_ctrl(DISABLE, mcu);

		if (ret < 0) {
			ILI_ERR("Enable ice mode failed\n");
			fun_ret = UPDATE_FAIL;
		}
	}

	return fun_ret;

}

int ilitek_fw_judge_hex(u8 *pfw, u32 *customerHexAddrStart, u32 *customerHexAddrEnd, u32 *mpHexAddrStart, u32 *mpHexAddrEnd)
{
	int ret = UPDATE_PASS, i = 0;
	u32 TagAddress = 0;
	u32 CheckVaildAddress = 0;
	int tagHexAddrStart = 0, tagHexAddrEnd = 0;
	const int addressLength = 4;
	u8 tagInfo[24] = {0x0};


	CheckVaildAddress = fbi[AP].end + 1 - 20;

	if (pfw[CheckVaildAddress] == 0x5A && pfw[CheckVaildAddress + 1] == 0xA5) {
		TagAddress = fbi[AP].end + 1 - pfw[CheckVaildAddress + 2];
	} else {
		ILI_INFO("Judge new hex CheckVaildValue not found: 0x%X, 0x%X\n", pfw[CheckVaildAddress], pfw[CheckVaildAddress + 1]);
		ret = UPDATE_FAIL;
		goto out;
	}

	memcpy(tagInfo, &pfw[TagAddress], 24);

	for (i = 0; i < 6; i++) {
		switch (i) {
		case 0:
			*mpHexAddrStart = (tagInfo[i * addressLength + 2] << 16) + (tagInfo[i * addressLength + 1] << 8) + tagInfo[i * addressLength + 0];
			break;
		case 1:
			*mpHexAddrEnd = (tagInfo[i * addressLength + 2] << 16) + (tagInfo[i * addressLength + 1] << 8) + tagInfo[i * addressLength + 0];
			break;
		case 2:
			*customerHexAddrStart = (tagInfo[i * addressLength + 2] << 16) + (tagInfo[i * addressLength + 1] << 8) + tagInfo[i * addressLength + 0];
			break;
		case 3:
			*customerHexAddrEnd = (tagInfo[i * addressLength + 2] << 16) + (tagInfo[i * addressLength + 1] << 8) + tagInfo[i * addressLength + 0];
			break;
		case 4:
			tagHexAddrStart = (tagInfo[i * addressLength + 2] << 16) + (tagInfo[i * addressLength + 1] << 8) + tagInfo[i * addressLength + 0];
			break;
		case 5:
			tagHexAddrEnd = (tagInfo[i * addressLength + 2] << 16) + (tagInfo[i * addressLength + 1] << 8) + tagInfo[i * addressLength + 0];
			break;
		}
	}

	if(fbi[TAG].start != tagHexAddrStart || fbi[TAG].end != tagHexAddrEnd) {
		ILI_INFO("fbi[TAG]: start addr = 0x%X, end addr = 0x%X\n ", fbi[TAG].start, fbi[TAG].end);
		ILI_INFO("Hex Tag: start addr = 0x%X, end addr = 0x%X\n ", tagHexAddrStart, tagHexAddrEnd);
		ret = UPDATE_FAIL;
		goto out;
	}
out:
	return ret;
}

static int ilitek_fw_check_ddi_chunk(u8 *pfw)
{
	bool backup_customer = false, backup_mp = false;
	int ret = 0, i = 0, j = 0, retry = 0;
	u8 cmd[7] = {0x0};
	u8 block_info[20] = {0x0};
	u32 customerFlashAddrStart = 0x0, customerFlashAddrEnd = 0x0, customerFlashSize = 0x0;
	u32 mpFlashAddrStart = 0x0, mpFlashAddrEnd = 0x0, mpFlashSize = 0x0;
	u32 customerHexAddrStart = 0x0, customerHexAddrEnd = 0x0, customerHexSize = 0x0;
	u32 mpHexAddrStart = 0x0, mpHexAddrEnd = 0x0, mpHexSize = 0x0;
	u32 Start_tmp = 0, End_tmp = 0, len = 0;

	bool hex_info = ilits->info_from_hex;
	if (ilits->chip->core_ver >= CORE_VER_2100) {
		do {
			cmd[0] = P5_X_GET_BLOCK_INFOMATION;
			ret = ilits->wrapper(cmd, sizeof(u8), block_info, sizeof(block_info), ON, OFF);
			if (ret < 0) {
				ILI_ERR("Failed to get block info\n");
				continue;
			}
			if (block_info[0] != cmd[0]) {
				ILI_INFO("Invalid block info\n");
			} else {
				ILI_INFO("Get block info successfully, retry %d times\n", retry);
				break;
			}
			mdelay(5);
		} while (++retry < 3);
	}

	ret = ilitek_fw_judge_hex(pfw, &customerHexAddrStart, &customerHexAddrEnd, &mpHexAddrStart, &mpHexAddrEnd);

	if (block_info[0] == P5_X_GET_BLOCK_INFOMATION && ret == UPDATE_PASS) {
		ILI_INFO("Block info is supported\n");

		customerFlashAddrStart = (block_info[3] << 16) + (block_info[2] << 8) + block_info[1];
		customerFlashAddrEnd = (block_info[6] << 16) + (block_info[5] << 8) + block_info[4];
		mpFlashAddrStart = (block_info[9] << 16) + (block_info[8] << 8) + block_info[7];
		mpFlashAddrEnd = (block_info[12] << 16) + (block_info[11] << 8) + block_info[10];
		customerFlashSize = customerFlashAddrEnd - customerFlashAddrStart + 1;
		mpFlashSize = mpFlashAddrEnd - mpFlashAddrStart + 1;
		ILI_INFO("Flash customer reserved block: start addr = 0x%X, end addr = 0x%X\n ", customerFlashAddrStart, customerFlashAddrEnd);
		ILI_INFO("Flash mp block: start addr = 0x%X, end addr = 0x%X\n ", mpFlashAddrStart, mpFlashAddrEnd);

		customerHexSize = customerHexAddrEnd - customerHexAddrStart + 1;
		mpHexSize = mpHexAddrEnd - mpHexAddrStart + 1;
		ILI_INFO("Hex customer reserved block: start addr = 0x%X, end addr = 0x%X\n ", customerHexAddrStart, customerHexAddrEnd);
		ILI_INFO("Hex mp block: start addr = 0x%X, end addr = 0x%X\n ", mpHexAddrStart, mpHexAddrEnd);

		/* Compare FlashAddr & HexAddr */
		if (customerFlashAddrStart != customerHexAddrStart || mpFlashAddrStart != mpHexAddrStart) {
			ILI_INFO("mp & customer addr have been changed\n");
			backup_customer = true;
			backup_mp = true;
		}
		for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
			if (fbi[i].end == 0)
				continue;

			Start_tmp = fbi[i].start - (fbi[i].start & 0xFFF);
			End_tmp = fbi[i].end | 0xFFF;

			if ((customerFlashAddrStart >= Start_tmp && customerFlashAddrStart <= End_tmp) ||
				(customerFlashAddrEnd >= Start_tmp && customerFlashAddrEnd <= End_tmp) ||
				(mpFlashAddrStart >= Start_tmp && mpFlashAddrStart <= End_tmp) ||
				(mpFlashAddrEnd >= Start_tmp && mpFlashAddrEnd <= End_tmp)) {
				ILI_INFO("mp & customer block in flash overlapped with fbi[%d] block. start:%x, end %x\n",
				i, fbi[i].start, fbi[i].end);
				backup_customer = true;
				backup_mp = true;
			}
		}


	} else if (block_info[0] != P5_X_GET_BLOCK_INFOMATION && ret != UPDATE_PASS) {
		ILI_INFO("Block info is not supported\n");

		if (!fbi[DDI].start || !fbi[DDI].end)
			return UPDATE_PASS;

		Start_tmp = fbi[DDI].start - (fbi[DDI].start & 0xFFF);
		End_tmp = fbi[DDI].end | 0xFFF;

		if (Start_tmp != fbi[DDI].start) {
			customerFlashAddrStart = Start_tmp;
			customerFlashAddrEnd = fbi[DDI].start - 1;
			customerFlashSize = fbi[DDI].start - Start_tmp;
			customerHexAddrStart = customerFlashAddrStart;
			customerHexAddrEnd = customerFlashAddrEnd;
			customerHexSize = customerFlashSize;
			backup_customer = true;

			for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
				if (fbi[i].end == 0)
					continue;

				if (fbi[i].end >= customerHexAddrStart && fbi[i].end <= customerHexAddrEnd) {
					ILI_INFO("customer block overlapped with fbi[%d] block. start:%x, end %x\n",
					i, fbi[i].start, fbi[i].end);
					backup_customer = false;
					break;
				}
			}
		}

		if (End_tmp != fbi[DDI].end) {
			mpFlashAddrStart = fbi[DDI].end + 1;
			mpFlashAddrEnd = End_tmp;
			mpFlashSize = End_tmp - fbi[DDI].end;
			mpHexAddrStart = mpFlashAddrStart;
			mpHexAddrEnd = mpFlashAddrEnd;
			mpHexSize = mpFlashSize;
			backup_mp = true;

			for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
				if (fbi[i].end == 0)
					continue;

				if (fbi[i].start >= mpHexAddrStart && fbi[i].start <= mpHexAddrEnd) {
					ILI_INFO("mp block overlapped with fbi[%d] block. start:%x, end %x\n",
					i, fbi[i].start, fbi[i].end);
					backup_mp = false;
					break;
				}
			}
		}
	} else {
		if (ret == UPDATE_PASS)
			ILI_ERR("Block info is supported by hex, but the current FW is not.\n");
		else
			ILI_ERR("Block info is supported in current FW, but hex is not.\n");

		if (ilits->force_fw_update)
			return UPDATE_PASS;
		else {
#if BOOT_FW_UPDATE
			len = 32;
			memset(fw_flash_tmp, 0xFF, len);
			/* Command for getting ddi info from firmware. */
			ili_ice_mode_ctrl(ENABLE, OFF);
			ilitek_tddi_fw_read_flash_data(fbi[AP].start, fbi[AP].start + len - 1, fw_flash_tmp, len);
			ili_ice_mode_ctrl(DISABLE, OFF);
			for (i = 0;i < len; i++) {
				if (fw_flash_tmp[i] != 0xFF) {
					ILI_ERR("Flash not empty, stop update\n");
					ILI_ERR("Try to use driver node to force an update\n");
					return UPDATE_FAIL;
				}
			}
			return UPDATE_PASS;
#else
			ILI_ERR("Try to use driver node to force an update\n");
			return UPDATE_FAIL;
#endif
		}
	}

	ret = UPDATE_PASS;
	if (!backup_customer && !backup_mp)
		goto out;

	/* To make sure if flash has data. */
	if (hex_info)
		ilits->info_from_hex = DISABLE;

	if (ili_ic_get_fw_ver() < 0) {
		ILI_INFO("Failed to get fw ver, reading flash by dma instead\n");
		goto dma;
	}

	if (backup_customer) {
		for(i = customerFlashAddrStart, j = customerHexAddrStart; i < customerFlashAddrStart + customerFlashSize; i += DDI_RSV_BK_SIZE, j += DDI_RSV_BK_SIZE) {
			len = (customerFlashAddrEnd - i + 1) < DDI_RSV_BK_SIZE ? (customerFlashAddrEnd - i + 1) : DDI_RSV_BK_SIZE;
			memset(fw_flash_tmp, 0xFF, sizeof(fw_flash_tmp));
			/* Command for getting ddi info from firmware. */
			cmd[0] = CMD_GET_FLASH_DATA;		/* cmd */
			cmd[1] = len & 0xFF;	/* len L byte */
			cmd[2] = (len >> 8) & 0xFF;	/* len H byte */

			/* Read customer reserved block */
			cmd[3] = (i & 0xFF);       	/* addr L byte */
			cmd[4] = (i >> 8) & 0xFF ;  /* addr M byte */
			cmd[5] = (i >> 16) & 0xFF; 	/* addr H byte */
			cmd[6] = ili_calc_packet_checksum(cmd, sizeof(cmd) - 1);
			ILI_INFO("Read customer reserved block: start addr = 0x%X, len = 0x%X\n", i, len);
			if (ilits->wrapper(cmd, sizeof(cmd), fw_flash_tmp, len + 7, OFF, ON) < 0) {
				ILI_ERR("Failed to read customer reserved block data from fw\n");
				goto dma;
			}
			ili_dump_data(fw_flash_tmp + 6, 8, 32, 16, "CUSTOMER");

			if ((customerHexAddrEnd - j + 1) < len) {
				ipio_memcpy(&pfw[j], fw_flash_tmp + 6, (customerHexAddrEnd - j + 1), MAX_HEX_FILE_SIZE - j);
				break;
			} else {
				ipio_memcpy(&pfw[j], fw_flash_tmp + 6, len, MAX_HEX_FILE_SIZE - j);
			}
		}
		fbi_b1[CUSTOMER].start = customerHexAddrStart;
		fbi_b1[CUSTOMER].end = customerHexAddrEnd;
		fbi_b1[CUSTOMER].len = customerHexSize;
	}

	if (backup_mp) {
		for(i = mpFlashAddrStart, j = mpHexAddrStart; i < mpFlashAddrStart + mpFlashSize; i += DDI_RSV_BK_SIZE, j += DDI_RSV_BK_SIZE) {
			len = (mpFlashAddrEnd - i + 1) < DDI_RSV_BK_SIZE ? (mpFlashAddrEnd - i + 1) : DDI_RSV_BK_SIZE;
			memset(fw_flash_tmp, 0xFF, sizeof(fw_flash_tmp));
			/* Command for getting ddi info from firmware. */
			cmd[0] = CMD_GET_FLASH_DATA;		/* cmd */
			cmd[1] = len & 0xFF;	/* len L byte */
			cmd[2] = (len >> 8) & 0xFF;	/* len H byte */
			/* Read mp data block */
			cmd[3] = (i & 0xFF);       	/* addr L byte */
			cmd[4] = (i >> 8) & 0xFF ;  /* addr M byte */
			cmd[5] = (i >> 16) & 0xFF; 	/* addr H byte */
			cmd[6] = ili_calc_packet_checksum(cmd, sizeof(cmd) - 1);
			ILI_INFO("Read mp data block: start addr = 0x%X, len = 0x%X\n", i, len);
			if (ilits->wrapper(cmd, sizeof(cmd), fw_flash_tmp, len + 7, OFF, ON) < 0) {
				ILI_ERR("Failed to read mp data block from fw\n");
				goto dma;
			}
			ili_dump_data(fw_flash_tmp + 6, 8, 32, 16, "MPDATA");

			if ((mpHexAddrEnd - j + 1) < len) {
				ipio_memcpy(&pfw[j], fw_flash_tmp + 6, (mpHexAddrEnd - j + 1), MAX_HEX_FILE_SIZE - j);
				break;
			} else {
				ipio_memcpy(&pfw[j], fw_flash_tmp + 6, len, MAX_HEX_FILE_SIZE - j);
			}
		}
		fbi_b1[MPDATA].start = mpHexAddrStart;
		fbi_b1[MPDATA].end = mpHexAddrEnd;
		fbi_b1[MPDATA].len = mpHexSize;
	}
	goto out;

dma:
	ILI_INFO("Obtain block data from dma\n");

	ili_ice_mode_ctrl(ENABLE, OFF);
	if (backup_customer) {
		/* Read customer reserved block */
		memset(fw_flash_tmp, 0xFF, sizeof(fw_flash_tmp));
		if (customerHexSize <= customerFlashSize) {
			End_tmp = customerFlashAddrStart + customerHexSize - 1;
		} else {
			End_tmp = customerFlashAddrStart + customerFlashSize - 1;
		}
		Start_tmp = customerFlashAddrStart;
		len = End_tmp - Start_tmp + 1;
		if (ilitek_tddi_fw_read_flash_data(Start_tmp, End_tmp, fw_flash_tmp, len) < 0) {
			ILI_ERR("Failed to read customer reserved block data from dma\n");
			ili_ice_mode_ctrl(DISABLE, OFF);
			ret = UPDATE_FAIL;
			goto out;
		}
		ili_dump_data(fw_flash_tmp, 8, 32, 16, "CUSTOMER");
		ipio_memcpy(&pfw[customerHexAddrStart], fw_flash_tmp, len, MAX_HEX_FILE_SIZE - customerHexAddrStart);
		ILI_INFO("Read customer reserved block: start addr = 0x%X, len = 0x%X ", Start_tmp, len);
		fbi_b1[CUSTOMER].start = customerHexAddrStart;
		fbi_b1[CUSTOMER].end = customerHexAddrStart + len - 1;
		fbi_b1[CUSTOMER].len = len;
	}

	if (backup_mp) {
		/* Read mp data block */
		memset(fw_flash_tmp, 0xFF, sizeof(fw_flash_tmp));
		if (mpHexSize <= mpFlashSize) {
			End_tmp = mpFlashAddrStart + mpHexSize - 1;
		} else {
			End_tmp = mpFlashAddrStart + mpFlashSize - 1;
		}
		Start_tmp = mpFlashAddrStart;
		len = End_tmp - Start_tmp + 1;;
		if (ilitek_tddi_fw_read_flash_data(Start_tmp, End_tmp, fw_flash_tmp, len) < 0) {
			ILI_ERR("Failed to read mp data block from dma\n");
			ili_ice_mode_ctrl(DISABLE, OFF);
			ret = UPDATE_FAIL;
			goto out;
		}
		ili_dump_data(fw_flash_tmp, 8, 32, 16, "MPDATA");
		ipio_memcpy(&pfw[mpHexAddrStart], fw_flash_tmp, len, MAX_HEX_FILE_SIZE - mpHexAddrStart);
		ILI_INFO("Read mp data block: start addr = 0x%X, len = 0x%X\n", Start_tmp, len);
		fbi_b1[MPDATA].start = mpHexAddrStart;
		fbi_b1[MPDATA].end = mpHexAddrStart + len - 1;
		fbi_b1[MPDATA].len = len;
	}
	ili_ice_mode_ctrl(DISABLE, OFF);
out:
	if (backup_customer || backup_mp) {
		tfd.end_addr = (mpHexAddrEnd > tfd.end_addr) ? mpHexAddrEnd : tfd.end_addr;
		tfd.end_addr = (customerHexAddrEnd > tfd.end_addr) ? customerHexAddrEnd : tfd.end_addr;
	}

	if (hex_info)
		ilits->info_from_hex = ENABLE;

	atomic_set(&ilits->cmd_int_check, DISABLE);
	return ret;
}

static int ilitek_tddi_flash_fw_crc_check(u8 *pfw, bool check_fw_ver)
{
	int i, len = 0, crc_byte_len = 4;
	u8 flash_crc[4] = {0};
	u32 start_addr = 0, end_addr = 0;
	u32 hw_crc = 0;
	u32 flash_crc_cb = 0, hex_crc = 0;
	u8 check_hex_hw_crc = ENABLE;

	/* Check Flash and HW CRC */
	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		start_addr = fbi[i].start;
		end_addr = fbi[i].end;

		/* Invaild end address */
		if (end_addr == 0)
			continue;

		if (ilitek_tddi_fw_read_flash_data(end_addr - crc_byte_len + 1, end_addr,
					flash_crc, sizeof(flash_crc)) < 0) {
			ILI_ERR("Read Flash failed\n");
			return UPDATE_FAIL;
		}

		flash_crc_cb = flash_crc[0] << 24 | flash_crc[1] << 16 | flash_crc[2] << 8 | flash_crc[3];

		if (ili_fw_read_hw_crc(start_addr, end_addr - start_addr - crc_byte_len + 1, &hw_crc) < 0) {
			ILI_ERR("Read HW CRC failed\n");
			return UPDATE_FAIL;
		}

		ILI_INFO("Block = %d, HW CRC = 0x%06x, Flash CRC = 0x%06x\n", i, hw_crc, flash_crc_cb);

		/* Compare Flash CRC with HW CRC */
		if (flash_crc_cb != hw_crc) {
			ILI_INFO("HW and Flash CRC not matched\n");
			return UPDATE_FAIL;
		}
		memset(flash_crc, 0, sizeof(flash_crc));
	}

	if (check_fw_ver) {
#if BOOT_FW_UPDATE_MODE
		if (ilits->force_fw_update) {
			if (tfd.new_fw_cb != ilits->chip->fw_ver) {
				ILI_INFO("FW version not matched\n");
				return UPDATE_FAIL;
			}
		} else {
#if (BOOT_FW_UPDATE_MODE == BOOT_FW_VER_UPGRADE)
			if (tfd.new_fw_cb > ilits->chip->fw_ver || tfd.new_mp_fw_cb > ilits->chip->fw_mp_ver
					|| ilits->chip->fw_ver == 0x0 || ilits->chip->fw_mp_ver == 0x0) {
#elif (BOOT_FW_UPDATE_MODE == BOOT_FW_VER_DOWNGRADE)
			if (tfd.new_fw_cb < ilits->chip->fw_ver || tfd.new_mp_fw_cb < ilits->chip->fw_mp_ver
					|| ilits->chip->fw_ver == 0x0 || ilits->chip->fw_mp_ver == 0x0) {
#endif
				ILI_INFO("after check FW version, do FW Upgrade.\n");
				return UPDATE_FAIL;
			}
			check_hex_hw_crc = DISABLE;
		}
#else
		if (tfd.new_fw_cb != ilits->chip->fw_ver) {
			ILI_INFO("FW version not matched\n");
			return UPDATE_FAIL;
		}
#endif
	}

	if (check_hex_hw_crc == ENABLE) {
		/* Check Hex and HW CRC */
		for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
			if (fbi[i].end == 0)
				continue;

			start_addr = fbi[i].start;
			end_addr = fbi[i].end;

			len = fbi[i].end - fbi[i].start + 1 - 4;
			hex_crc = CalculateCRC32(fbi[i].start, len, pfw);

			if (ili_fw_read_hw_crc(start_addr, end_addr - start_addr - crc_byte_len + 1, &hw_crc) < 0) {
				ILI_ERR("Read HW CRC failed\n");
				return UPDATE_FAIL;
			}

			ILI_INFO("Block = %d, HW CRC = 0x%06x, Hex CRC = 0x%06x\n", i, hw_crc, hex_crc);
			if (hex_crc != hw_crc) {
				ILI_ERR("Hex and HW CRC not matched\n");
				return UPDATE_FAIL;
			}
		}
	}

	ILI_INFO("Flash FW is the same as targe file FW\n");
	return UPDATE_PASS;
}

static int ilitek_tddi_fw_flash_program(u8 *pfw)
{
	int ret = UPDATE_PASS;
	u8 *buf;
	u32 i = 0, j = 0, addr = 0, k = 0, recv_addr = 0;
	int page = ilits->program_page;
	bool skip = true;

	buf = kmalloc(512 * sizeof(u8), GFP_DMA);
	if (ERR_ALLOC_MEM(buf)) {
		ILI_ERR("Failed to allocate buf memory, %ld\n", PTR_ERR(buf));
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].end == 0)
			continue;

		ILI_INFO("Block[%d]: Programing from (0x%x) to (0x%x), tfd.end_addr = 0x%x\n",
				i, fbi[i].start, fbi[i].end, tfd.end_addr);

		for (addr = fbi[i].start; addr < fbi[i].end; addr += page) {
			buf[0] = 0x25;
			buf[3] = 0x04;
			buf[2] = 0x10;
			buf[1] = 0x08;

			for (k = 0; k < page; k++) {
				if (addr + k <= tfd.end_addr)
					buf[4 + k] = pfw[addr + k];
				else
					buf[4 + k] = 0xFF;

				if (buf[4 + k] != 0xFF)
					skip = false;
			}

			if (skip) {
				if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
					ILI_ERR("Write cs high failed\n");
				ret = UPDATE_FAIL;
				goto out;
			}

			ilitek_tddi_flash_write_enable();

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
				ILI_ERR("Write cs low failed\n");

			if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
				ILI_ERR("Write key failed\n");

			if (ili_ice_mode_write(FLASH2_ADDR, 0x2, 1) < 0)
				ILI_ERR("Write 0x2 failed\n");

			recv_addr = ((addr & 0xFF0000) >> 16) | (addr & 0x00FF00) | ((addr & 0x0000FF) << 16);
			if (ili_ice_mode_write(FLASH2_ADDR, recv_addr, 3) < 0)
				ILI_ERR("Write address failed\n");

			if (ilits->wrapper(buf, page + 4, NULL, 0, OFF, OFF) < 0) {
				ILI_ERR("Failed to program data at start_addr = 0x%X, k = 0x%X, addr = 0x%x\n",
				addr, k, addr + k);
				if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
					ILI_ERR("Write cs high failed\n");
				ret = UPDATE_FAIL;
				goto out;
			}

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
				ILI_ERR("Write cs high failed\n");

			if (ilits->flash_mid == 0xEF) {
				mdelay(1);
			} else {
				if (ilitek_tddi_flash_poll_busy(TIMEOUT_PROGRAM) < 0) {
					ret = UPDATE_FAIL;
					goto out;
				}
			}
			if (ilits->fw_update_stat != ((j * 100) / tfd.end_addr)) {
				ilits->fw_update_stat = (j * 100) / tfd.end_addr;
				ILI_DBG("Program flahse data .... %d%c", ilits->fw_update_stat, '%');
			}
		}
	}
	for (i = 0; i < FW_BLOCK_INFO_B1_NUM; i++) {
		if (fbi_b1[i].end == 0)
			continue;

		ILI_INFO("B1 Block[%d]: Programing from (0x%x) to (0x%x), tfd.end_addr = 0x%x\n",
				i, fbi_b1[i].start, fbi_b1[i].end, tfd.end_addr);

		for (addr = fbi_b1[i].start; addr < fbi_b1[i].end; addr += page) {
			buf[0] = 0x25;
			buf[3] = 0x04;
			buf[2] = 0x10;
			buf[1] = 0x08;

			for (k = 0; k < page; k++) {
				if (addr + k <= tfd.end_addr)
					buf[4 + k] = pfw[addr + k];
				else
					buf[4 + k] = 0xFF;

			}

			ilitek_tddi_flash_write_enable();

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
				ILI_ERR("Write cs low failed\n");

			if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
				ILI_ERR("Write key failed\n");

			if (ili_ice_mode_write(FLASH2_ADDR, 0x2, 1) < 0)
				ILI_ERR("Write 0x2 failed\n");

			recv_addr = ((addr & 0xFF0000) >> 16) | (addr & 0x00FF00) | ((addr & 0x0000FF) << 16);
			if (ili_ice_mode_write(FLASH2_ADDR, recv_addr, 3) < 0)
				ILI_ERR("Write address failed\n");

			if (ilits->wrapper(buf, page + 4, NULL, 0, OFF, OFF) < 0) {
				ILI_ERR("Failed to program data at start_addr = 0x%X, k = 0x%X, addr = 0x%x\n",
				addr, k, addr + k);
				if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
					ILI_ERR("Write cs high failed\n");
				ret = UPDATE_FAIL;
				goto out;
			}

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
				ILI_ERR("Write cs high failed\n");

			if (ilits->flash_mid == 0xEF) {
				mdelay(1);
			} else {
				if (ilitek_tddi_flash_poll_busy(TIMEOUT_PROGRAM) < 0) {
					ret = UPDATE_FAIL;
					goto out;
				}
			}
			if (ilits->fw_update_stat != ((j * 100) / tfd.end_addr)) {
				ilits->fw_update_stat = (j * 100) / tfd.end_addr;
				ILI_DBG("Program flahse data .... %d%c", ilits->fw_update_stat, '%');
			}
		}
	}

out:
	ipio_kfree((void **)&buf);
	return ret;
}

static int ilitek_tddi_fw_flash_erase(bool mcu)
{
	int ret = 0;
	u32 i = 0, addr = 0, recv_addr = 0;
	bool bk_erase = false;
	bool ice = atomic_read(&ilits->ice_stat);

	if (!ice) {
		if (ili_ice_mode_ctrl(ENABLE, mcu) < 0)
			ILI_ERR("Enable ice mode failed while erasing flash\n");
	}

	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].end == 0)
			continue;

		ILI_INFO("Block[%d]: Erasing from (0x%x) to (0x%x) \n", i, fbi[i].start, fbi[i].end);

		for (addr = fbi[i].start; addr <= fbi[i].end; addr += ilits->flash_sector) {
			ilitek_tddi_flash_write_enable();

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
				ILI_ERR("Write cs low failed\n");

			if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
				ILI_ERR("Write key failed\n");

			if (addr == fbi[AP].start) {
				/* block erase with 64k bytes. */
				if (ili_ice_mode_write(FLASH2_ADDR, 0xD8, 1) < 0)
					ILI_ERR("Write 0xB at %x failed\n", FLASH2_ADDR);
				bk_erase = true;
			} else {
				/* sector erase with 4k bytes. */
				if (ili_ice_mode_write(FLASH2_ADDR, 0x20, 1) < 0)
					ILI_ERR("Write 0x20 at %x failed\n", FLASH2_ADDR);
			}

			recv_addr = ((addr & 0xFF0000) >> 16) | (addr & 0x00FF00) | ((addr & 0x0000FF) << 16);
			if (ili_ice_mode_write(FLASH2_ADDR, recv_addr, 3) < 0)
				ILI_ERR("Write address failed\n");

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
				ILI_ERR("Write cs high failed\n");

			/* Waitint for flash setting ready */
			mdelay(1);

			if (addr == fbi[AP].start)
				ret = ilitek_tddi_flash_poll_busy(TIMEOUT_PAGE);
			else
				ret = ilitek_tddi_flash_poll_busy(TIMEOUT_SECTOR);

			if (ret < 0)
				return UPDATE_FAIL;

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
				ILI_ERR("Write cs high failed\n");

			if (i == AP && tfd.is80k && bk_erase) {
				addr = 0x10000 - 0x1000;
				bk_erase = false;
			}
		}
	}
	for (i = 0; i < FW_BLOCK_INFO_B1_NUM; i++) {
		if (fbi_b1[i].end == 0)
			continue;

		ILI_INFO("B1 Block[%d]: Erasing from (0x%x) to (0x%x) \n", i, fbi_b1[i].start, fbi_b1[i].end);

		for (addr = fbi_b1[i].start; addr <= fbi_b1[i].end; addr += ilits->flash_sector) {
			ilitek_tddi_flash_write_enable();

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0)
				ILI_ERR("Write cs low failed\n");

			if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
				ILI_ERR("Write key failed\n");

			/* sector erase with 4k bytes. */
			if (ili_ice_mode_write(FLASH2_ADDR, 0x20, 1) < 0)
				ILI_ERR("Write 0x20 at %x failed\n", FLASH2_ADDR);

			recv_addr = ((addr & 0xFF0000) >> 16) | (addr & 0x00FF00) | ((addr & 0x0000FF) << 16);
			if (ili_ice_mode_write(FLASH2_ADDR, recv_addr, 3) < 0)
				ILI_ERR("Write address failed\n");

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
				ILI_ERR("Write cs high failed\n");

			/* Waitint for flash setting ready */
			mdelay(1);


			ret = ilitek_tddi_flash_poll_busy(TIMEOUT_SECTOR);

			if (ret < 0)
				return UPDATE_FAIL;

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0)
				ILI_ERR("Write cs high failed\n");
		}
	}
	if (!ice) {
		if (ili_ice_mode_ctrl(DISABLE, mcu) < 0)
			ILI_ERR("Disable ice mode failed after erase flash\n");
	}
	return UPDATE_PASS;
}

static int ilitek_fw_flash_upgrade(u8 *pfw, bool mcu)
{
	int ret = UPDATE_PASS;
	bool hex_info = ilits->info_from_hex;

	if (ilits->cascade_info_block.nNum != 0) {
		if (ili_cascade_reset_ctrl(ilits->reset, true) < 0) {
			ILI_ERR("TP reset failed during flash progam\n");
			return -EFW_REST;
		}
	} else {
		if (ili_reset_ctrl(ilits->reset) < 0) {
			ILI_ERR("TP reset failed during flash progam\n");
			return -EFW_REST;
		}
	}

	if (ilitek_fw_check_ddi_chunk(pfw) < 0) {
		return -EFW_BACKUP;
	}

	if (hex_info)
		ilits->info_from_hex = DISABLE;

	if (ili_ic_get_fw_ver() < 0) {
		ILI_INFO("Failed to get fw ver\n");
	}

	if (hex_info)
		ilits->info_from_hex = ENABLE;

	/* Check FW version */
	ILI_INFO("New FW ver = 0x%x, Current FW ver = 0x%x, New MP FW ver = 0x%x, Current MP FW ver = 0x%x\n",
			tfd.new_fw_cb, ilits->chip->fw_ver, tfd.new_mp_fw_cb, ilits->chip->fw_mp_ver);
	if (ilits->cascade_info_block.nNum != 0) {
		ret = ili_ice_mode_ctrl_by_mode(ENABLE, mcu, BOTH);
	} else {
		ret = ili_ice_mode_ctrl(ENABLE, mcu);
	}

	if (ret < 0)
		return -EFW_ICE_MODE;

	ret = ilitek_tddi_flash_fw_crc_check(pfw, true);
	if (ret == UPDATE_PASS) {
		if (ilits->cascade_info_block.nNum != 0) {
			if (ili_ice_mode_ctrl_by_mode(DISABLE, OFF, BOTH) < 0) {
				if (ili_cascade_reset_ctrl(ilits->reset, false) < 0) {
					ILI_ERR("TP reset failed during flash progam\n");
					return -EFW_REST;
				}
			}
		} else {
			if (ili_ice_mode_ctrl(DISABLE, mcu) < 0) {
				ILI_ERR("Disable ice mode failed, call reset instead\n");
				if (ili_reset_ctrl(ilits->reset) < 0) {
					ILI_ERR("TP reset failed during flash progam\n");
					return -EFW_REST;
				}
			}
		}
		return UPDATE_PASS;
	} else {
		ILI_INFO("Flash FW and target file FW are different, do upgrade\n");
	}

	ret = ilitek_tddi_flash_protect(DISABLE, OFF);
	if (ret == UPDATE_FAIL)
		return -EFW_FP;

	ret = ilitek_tddi_fw_flash_erase(mcu);
	if (ret == UPDATE_FAIL)
		return -EFW_ERASE;

	ret = ilitek_tddi_fw_flash_program(pfw);
	if (ret == UPDATE_FAIL)
		return -EFW_PROGRAM;

	ilitek_tddi_flash_protect(ENABLE, OFF);

	ret = ilitek_tddi_flash_fw_crc_check(pfw, false);
	if (ret == UPDATE_FAIL)
		return -EFW_CRC;

	/* We do have to reset chip in order to move new code from flash to iram. */
	if (ilits->cascade_info_block.nNum != 0) {
		if (ili_cascade_reset_ctrl(ilits->reset, false) < 0) {
			ILI_ERR("TP reset failed after flash progam\n");
			ret = -EFW_REST;
		}
	} else {
		if (ili_reset_ctrl(ilits->reset) < 0) {
			ILI_ERR("TP reset failed after flash progam\n");
			ret = -EFW_REST;
		}
	}
	return ret;
}

static int ilitek_fw_calc_file_crc(u8 *pfw)
{
	int i, block_num = 0;
	u32 ex_addr, data_crc, file_crc;

	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		if (fbi[i].len >= MAX_HEX_FILE_SIZE) {
			ILI_ERR("Content of fw file is invalid. (fbi[%d].len=0x%x)\n",
				i, fbi[i].len);
			return -1;
		}

		if (fbi[i].end <= 4)
			continue;
		block_num++;
		ex_addr = fbi[i].end;
		data_crc = CalculateCRC32(fbi[i].start, fbi[i].len - 4, pfw);
		file_crc = pfw[ex_addr - 3] << 24 | pfw[ex_addr - 2] << 16 | pfw[ex_addr - 1] << 8 | pfw[ex_addr];
		ILI_DBG("data crc = %x, file crc = %x\n", data_crc, file_crc);
		if (data_crc != file_crc) {
			ILI_ERR("Content of fw file is broken. (%d, %x, %x)\n",
				i, data_crc, file_crc);
			return -1;
		}
	}

	if (fbi[MP].end <= 1 * K || fbi[AP].end <= 1 * K || (block_num == 0)) {
		ILI_ERR("Content of fw file is broken. fbi[AP].end = 0x%x, fbi[MP].end = 0x%x, block_num = %d\n",
			fbi[AP].end, fbi[MP].end, block_num);
		return -1;
	}

	ILI_INFO("Content of fw file is correct\n");
	return 0;
}

static void ilitek_tddi_fw_update_block_info(u8 *pfw)
{
	u32 fw_info_addr = 0, fw_mp_ver_addr = 0;

	fbi[AP].name = "AP";
	fbi[DATA].name = "DATA";
	fbi[TUNING].name = "TUNING";
	fbi[MP].name = "MP";
	fbi[GESTURE].name = "GESTURE";
	fbi[PEN].name = "PEN";

	/* upgrade mode define */
	fbi[DATA].mode = fbi[AP].mode = fbi[TUNING].mode = AP;
	fbi[PEN].mode = AP;
	fbi[MP].mode = MP;
	fbi[GESTURE].mode = GESTURE;

	if (fbi[AP].end > (64*K))
		tfd.is80k = true;

	/* Copy fw info */
	fw_info_addr = fbi[AP].end - INFO_HEX_ST_ADDR;
	ILI_INFO("Parsing hex info start addr = 0x%x\n", fw_info_addr);
	ipio_memcpy(ilits->fw_info, (pfw + fw_info_addr), sizeof(ilits->fw_info), sizeof(ilits->fw_info));

	/* copy fw mp ver */
	fw_mp_ver_addr = fbi[MP].end - INFO_MP_HEX_ADDR;
	ILI_INFO("Parsing hex mp ver addr = 0x%x\n", fw_mp_ver_addr);
	ipio_memcpy(ilits->fw_mp_ver, pfw + fw_mp_ver_addr, sizeof(ilits->fw_mp_ver), sizeof(ilits->fw_mp_ver));
	tfd.new_mp_fw_cb = (ilits->fw_mp_ver[0] << 24) | (ilits->fw_mp_ver[1]<< 16) |
			(ilits->fw_mp_ver[2] << 8) | ilits->fw_mp_ver[3];

	/* copy fw core ver */
	ilits->chip->core_ver = (ilits->fw_info[68] << 24) | (ilits->fw_info[69] << 16) |
			(ilits->fw_info[70] << 8) | ilits->fw_info[71];
	ILI_INFO("New FW Core version = %x\n", ilits->chip->core_ver);

	/* Get hex fw vers */
	tfd.new_fw_cb = (ilits->fw_info[48] << 24) | (ilits->fw_info[49] << 16) |
			(ilits->fw_info[50] << 8) | ilits->fw_info[51];

	/* Get hex report info block*/
	ipio_memcpy(&ilits->rib, ilits->fw_info, sizeof(ilits->rib), sizeof(ilits->rib));
	/*1 byte, Resolution 0-2 bits, CustomType 3-5 bits, PenType 6-7 bits*/
	ilits->rib.nReportResolutionMode = (ilits->chip->core_ver >= CORE_VER_1470) ? (ilits->rib.nReportResolutionMode & 0x07) : POSITION_LOW_RESOLUTION;
	ilits->PenType = (ilits->chip->core_ver >= CORE_VER_1700) ? (ilits->rib.nCustomerType >> 3) : POSITION_PEN_TYPE_OFF;

	if (ilits->chip->core_ver >= CORE_VER_1700) {
		/*CustomerType 3 bits*/
		ilits->rib.nCustomerType = ilits->rib.nCustomerType & 0x07;
		ilits->customertype_off = POSITION_CUSTOMER_TYPE_OFF_3BITS;
	} else {
		/*CustomerType 5 bits*/
		ilits->rib.nCustomerType = (ilits->chip->core_ver >= CORE_VER_1470) ? ilits->rib.nCustomerType : POSITION_CUSTOMER_TYPE_OFF;
		ilits->customertype_off = POSITION_CUSTOMER_TYPE_OFF;
	}

	ILI_INFO("report_info_block : nReportByPixel = %d, nIsHostDownload = %d, nIsSPIICE = %d, nIsSPISLAVE = %d, nIsI2C = %d\n",
		ilits->rib.nReportByPixel, ilits->rib.nIsHostDownload, ilits->rib.nIsSPIICE, ilits->rib.nIsSPISLAVE, ilits->rib.nIsI2C);

	ILI_INFO("report_info_block : nReserved00 = %d, nReportResolutionMode = %d, nCustomerType = %d\n",
		ilits->rib.nReserved00, ilits->rib.nReportResolutionMode, ilits->rib.nCustomerType);

	ILI_INFO("PenType = 0x%x, Customer Type OFF = 0x%x\n", ilits->PenType, ilits->customertype_off);

#if ENABLE_PEN_MODE
	/* Get hex Pen info block*/
	fw_info_addr = fbi[AP].end - INFO_PEN_ST_ADDR;
	ILI_INFO("Parsing pen info block start addr = 0x%x\n", fw_info_addr);
	ipio_memcpy(&ilits->pen_info_block, (pfw + fw_info_addr), sizeof(ilits->pen_info_block), sizeof(ilits->pen_info_block));

	ILI_INFO("pen_info_block : nPxRaw = %d, nPyRaw = %d, nPxVa = %d,nPyVa = %d, nPenX_MP = %d, nPenChipnum = %d, nPenSamplenum = %d, nReserved03 = %d\n",
		ilits->pen_info_block.nPxRaw, ilits->pen_info_block.nPyRaw, ilits->pen_info_block.nPxVa, ilits->pen_info_block.nPyVa, ilits->pen_info_block.nPenX_MP, ilits->pen_info_block.nPenChipnum, ilits->pen_info_block.nPenSamplenum, ilits->pen_info_block.nReserved03);
#endif

#if ENABLE_CASCADE
	/* Get hex Cascade info block*/
	fw_info_addr = fbi[AP].end - INFO_CASCADE_ST_ADDR;
	ILI_INFO("Parsing cascade info block start addr = 0x%x\n", fw_info_addr);
	ipio_memcpy(&ilits->cascade_info_block, (pfw + fw_info_addr), sizeof(ilits->cascade_info_block), sizeof(ilits->cascade_info_block));

	ILI_INFO("cascade_info_block : nDisable = %d, nNum = %d, nReserved0 = %d, nReserved1 = %d, nReserved2 = %d, nReserved03 = %d\n",
		ilits->cascade_info_block.nDisable, ilits->cascade_info_block.nNum, ilits->cascade_info_block.nReserved00, ilits->cascade_info_block.nReserved01, ilits->cascade_info_block.nReserved02, ilits->cascade_info_block.nReserved03);
#else
	ilits->cascade_info_block.nDisable = ENABLE;
	ilits->cascade_info_block.nNum = 0;
	ILI_INFO("cascade_info_block : nDisable = %d, nNum = %d\n", ilits->cascade_info_block.nDisable, ilits->cascade_info_block.nNum);
#endif

	/* Calculate update address */
	ILI_INFO("New FW ver = 0x%x, New MP FW version = 0x%x,\n", tfd.new_fw_cb, tfd.new_mp_fw_cb);
	ILI_INFO("star_addr = 0x%06X, end_addr = 0x%06X, Block Num = %d\n", tfd.start_addr, tfd.end_addr, tfd.block_number);
}

static int ilitek_tddi_fw_ili_convert(u8 *pfw)
{
	int i, size, blk_num = 0, blk_map = 0, num = 0;
	int b0_addr = 0, b0_num = 0;

	if (ERR_ALLOC_MEM(ilits->md_fw_ili))
		return -ENOMEM;

	CTPM_FW = ilits->md_fw_ili;
	size = ilits->md_fw_ili_size;

	if (size < ILI_FILE_HEADER || size > (MAX_HEX_FILE_SIZE + ILI_FILE_HEADER)) {
		ILI_ERR("size of ILI file is invalid\n");
		return -EINVAL;
	}

	/* Check if it's old version of ILI format. */
	if (CTPM_FW[22] == 0xFF && CTPM_FW[23] == 0xFF &&
		CTPM_FW[24] == 0xFF && CTPM_FW[25] == 0xFF) {
		ILI_ERR("Invaild ILI format, abort!\n");
		return -EINVAL;
	}

	blk_num = CTPM_FW[131];
	blk_map = (CTPM_FW[129] << 8) | CTPM_FW[130];
	ILI_INFO("Parsing ILI file, block num = %d, block mapping = %x\n", blk_num, blk_map);

	if (blk_num > (FW_BLOCK_INFO_NUM - 1) || !blk_num || !blk_map) {
		ILI_ERR("Number of block or block mapping is invalid, abort!\n");
		return -EINVAL;
	}

	memset(fbi, 0x0, sizeof(fbi));
	tfd.start_addr = 0;
	tfd.end_addr = 0;
	tfd.hex_tag = BLOCK_TAG_AF;

	/* Parsing block info */
	for (i = 0; i < FW_BLOCK_INFO_NUM; i++) {
		/* B0 tag */
		b0_addr = (CTPM_FW[4 + i * 4] << 16) | (CTPM_FW[5 + i * 4] << 8) | (CTPM_FW[6 + i * 4]);
		b0_num = CTPM_FW[7 + i * 4];
		if ((b0_num != 0) && (b0_addr != 0x000000))
			fbi[b0_num].fix_mem_start = b0_addr;

		/* AF tag */
		num = i + 1;
		if (num >= FW_BLOCK_INFO_NUM)
			break;
		if (((blk_map >> i) & 0x01) == 0x01) {
			fbi[num].start = (CTPM_FW[132 + i * 6] << 16) | (CTPM_FW[133 + i * 6] << 8) | CTPM_FW[134 + i * 6];
			fbi[num].end = (CTPM_FW[135 + i * 6] << 16) | (CTPM_FW[136 + i * 6] << 8) |  CTPM_FW[137 + i * 6];

			if (fbi[num].fix_mem_start == 0)
				fbi[num].fix_mem_start = INT_MAX;

			fbi[num].len = fbi[num].end - fbi[num].start + 1;
			ILI_INFO("Block[%d]: start_addr = %x, end = %x, fix_mem_start = 0x%x\n", num, fbi[num].start,
								fbi[num].end, fbi[num].fix_mem_start);
		}
	}

	memcpy(pfw, CTPM_FW + ILI_FILE_HEADER, size - ILI_FILE_HEADER);

	if (ilitek_fw_calc_file_crc(pfw) < 0)
		return -1;

	tfd.block_number = blk_num;
	tfd.end_addr = size - ILI_FILE_HEADER;
	return 0;
}

static int ilitek_tddi_fw_hex_convert(u8 *phex, int size, u8 *pfw)
{
	int block = 0;
	u32 i = 0, j = 0, k = 0, num = 0;
	u32 len = 0, addr = 0, type = 0;
	u32 start_addr = 0x0, end_addr = 0x0, ex_addr = 0;
	u32 offset;

	memset(fbi, 0x0, sizeof(fbi));
	memset(fbi_b1, 0x0, sizeof(fbi_b1));

	/* Parsing HEX file */
	for (; i < size;) {
		len = HexToDec(&phex[i + 1], 2);
		addr = HexToDec(&phex[i + 3], 4);
		type = HexToDec(&phex[i + 7], 2);

		if (type == 0x04) {
			ex_addr = HexToDec(&phex[i + 9], 4);
		} else if (type == 0x02) {
			ex_addr = HexToDec(&phex[i + 9], 4);
			ex_addr = ex_addr >> 12;
		} else if (type == BLOCK_TAG_AF) {
			/* insert block info extracted from hex */
			tfd.hex_tag = type;
			if (tfd.hex_tag == BLOCK_TAG_AF)
				num = HexToDec(&phex[i + 9 + 6 + 6], 2);
			else
				num = 0xFF;

			if (num > (FW_BLOCK_INFO_NUM - 1)) {
				ILI_ERR("ERROR! block num is larger than its define (%d, %d)\n",
						num, FW_BLOCK_INFO_NUM - 1);
				return -EINVAL;
			}

			fbi[num].start = HexToDec(&phex[i + 9], 6);
			fbi[num].end = HexToDec(&phex[i + 9 + 6], 6);
			fbi[num].fix_mem_start = INT_MAX;
			fbi[num].len = fbi[num].end - fbi[num].start + 1;
			ILI_INFO("Block[%d]: start_addr = %x, end = %x", num, fbi[num].start, fbi[num].end);

			block++;
		} else if (type == BLOCK_TAG_B0 && tfd.hex_tag == BLOCK_TAG_AF) {
			num = HexToDec(&phex[i + 9 + 6], 2);

			if (num > (FW_BLOCK_INFO_NUM - 1)) {
				ILI_ERR("ERROR! block num is larger than its define (%d, %d)\n",
						num, FW_BLOCK_INFO_NUM - 1);
				return -EINVAL;
			}

			fbi[num].fix_mem_start = HexToDec(&phex[i + 9], 6);
			ILI_INFO("Tag 0xB0: change Block[%d] to addr = 0x%x\n", num, fbi[num].fix_mem_start);
		}

		addr = addr + (ex_addr << 16);

		if (phex[i + 1 + 2 + 4 + 2 + (len * 2) + 2] == 0x0D)
			offset = 2;
		else
			offset = 1;

		if (addr >= MAX_HEX_FILE_SIZE) {
			ILI_ERR("Invalid hex format %d\n", addr);
			return -1;
		}

		if (type == 0x00) {
			end_addr = addr + len;
			if (addr < start_addr)
				start_addr = addr;
			/* fill data */
			for (j = 0, k = 0; j < (len * 2); j += 2, k++)
				pfw[addr + k] = HexToDec(&phex[i + 9 + j], 2);
		}
		i += 1 + 2 + 4 + 2 + (len * 2) + 2 + offset;
	}

	if (ilitek_fw_calc_file_crc(pfw) < 0)
		return -1;

	tfd.start_addr = start_addr;
	tfd.end_addr = end_addr;
	tfd.block_number = block;
	return 0;
}

static int ilitek_tdd_fw_hex_open(u8 op, u8 *pfw)
{
	int ret = 0, fsize = 0;
	const struct firmware *fw = NULL;
#if (!GENERIC_KERNEL_IMAGE)
	struct file *f = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)
	mm_segment_t old_fs;
#endif
	loff_t pos = 0;
#endif
	ILI_INFO("Open file method = %s, path = %s\n",
		op ? "FILP_OPEN" : "REQUEST_FIRMWARE",
		op ? ilits->md_fw_filp_path : ilits->md_fw_rq_path);

	switch (op) {
	case REQUEST_FIRMWARE:
		if (request_firmware(&fw, ilits->md_fw_rq_path, ilits->dev) < 0) {
			ILI_ERR("Request firmware failed, try again\n");
			if (request_firmware(&fw, ilits->md_fw_rq_path, ilits->dev) < 0) {
				ILI_ERR("Request firmware failed after retry\n");
				ret = -1;
				goto out;
			}
		}

		fsize = fw->size;
		ILI_INFO("fsize = %d\n", fsize);
		if (fsize <= 0) {
			ILI_ERR("The size of file is zero\n");
			release_firmware(fw);
			ret = -1;
			goto out;
		}

		ilits->tp_fw.size = 0;
		ilits->tp_fw.data = vmalloc(fsize);
		if (ERR_ALLOC_MEM(ilits->tp_fw.data)) {
			ILI_ERR("Failed to allocate tp_fw by vmalloc, try again\n");
			ilits->tp_fw.data = vmalloc(fsize);
			if (ERR_ALLOC_MEM(ilits->tp_fw.data)) {
				ILI_ERR("Failed to allocate tp_fw after retry\n");
				release_firmware(fw);
				ret = -ENOMEM;
				goto out;
			}
		}

		/* Copy fw data got from request_firmware to global */
		ipio_memcpy((u8 *)ilits->tp_fw.data, fw->data, fsize * sizeof(*fw->data), fsize);
		ilits->tp_fw.size = fsize;
		release_firmware(fw);
		break;
	case FILP_OPEN:
#if GENERIC_KERNEL_IMAGE
	ILI_ERR("GKI version not allow drivers to use filp_open\n");
	return -1;
#else
		f = filp_open(ilits->md_fw_filp_path, O_RDONLY, 0644);
		if (ERR_ALLOC_MEM(f)) {
			ILI_ERR("Failed to open the file at %ld\n", PTR_ERR(f));
			ret = -1;
			goto out;
		}

		fsize = f->f_inode->i_size;
		ILI_INFO("fsize = %d\n", fsize);
		if (fsize <= 0) {
			ILI_ERR("The size of file is invaild\n");
			filp_close(f, NULL);
			ret = -1;
			goto out;
		}

		ipio_vfree((void **)&(ilits->tp_fw.data));
		ilits->tp_fw.size = 0;
		ilits->tp_fw.data = vmalloc(fsize);
		if (ERR_ALLOC_MEM(ilits->tp_fw.data)) {
			ILI_ERR("Failed to allocate tp_fw by vmalloc, try again\n");
			ilits->tp_fw.data = vmalloc(fsize);
			if (ERR_ALLOC_MEM(ilits->tp_fw.data)) {
				ILI_ERR("Failed to allocate tp_fw after retry\n");
				filp_close(f, NULL);
				ret = -ENOMEM;
				goto out;
			}
		}

		/* ready to map user's memory to obtain data by reading files */
		pos = 0;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		vfs_read(f, (u8 *)ilits->tp_fw.data, fsize, &pos);
		set_fs(old_fs);
#else
		kernel_read(f, (u8 *)ilits->tp_fw.data, fsize, &pos);
#endif
		filp_close(f, NULL);
		ilits->tp_fw.size = fsize;
#endif
		break;
	default:
		ILI_ERR("Unknown open file method, %d\n", op);
		break;
	}

	if (ERR_ALLOC_MEM(ilits->tp_fw.data) || ilits->tp_fw.size <= 0) {
		ILI_ERR("fw data/size is invaild\n");
		ret = -1;
		goto out;
	}

	/* Convert hex and copy data from tp_fw.data to pfw */
	if (ilitek_tddi_fw_hex_convert((u8 *)ilits->tp_fw.data, ilits->tp_fw.size, pfw) < 0) {
		ILI_ERR("Convert hex file failed\n");
		ret = -1;
	}

out:
	ipio_vfree((void **)&(ilits->tp_fw.data));
	return ret;
}

int ili_fw_upgrade(int op)
{
	int i, ret = 0, retry = 3;

	if (!ilits->boot || ilits->force_fw_update || ERR_ALLOC_MEM(pfw)) {
		if (ERR_ALLOC_MEM(pfw)) {
			ipio_vfree((void **)&pfw);
			pfw = vmalloc(MAX_HEX_FILE_SIZE * sizeof(u8));
			if (ERR_ALLOC_MEM(pfw)) {
				ILI_ERR("Failed to allocate pfw memory, %ld\n", PTR_ERR(pfw));
				ipio_vfree((void **)&pfw);
				ret = -ENOMEM;
				goto out;
			}
		}

		for (i = 0; i < MAX_HEX_FILE_SIZE; i++)
			pfw[i] = 0xFF;

		if (ilitek_tdd_fw_hex_open(op, pfw) < 0) {
			ILI_ERR("Open hex file fail, try upgrade from ILI file\n");

			/*
			 * Users might not be aware of a broken hex file when recovering
			 * fw from ILI file. We should force them to check
			 * hex files they attempt to update via device node.
			 */
			if (ilits->node_update) {
				ILI_ERR("Ignore update from ILI file\n");
				ipio_vfree((void **)&pfw);
				return -EFW_CONVERT_FILE;
			}

			if (ilitek_tddi_fw_ili_convert(pfw) < 0) {
				ILI_ERR("Convert ILI file error\n");
				ret = -EFW_CONVERT_FILE;
				goto out;
			}
		}
		ilitek_tddi_fw_update_block_info(pfw);

		if (ilits->chip->core_ver >= CORE_VER_1470 && ilits->rib.nIsHostDownload == 1) {
			ILI_ERR("hex file interface no match error\n");
			ret = -EFW_INTERFACE;
			goto out;
		}
	}

#if (ENGINEER_FLOW)
	if (!ilits->eng_flow) {
		do {
			ret = ilitek_fw_flash_upgrade(pfw, OFF);
			if (ret == UPDATE_PASS)
				break;

			ILI_ERR("Upgrade failed, do retry!\n");
		} while (--retry > 0);

		if (ret != UPDATE_PASS) {
			if (ret != -EFW_FP && ret != -EFW_BACKUP) {
				ILI_ERR("Failed to upgrade fw %d times, erasing flash\n", retry);
				if (ilitek_tddi_fw_flash_erase(OFF) < 0)
					ILI_ERR("Failed to erase flash\n");
			}

			if (ilits->cascade_info_block.nNum != 0) {
				if (ili_cascade_reset_ctrl(ilits->reset, false) < 0)
					ILI_ERR("TP reset failed after erase flash\n");
			} else {
				if (ili_reset_ctrl(ilits->reset) < 0)
					ILI_ERR("TP reset failed after erase flash\n");
			}
		}
	} else {
		ILI_ERR("eng_flow do reset!\n");
		if (ilits->cascade_info_block.nNum != 0) {
			ili_cascade_reset_ctrl(ilits->reset, true);
		} else {
			ili_reset_ctrl(ilits->reset);
		}
	}
#else
	do {
		ret = ilitek_fw_flash_upgrade(pfw, OFF);
		if (ret == UPDATE_PASS)
			break;

		ILI_ERR("Upgrade failed, do retry!\n");
	} while (--retry > 0);

	if (ret != UPDATE_PASS) {
		if (ret != -EFW_FP && ret != -EFW_BACKUP) {
			ILI_ERR("Failed to upgrade fw %d times, erasing flash\n", retry);
			if (ilitek_tddi_fw_flash_erase(OFF) < 0)
				ILI_ERR("Failed to erase flash\n");
		}

		if (ilits->cascade_info_block.nNum != 0) {
			if (ili_cascade_reset_ctrl(ilits->reset, false) < 0)
					ILI_ERR("TP reset failed after erase flash\n");
		} else {
			if (ili_reset_ctrl(ilits->reset) < 0)
				ILI_ERR("TP reset failed after erase flash\n");
		}
	}
#endif

out:
	if (ret < 0)
		ilits->info_from_hex = DISABLE;

	if (ilits->cascade_info_block.nNum != 0) {
		if (atomic_read(&ilits->ice_stat)) {
			ili_ice_mode_ctrl_by_mode(DISABLE, OFF, BOTH);
		}
	} else {
		if (atomic_read(&ilits->ice_stat))
			ili_ice_mode_ctrl(DISABLE, OFF);
	}

	ili_ic_get_all_info();
	ili_ic_func_ctrl_reset();

	if (!ilits->info_from_hex)
		ilits->info_from_hex = ENABLE;

	return ret;
}

void ili_fw_read_flash_info(bool mcu)
{
	int i = 0, flashIDIndex = 0, flashSignatureIndex = 0, cntOfSupportedFlash = 0;
	u8 buf[3] = {0};
	u8 cmd = 0x9F;
	u32 tmp = 0;
	u32 flash_id = 0;
	u32 signature = 0;
	bool ice = atomic_read(&ilits->ice_stat);

	if (!ice) {
		if (ili_ice_mode_ctrl(ENABLE, mcu) < 0)
			ILI_ERR("Enable ice mode failed while reading flash info\n");
	}

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0) /* CS Low */
		ILI_ERR("Write cs low failed\n");

	if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(FLASH2_ADDR, cmd, 1) < 0) /* Read JEDEC ID */
		ILI_ERR("Write 0x9F failed\n");

	for (i = 0; i < ARRAY_SIZE(buf); i++) {
		if (ili_ice_mode_write(FLASH2_ADDR, 0xFF, 1) < 0)
			ILI_ERR("Write dummy failed\n");

		if (ili_ice_mode_read(FLASH4_ADDR, &tmp, sizeof(u8)) < 0)
			ILI_ERR("Read flash info error\n");

		buf[i] = tmp;
	}

	if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0) /* CS High */
		ILI_ERR("Write cs high failed\n");

	ilits->flash_mid = buf[0];
	ilits->flash_devid = buf[1] << 8 | buf[2];
	ilits->program_page = 256;
	ilits->flash_sector = (4 * K);
	ilits->flashName = "";
	ilits->isSupportFlash = false;
	ilits->supportFlashIndex = 0;

	ILI_INFO("Flash MID = 0x%x, Flash DEV_ID = 0x%x\n", ilits->flash_mid, ilits->flash_devid);

	flash_id = (ilits->flash_mid << 16) + ilits->flash_devid;
	for (flashIDIndex = 0; flashIDIndex < ARRAY_SIZE(flash_protect_list); flashIDIndex++) {
		if (flash_id == flash_protect_list[flashIDIndex].flashUID) {
			ilits->isSupportFlash = true;
			ilits->supportFlashIndex = flashIDIndex;
			cntOfSupportedFlash++;
		}
	}

	if ( ilits->isSupportFlash == false )
		ILI_INFO("Not found flash id in table\n");
	else {
		if (((flash_id >> 16) == 0xC8 || (flash_id >> 16) == 0x85) && cntOfSupportedFlash > 1) { /* special case, need to read flash signature when flash_id start as 0xC8 || 0x85 */
			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x0, 1) < 0) /* CS Low */
				ILI_ERR("Write cs low failed\n");

			if (ili_ice_mode_write(FLASH1_ADDR, 0x66aa55, 3) < 0)
				ILI_ERR("Write key failed\n");

			/* read serial flash discoverable parameter (0x5A) */
			if (ili_ice_mode_write(FLASH2_ADDR, 0x5A, 1) < 0)
				ILI_ERR("Write 0x5A failed\n");

			/* signature address = 0x000000 */
			for (i = 0; i < 3; i++)
				if (ili_ice_mode_write(FLASH2_ADDR, 0x00, 1) < 0)
					ILI_ERR("Write 0x00 failed\n");

			/* write 1 byte dummy */
			if (ili_ice_mode_write(FLASH2_ADDR, 0x00, 1) < 0)
				ILI_ERR("Write 0x00 failed\n");

			/* read 4 byte flash signature */
			for (i = 0; i < 4; i++) {
				if (ili_ice_mode_write(FLASH2_ADDR, 0xFF, 1) < 0)
					ILI_ERR("Write dummy failed\n");

				if (ili_ice_mode_read(FLASH4_ADDR, &tmp, sizeof(u8)) < 0)
					ILI_ERR("Read flash signature error\n");

				signature += (tmp << (i * 8));
			}

			if (ili_ice_mode_write(FLASH_BASED_ADDR, 0x1, 1) < 0) /* CS High */
				ILI_ERR("Write cs high failed\n");

			for (flashSignatureIndex = 0; flashSignatureIndex < ARRAY_SIZE(flash_protect_list); flashSignatureIndex++) {
				if (flash_id == flash_protect_list[flashSignatureIndex].flashUID) {
					for (i = 0; i < flash_protect_list[flashSignatureIndex].flashSignatureCount; i++) {
						if (signature == flash_protect_list[flashSignatureIndex].flashSignature[i])
							goto out;
					}
				}
			}
out:
			if (flashSignatureIndex >= ARRAY_SIZE(flash_protect_list)) {
				ilits->isSupportFlash = false;
				ILI_INFO("Not found flash signature (0x%X) in table\n", signature);
			} else {
				ilits->supportFlashIndex = flashSignatureIndex;
				ilits->flashName = flash_protect_list[flashSignatureIndex].name;
				ILI_INFO("Update Flash Name = %s\n", ilits->flashName);
			}
		} else { /* normal case */
			ilits->flashName = flash_protect_list[ilits->supportFlashIndex].name;
			ILI_INFO("Flash Name = %s\n", ilits->flashName);
		}
	}

	if (!ice) {
		if (ili_ice_mode_ctrl(DISABLE, mcu) < 0)
			ILI_ERR("Disable ice mode failed while reading flash info\n");
	}
}