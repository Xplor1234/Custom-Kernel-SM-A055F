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

/*gesture info mode*/
struct ili_demo_debug_info_id0 {
	u32 id			: 8;
	u32 sys_powr_state_e	: 3;
	u32 sys_state_e 	: 3;
	u32 tp_state_e		: 2;

	u32 touch_palm_state	: 2;
	u32 app_an_statu_e	: 3;
	u32 app_sys_bg_err	: 1;
	u32 g_b_wrong_bg	: 1;
	u32 reserved0		: 1;

	u32 normal_mode 	: 1;
	u32 charger_mode	: 1;
	u32 glove_mode		: 1;
	u32 stylus_mode 	: 1;
	u32 multi_mode		: 1;
	u32 noise_mode		: 1;
	u32 palm_plus_mode	: 1;
	u32 floating_mode	: 1;

	u32 algo_pt_status0	: 3;
	u32 algo_pt_status1	: 3;
	u32 algo_pt_status2	: 3;
	u32 algo_pt_status3	: 3;
	u32 algo_pt_status4	: 3;
	u32 algo_pt_status5	: 3;
	u32 algo_pt_status6	: 3;
	u32 algo_pt_status7	: 3;
	u32 algo_pt_status8	: 3;
	u32 algo_pt_status9	: 3;
	u32 reserved2		: 2;

	u32 hopping_flg 	: 1;
	u32 hopping_index	: 5;
	u32 frequency_h 	: 2;
	u32 frequency_l 	: 8;
	u32 reserved3		: 8;
	u32 reserved4		: 8;

};

void ili_dump_data(void *data, int type, int len, int row_len, const char *name)
{
	int i, row = 31;
	u8 *p8 = NULL;
	s32 *p32 = NULL;
	s16 *p16 = NULL;

	if (!debug_en)
		return;

	if (row_len > 0)
		row = row_len;

	if (data == NULL) {
		ILI_ERR("The data going to dump is NULL\n");
		return;
	}

	pr_cont("\n\n");
	pr_cont("ILITEK: Dump %s data\n", name);
	pr_cont("ILITEK: ");

	if (type == 8)
		p8 = (u8 *) data;
	if (type == 32 || type == 10)
		p32 = (s32 *) data;
	if (type == 16)
		p16 = (s16 *) data;

	for (i = 0; i < len; i++) {
		if (type == 8)
			pr_cont(" %4x ", p8[i]);
		else if (type == 32)
			pr_cont(" %4x ", p32[i]);
		else if (type == 10)
			pr_cont(" %4d ", p32[i]);
		else if (type == 16)
			pr_cont(" %4d ", p16[i]);

		if ((i % row) == row - 1) {
			pr_cont("\n");
			pr_cont("ILITEK: ");
		}
	}
	pr_cont("\n\n");
}

static void dma_clear_reg_setting(void)
{
	/* 1. interrupt t0/t1 enable flag */
	if (ili_ice_mode_bit_mask_write(INTR32_ADDR, INTR32_reg_t0_int_en | INTR32_reg_t1_int_en, (0 << 24)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%d, addr=0x%x failed\n", INTR32_reg_t0_int_en | INTR32_reg_t1_int_en, (0 << 24), INTR32_ADDR);

	/* 2. clear tdi_err_int_flag */
	if (ili_ice_mode_bit_mask_write(INTR2_ADDR, INTR2_tdi_err_int_flag_clear, BIT(18)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR2_tdi_err_int_flag_clear, BIT(18), INTR2_ADDR);

	/* 3. clear dma channel 0 src1 info */
	if (ili_ice_mode_write(DMA49_reg_dma_ch0_src1_addr, 0x00000000, 4) < 0)
		ILI_ERR("Write 0x00000000 at %x failed\n", DMA49_reg_dma_ch0_src1_addr);
	if (ili_ice_mode_write(DMA50_reg_dma_ch0_src1_step_inc, 0x00, 1) < 0)
		ILI_ERR("Write 0x0 at %x failed\n", DMA50_reg_dma_ch0_src1_step_inc);
	if (ili_ice_mode_bit_mask_write(DMA50_ADDR, DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, BIT(31)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, BIT(31), DMA50_ADDR);

	/* 4. clear dma channel 0 trigger select */
	if (ili_ice_mode_bit_mask_write(DMA48_ADDR, DMA48_reg_dma_ch0_trigger_sel | BIT(20), (0 << 16)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%d, addr=0x%x failed\n", DMA48_reg_dma_ch0_trigger_sel | BIT(20), (0 << 16), DMA48_ADDR);
	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, BIT(25)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_flash_int_flag, BIT(25), INTR1_ADDR);

	/* 5. clear dma flash setting */
	ili_flash_clear_dma();
}

static void dma_clear_reg_setting_cascade(void)
{
	/* 1. interrupt t0/t1 enable flag */
	if (ili_ice_mode_bit_mask_write_cascade(INTR32_ADDR, INTR32_reg_t0_int_en | INTR32_reg_t1_int_en, (0 << 24), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%d, addr=0x%x failed\n", INTR32_reg_t0_int_en | INTR32_reg_t1_int_en, (0 << 24), INTR32_ADDR);

	/* 2. clear tdi_err_int_flag */
	if (ili_ice_mode_bit_mask_write_cascade(INTR2_ADDR, INTR2_tdi_err_int_flag_clear, BIT(18), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR2_tdi_err_int_flag_clear, BIT(18), INTR2_ADDR);

	/* 3. clear dma channel 0 src1 info */
	if (ili_ice_mode_write_by_mode(DMA49_reg_dma_ch0_src1_addr, 0x00000000, 4, BOTH) < 0)
		ILI_ERR("Cascade write 0x00000000 at %x failed\n", DMA49_reg_dma_ch0_src1_addr);
	if (ili_ice_mode_write_by_mode(DMA50_reg_dma_ch0_src1_step_inc, 0x00, 1, BOTH) < 0)
		ILI_ERR("Cascade write 0x0 at %x failed\n", DMA50_reg_dma_ch0_src1_step_inc);
	if (ili_ice_mode_bit_mask_write_cascade(DMA50_ADDR, DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, BIT(31), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, BIT(31), DMA50_ADDR);

	/* 4. clear dma channel 0 trigger select */
	if (ili_ice_mode_bit_mask_write_cascade(DMA48_ADDR, DMA48_reg_dma_ch0_trigger_sel | BIT(20), (0 << 16), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%d, addr=0x%x failed\n", DMA48_reg_dma_ch0_trigger_sel | BIT(20), (0 << 16), DMA48_ADDR);
	if (ili_ice_mode_bit_mask_write_cascade(INTR1_ADDR, INTR1_reg_flash_int_flag, BIT(25), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_flash_int_flag, BIT(25), INTR1_ADDR);

	/* 5. clear dma flash setting */
	ili_flash_clear_dma_cascade();
}

static void dma_trigger_reg_setting(u32 reg_dest_addr, u32 flash_start_addr, u32 copy_size)
{
	int retry = 30;
	u32 stat = 0;

	/* 1. set dma channel 0 clear */
	if (ili_ice_mode_bit_mask_write(DMA48_ADDR, DMA48_reg_dma_ch0_start_clear, BIT(25)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", DMA48_reg_dma_ch0_start_clear, BIT(25), DMA48_ADDR);

	/* 2. set dma channel 0 src1 info */
	if (ili_ice_mode_write(DMA49_reg_dma_ch0_src1_addr, 0x00041010, 4) < 0)
		ILI_ERR("Write 0x00041010 at %x failed\n", DMA49_reg_dma_ch0_src1_addr);
	if (ili_ice_mode_write(DMA50_reg_dma_ch0_src1_step_inc, 0x00, 1) < 0)
		ILI_ERR("Write 0x00 at %x failed\n", DMA50_reg_dma_ch0_src1_step_inc);
	if (ili_ice_mode_bit_mask_write(DMA50_ADDR, DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, BIT(31)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, BIT(31), DMA50_ADDR);

	/* 3. set dma channel 0 src2 info */
	if (ili_ice_mode_bit_mask_write(DMA52_ADDR, DMA52_reg_dma_ch0_src2_en, (0 << 31)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%d, addr=0x%x failed\n", DMA52_reg_dma_ch0_src2_en, (0 << 31), DMA52_ADDR);

	/* 4. set dma channel 0 dest info */
	if (ili_ice_mode_write(DMA53_reg_dma_ch0_dest_addr, reg_dest_addr, 3) < 0)
		ILI_ERR("Write %x at %x failed\n", reg_dest_addr, DMA53_reg_dma_ch0_dest_addr);
	if (ili_ice_mode_write(DMA54_reg_dma_ch0_dest_step_inc, 0x01, 1) < 0)
		ILI_ERR("Write 0x01 at %x failed\n", DMA54_reg_dma_ch0_dest_step_inc);
	if (ili_ice_mode_bit_mask_write(DMA54_ADDR, DMA54_reg_dma_ch0_dest_format | DMA54_reg_dma_ch0_dest_en, BIT(31)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", DMA54_reg_dma_ch0_dest_format | DMA54_reg_dma_ch0_dest_en, BIT(31), DMA54_ADDR);

	/* 5. set dma channel 0 trafer info */
	if (ili_ice_mode_write(DMA55_reg_dma_ch0_trafer_counts, copy_size, 4) < 0)
		ILI_ERR("Write %x at %x failed\n", copy_size, DMA55_reg_dma_ch0_trafer_counts);
	if (ili_ice_mode_bit_mask_write(DMA55_ADDR, DMA55_reg_dma_ch0_trafer_mode, (0 << 24)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%d, addr=0x%x failed\n", DMA55_reg_dma_ch0_trafer_mode, (0 << 24), DMA55_ADDR);

	/* 6. set dma channel 0 int info */
	/*if (ili_ice_mode_bit_mask_write(INTR33_ADDR, INTR33_reg_dma_ch0_int_en, BIT(16)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR33_reg_dma_ch0_int_en, BIT(16), INTR33_ADDR);*/

	/* 7. set dma channel 0 trigger select */
	if (ili_ice_mode_bit_mask_write(DMA48_ADDR, DMA48_reg_dma_ch0_trigger_sel | BIT(20), BIT(16)) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", DMA48_reg_dma_ch0_trigger_sel | BIT(20), BIT(16), DMA48_ADDR);

	/* 8. set dma flash setting */
	ili_flash_dma_write(flash_start_addr, (flash_start_addr+copy_size), copy_size);

	/* 9. clear flash and dma ch0 int flag */
	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_dma_ch0_int_flag | INTR1_reg_flash_int_flag, (BIT(16) | BIT(25))) < 0)
		ILI_ERR("Mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_dma_ch0_int_flag | INTR1_reg_flash_int_flag, (BIT(16) | BIT(25)), INTR1_ADDR);
	if (ili_ice_mode_bit_mask_write(0x041013, BIT(0), 1) < 0) /* patch */
		ILI_ERR("Mask write: mask=%lu, value=%d, addr=0x%x failed\n", BIT(0), 1, 0x041013);

	/* DMA Trigger */
	if (ili_ice_mode_write(FLASH4_reg_rcv_data, 0xFF, 1) < 0)
		ILI_ERR("Trigger DMA failed\n");

	/* waiting for fw reload code completed. */
	while (retry > 0) {
		if (ili_ice_mode_read(INTR1_ADDR, &stat, sizeof(u32)) < 0) {
			ILI_ERR("Read 0x%x error\n", INTR1_ADDR);
			retry--;
			continue;
		}

		ILI_DBG("fw dma stat = %x\n", stat);

		if ((stat & BIT(16)) == BIT(16))
			break;

		retry--;
		usleep_range(1000, 1000);
	}

	if (retry <= 0)
		ILI_ERR("DMA fail: Regsiter = 0x%x Flash = 0x%x, Size = %d\n",
			reg_dest_addr, flash_start_addr, copy_size);

	/* CS High */
	if (ili_ice_mode_write(FLASH0_reg_flash_csb, 0x1, 1) < 0)
		ILI_ERR("Pull CS High failed\n");
	/* waiting for CS status done */
	mdelay(10);
}

static void dma_trigger_reg_setting_cascade(u32 reg_dest_addr, u32 flash_start_addr, u32 copy_size)
{
	int retry = 30;
	u32 stat = 0;

	/* 1. set dma channel 0 clear */
	if (ili_ice_mode_bit_mask_write_cascade(DMA48_ADDR, DMA48_reg_dma_ch0_start_clear, BIT(25), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", DMA48_reg_dma_ch0_start_clear, BIT(25), DMA48_ADDR);

	/* 2. set dma channel 0 src1 info */
	if (ili_ice_mode_write_by_mode(DMA49_reg_dma_ch0_src1_addr, 0x00041010, 4, BOTH) < 0)
		ILI_ERR("Cascade write 0x00041010 at %x failed\n", DMA49_reg_dma_ch0_src1_addr);
	if (ili_ice_mode_write_by_mode(DMA50_reg_dma_ch0_src1_step_inc, 0x00, 1, BOTH) < 0)
		ILI_ERR("Cascade write 0x00 at %x failed\n", DMA50_reg_dma_ch0_src1_step_inc);
	if (ili_ice_mode_bit_mask_write_cascade(DMA50_ADDR, DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, BIT(31), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, BIT(31), DMA50_ADDR);

	/* 3. set dma channel 0 src2 info */
	if (ili_ice_mode_bit_mask_write_cascade(DMA52_ADDR, DMA52_reg_dma_ch0_src2_en, (0 << 31), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%d, addr=0x%x failed\n", DMA52_reg_dma_ch0_src2_en, (0 << 31), DMA52_ADDR);

	/* 4. set dma channel 0 dest info */
	if (ili_ice_mode_write_by_mode(DMA53_reg_dma_ch0_dest_addr, reg_dest_addr, 3, BOTH) < 0)
		ILI_ERR("Cascade write %x at %x failed\n", reg_dest_addr, DMA53_reg_dma_ch0_dest_addr);
	if (ili_ice_mode_write_by_mode(DMA54_reg_dma_ch0_dest_step_inc, 0x01, 1, BOTH) < 0)
		ILI_ERR("Cascade write 0x01 at %x failed\n", DMA54_reg_dma_ch0_dest_step_inc);

	if (ili_ice_mode_bit_mask_write_cascade(DMA54_ADDR, DMA54_reg_dma_ch0_dest_format | DMA54_reg_dma_ch0_dest_en, BIT(31), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", DMA54_reg_dma_ch0_dest_format | DMA54_reg_dma_ch0_dest_en, BIT(31), DMA54_ADDR);

	/* 5. set dma channel 0 trafer info */
	if (ili_ice_mode_write_by_mode(DMA55_reg_dma_ch0_trafer_counts, copy_size, 4, BOTH) < 0)
		ILI_ERR("Cascade write %x at %x failed\n", copy_size, DMA55_reg_dma_ch0_trafer_counts);
	if (ili_ice_mode_bit_mask_write_cascade(DMA55_ADDR, DMA55_reg_dma_ch0_trafer_mode, (0 << 24), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%d, addr=0x%x failed\n", DMA55_reg_dma_ch0_trafer_mode, (0 << 24), DMA55_ADDR);

	/* 6. set dma channel 0 int info */
	/*if (ili_ice_mode_bit_mask_write_cascade(INTR33_ADDR, INTR33_reg_dma_ch0_int_en, BIT(16), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR33_reg_dma_ch0_int_en, BIT(16), INTR33_ADDR);*/

	/* 7. set dma channel 0 trigger select */
	if (ili_ice_mode_bit_mask_write_cascade(DMA48_ADDR, DMA48_reg_dma_ch0_trigger_sel | BIT(20), BIT(16), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", DMA48_reg_dma_ch0_trigger_sel | BIT(20), BIT(16), DMA48_ADDR);

	/* 8. set dma flash setting */
	ili_flash_dma_write_cascade(flash_start_addr, (flash_start_addr+copy_size), copy_size);

	/* 9. clear flash and dma ch0 int flag */
	if (ili_ice_mode_bit_mask_write_cascade(INTR1_ADDR, INTR1_reg_dma_ch0_int_flag | INTR1_reg_flash_int_flag, (BIT(16) | BIT(25)), BOTH) < 0)
		ILI_ERR("Cascade mask write: mask=%lu, value=%lu, addr=0x%x failed\n", INTR1_reg_dma_ch0_int_flag | INTR1_reg_flash_int_flag, (BIT(16) | BIT(25)), INTR1_ADDR);
	if (ili_ice_mode_bit_mask_write_cascade(0x041013, BIT(0), 1, BOTH) < 0) /* patch */
		ILI_ERR("Cascade mask write: mask=%lu, value=%d, addr=0x%x failed\n", BIT(0), 1, 0x041013);

	/* DMA Trigger */
	if (ili_ice_mode_write_by_mode(FLASH4_reg_rcv_data, 0xFF, 1, BOTH) < 0)
		ILI_ERR("Cascade trigger DMA failed\n");

	/* waiting for fw reload code completed. */
	while (retry > 0) {
		if (ili_ice_mode_read_by_mode(INTR1_ADDR, &stat, sizeof(u32), MASTER) < 0) {
			ILI_ERR("Read 0x%x error\n", INTR1_ADDR);
			retry--;
			continue;
		}

		ILI_DBG("fw dma stat = %x\n", stat);

		if ((stat & BIT(16)) == BIT(16))
			break;

		retry--;
		usleep_range(1000, 1000);
	}

	if (retry <= 0)
		ILI_ERR("DMA fail: Regsiter = 0x%x Flash = 0x%x, Size = %d\n",
			reg_dest_addr, flash_start_addr, copy_size);

	/* CS High */
	if (ili_ice_mode_write_by_mode(FLASH0_reg_flash_csb, 0x1, 1, BOTH) < 0)
		ILI_ERR("Cascade pull CS High failed\n");
	/* waiting for CS status done */
	mdelay(10);
}

int ili_move_mp_code_flash(void)
{
	int ret = 0, retry = 10;
	u32 mp_text_size = 0, mp_andes_init_size = 0, stat = 0;
	u32 mp_flash_addr, mp_size, overlay_start_addr, overlay_end_addr;
	u8 overlay_drv_enable = 0;
	u8 cmd[2] = {0};
	u8 data[16] = {0};

	cmd[0] = P5_X_MP_TEST_MODE_INFO;
	ret = ilits->wrapper(cmd, sizeof(u8), data, ilits->protocol->mp_info_len, ON, OFF);
	ili_dump_data(data, 8, ilits->protocol->mp_info_len, 0, "MP overlay info");
	if (ret < 0) {
		ILI_ERR("Failed to write info cmd\n");
		goto out;
	}

	mp_flash_addr = data[3] + (data[2] << 8) + (data[1] << 16);
	mp_size = data[6] + (data[5] << 8) + (data[4] << 16);
	overlay_start_addr = data[9] + (data[8] << 8) + (data[7] << 16);
	overlay_end_addr = data[12] + (data[11] << 8) + (data[10] << 16);

	if (overlay_start_addr != 0x0 && overlay_end_addr != 0x0
			&& data[0] == P5_X_MP_TEST_MODE_INFO)
		overlay_drv_enable = 1;

	ILI_INFO("MP info Overlay: Enable = %d, addr = 0x%x ~ 0x%x, flash addr = 0x%x, mp size = 0x%x\n",
		overlay_drv_enable, overlay_start_addr,
		overlay_end_addr, mp_flash_addr, mp_size);

	if (ilits->chip->core_ver >= CORE_VER_1700) {
		cmd[0] = P5_X_NEW_CONTROL_FORMAT;
	} else {
		cmd[0] = P5_X_MODE_CONTROL;
	}
	cmd[1] = P5_X_FW_TEST_MODE;
	ret = ilits->wrapper(cmd, 2, NULL, 0, ON, OFF);
	if (ret < 0)
		goto out;

	/* Check if ic is ready switching test mode from demo mode */
	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	ret = ili_ic_check_busy(20, 50, ON); /* Set busy as 0x41 */
	if (ret < 0)
		goto out;

	ilits->ice_flash_cs_ctrl = DISABLE;
	if (ilits->cascade_info_block.nNum != 0) {
		ret = ili_ice_mode_ctrl_by_mode(ENABLE, OFF, BOTH);
	} else {
		ret = ili_ice_mode_ctrl(ENABLE, OFF);
	}
	ilits->ice_flash_cs_ctrl = ENABLE;

	if (ret < 0)
		goto out;

	if (overlay_drv_enable == 1) {
		mp_andes_init_size = overlay_start_addr;
		mp_text_size = (mp_size - overlay_end_addr) + 1;
		ILI_INFO("MP andes init size = %d , MP text size = %d\n", mp_andes_init_size, mp_text_size);

		if (ilits->cascade_info_block.nNum != 0) {
			/* Support 9882T & 9882V */
			ILI_INFO("MP Cascade Overlay Drv\n");
			dma_clear_reg_setting_cascade();
			ILI_INFO("[Cascade Move ANDES.INIT to DRAM]\n");
			dma_trigger_reg_setting_cascade(0, mp_flash_addr, mp_andes_init_size);	 /* DMA ANDES.INIT */
			dma_clear_reg_setting_cascade();
			ILI_INFO("[Cascade Move MP.TEXT to DRAM]\n");
			dma_trigger_reg_setting_cascade(overlay_end_addr, (mp_flash_addr + overlay_start_addr), mp_text_size);
			dma_clear_reg_setting_cascade();
		} else {
			ILI_INFO("MP Overlay Drv\n");
			dma_clear_reg_setting();
			ILI_INFO("[Move ANDES.INIT to DRAM]\n");
			dma_trigger_reg_setting(0, mp_flash_addr, mp_andes_init_size);	 /* DMA ANDES.INIT */
			dma_clear_reg_setting();
			ILI_INFO("[Move MP.TEXT to DRAM]\n");
			dma_trigger_reg_setting(overlay_end_addr, (mp_flash_addr + overlay_start_addr), mp_text_size);
			dma_clear_reg_setting();
		}
	} else {
		if (ilits->cascade_info_block.nNum != 0) {
			/* DMA Trigger */
			if (ili_ice_mode_write_by_mode(FLASH4_reg_rcv_data, 0xFF, 1, BOTH) < 0)
				ILI_ERR("Trigger DMA failed\n");

			/* waiting for fw reload code completed. */
			while (retry > 0) {
				if (ili_ice_mode_read_by_mode(INTR1_FLASH_INT_FLAG, &stat, sizeof(u32), MASTER) < 0) {
					ILI_ERR("Read 0x%x error\n", INTR1_FLASH_INT_FLAG);
					retry--;
					continue;
				}

				ILI_DBG("fw dma stat = 0x%x\n", stat);

				if ((stat & BIT(1)) == BIT(1))
					break;

				retry--;
				usleep_range(10000, 10000);
			}

			if (retry <= 0)
				ILI_ERR("Polling DMA fail: Regsiter = 0x%X state = 0x%X\n", INTR1_FLASH_INT_FLAG, stat);

			/* CS High */
			if (ili_ice_mode_write_by_mode(FLASH0_reg_flash_csb, 0x1, 1, BOTH) < 0)
				ILI_ERR("Pull CS High failed\n");

			/* waiting for CS status done */
			mdelay(10);
		} else {
			if (ili_ice_mode_write(FLASH4_reg_rcv_data, 0xFF, 1) < 0)
				ILI_ERR("Trigger DMA failed\n");

			/* waiting for fw reload code completed. */
			while (retry > 0) {
				if (ili_ice_mode_read(INTR1_FLASH_INT_FLAG, &stat, sizeof(u32)) < 0) {
					ILI_ERR("Read 0x%x error\n", INTR1_FLASH_INT_FLAG);
					retry--;
					continue;
				}

				ILI_DBG("fw dma stat = 0x%x\n", stat);

				if ((stat & BIT(1)) == BIT(1))
					break;

				retry--;
				usleep_range(10000, 10000);
			}

			if (retry <= 0)
				ILI_ERR("Polling DMA fail: Regsiter = 0x%X state = 0x%X\n", INTR1_FLASH_INT_FLAG, stat);

			/* CS High */
			if (ili_ice_mode_write(FLASH0_reg_flash_csb, 0x1, 1) < 0)
				ILI_ERR("Pull CS High failed\n");

			/* waiting for CS status done */
			mdelay(10);
		}
	}

	if (ilits->cascade_info_block.nNum != 0) {
		if (ili_cascade_reset_ctrl(TP_IC_WHOLE_RST_WITHOUT_FLASH, false) < 0)
			ILI_ERR("IC whole reset without flash failed during moving mp code\n");
	} else {
		if (ili_reset_ctrl(TP_IC_WHOLE_RST_WITHOUT_FLASH) < 0)
			ILI_ERR("IC whole reset without flash failed during moving mp code\n");
	}

	/* Check if ic is already in test mode */
	ilits->actual_tp_mode = P5_X_FW_TEST_MODE; /* set busy as 0x51 */
	ret = ili_ic_check_busy(20, 50, ON);
	if (ret < 0)
		ILI_ERR("Check cdc timeout failed after moved mp code\n");

out:
	return ret;
}

int ili_move_mp_code_iram(void)
{
	ILI_INFO("Download MP code to iram\n");
	return ili_fw_upgrade_handler(NULL);
}

int ili_proximity_near(int mode)
{
	int ret = 0;

	ilits->prox_near = true;

	switch (mode) {
	case DDI_POWER_ON:
		/*
		 * If the power of VSP and VSN keeps alive when proximity near event
		 * occures, TP can just go to sleep in.
		 */
		ret = ili_ic_func_ctrl("sleep", SLEEP_IN);
		if (ret < 0)
			ILI_ERR("Write sleep in cmd failed\n");
		break;
	case DDI_POWER_OFF:
		ILI_INFO("DDI POWER OFF, do nothing\n");
		break;
	default:
		ILI_ERR("Unknown mode (%d)\n", mode);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int ili_proximity_far(int mode)
{
	int ret = 0;
	u8 cmd[2] = {0};
	u8 data_type = P5_X_FW_SIGNAL_DATA_MODE;

	if (!ilits->prox_near) {
		ILI_INFO("No proximity near event, break\n");
		return 0;
	}

	switch (mode) {
	case WAKE_UP_GESTURE_RECOVERY:
		/*
		 * If the power of VSP and VSN has been shut down previsouly,
		 * TP should go through gesture recovery to get back.
		 */
		ili_gesture_recovery();
		break;
	case WAKE_UP_SWITCH_GESTURE_MODE:
		/*
		 * If the power of VSP and VSN keeps alive in the event of proximity near,
		 * TP can be just recovered by switching gesture mode to get back.
		 */
		cmd[0] = 0xF6;
		cmd[1] = 0x0A;

		ILI_INFO("write prepare gesture command 0xF6 0x0A\n");
		ret = ilits->wrapper(cmd, 2, NULL, 0, ON, OFF);
		if (ret < 0) {
			ILI_INFO("write prepare gesture command error\n");
			break;
		}

		ret = ili_set_tp_data_len(ilits->gesture_mode, true, &data_type);
		if (ret < 0)
			ILI_ERR("Switch to gesture mode failed during proximity far\n");
		break;
	default:
		ILI_ERR("Unknown mode (%d)\n", mode);
		ret = -EINVAL;
		break;
	}

	ilits->prox_near = false;

	return ret;
}

int ili_move_gesture_code_flash(int mode)
{
	int ret = 0;
	u8 data_type = P5_X_FW_SIGNAL_DATA_MODE;

	/*
	 * NOTE: If functions need to be added during suspend,
	 * they must be called before gesture cmd reaches to FW.
	 */

	ILI_INFO("Gesture mode = %d\n",  mode);
	ret = ili_set_tp_data_len(mode, true, &data_type);

	return ret;
}

void ili_set_gesture_symbol(void)
{
	u8 cmd[7] = {0};
	struct gesture_symbol *ptr_sym = &ilits->ges_sym;
	u8 *ptr;

	ptr = (u8 *) ptr_sym;
	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = 0x01;
	cmd[2] = 0x0A;
	cmd[3] = 0x08;
	cmd[4] = ptr[0];
	cmd[5] = ptr[1];
	cmd[6] = ptr[2];

	ili_dump_data(cmd, 8, sizeof(cmd), 0, "Gesture symbol");

	if (ilits->wrapper(cmd, 2, NULL, 0, ON, OFF) < 0) {
		ILI_ERR("Write pre cmd failed\n");
		return;

	}

	if (ilits->wrapper(&cmd[1], (sizeof(cmd) - 1), NULL, 0, ON, OFF)) {
		ILI_ERR("Write gesture symbol fail\n");
		return;
	}

	ILI_DBG(" double_tap = %d\n", ilits->ges_sym.double_tap);
	ILI_DBG(" alphabet_line_2_top = %d\n", ilits->ges_sym.alphabet_line_2_top);
	ILI_DBG(" alphabet_line_2_bottom = %d\n", ilits->ges_sym.alphabet_line_2_bottom);
	ILI_DBG(" alphabet_line_2_left = %d\n", ilits->ges_sym.alphabet_line_2_left);
	ILI_DBG(" alphabet_line_2_right = %d\n", ilits->ges_sym.alphabet_line_2_right);
	ILI_DBG(" alphabet_w = %d\n", ilits->ges_sym.alphabet_w);
	ILI_DBG(" alphabet_c = %d\n", ilits->ges_sym.alphabet_c);
	ILI_DBG(" alphabet_E = %d\n", ilits->ges_sym.alphabet_E);
	ILI_DBG(" alphabet_V = %d\n", ilits->ges_sym.alphabet_V);
	ILI_DBG(" alphabet_O = %d\n", ilits->ges_sym.alphabet_O);
	ILI_DBG(" alphabet_S = %d\n", ilits->ges_sym.alphabet_S);
	ILI_DBG(" alphabet_Z = %d\n", ilits->ges_sym.alphabet_Z);
	ILI_DBG(" alphabet_V_down = %d\n", ilits->ges_sym.alphabet_V_down);
	ILI_DBG(" alphabet_V_left = %d\n", ilits->ges_sym.alphabet_V_left);
	ILI_DBG(" alphabet_V_right = %d\n", ilits->ges_sym.alphabet_V_right);
	ILI_DBG(" alphabet_two_line_2_bottom= %d\n", ilits->ges_sym.alphabet_two_line_2_bottom);
	ILI_DBG(" alphabet_F= %d\n", ilits->ges_sym.alphabet_F);
	ILI_DBG(" alphabet_AT= %d\n", ilits->ges_sym.alphabet_AT);
	ILI_DBG(" single_tap = %d\n", ilits->ges_sym.single_tap);
}

int ili_move_gesture_code_iram(int mode)
{
	int i, ret = 0, timeout = 100;
	u8 cmd[2] = {0};
	u8 cmd_write[3] = {0x01, 0x0A, 0x05};
	u8 data_type = P5_X_FW_SIGNAL_DATA_MODE;
	u8 backup_rec_state = 0;

	/*
	 * NOTE: If functions need to be added during suspend,
	 * they must be called before gesture cmd reaches to FW.
	 */

	ILI_INFO("Gesture code loaded by %s\n", ilits->gesture_load_code ? "driver" : "firmware");

	if (!ilits->gesture_load_code) {
		ret = ili_set_tp_data_len(mode, true, &data_type);
		goto out;
	}

	/*pre-command for ili_ic_func_ctrl("lpwg", 0x3)*/
	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = 0x1;
	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, OFF, OFF);
	if (ret < 0) {
		ILI_ERR("Write 0xF6,0x1 failed\n");
		goto out;
	}

	ret = ili_ic_func_ctrl("lpwg", 0x3);
	if (ret < 0) {
		ILI_ERR("write gesture flag failed\n");
		goto out;
	}

	ret = ili_set_tp_data_len(mode, true, &data_type);
	if (ret < 0) {
		ILI_ERR("Failed to set tp data length\n");
		goto out;
	}

	ili_irq_enable();
	/* Prepare Check Ready */
	cmd[0] = P5_X_READ_DATA_CTRL;
	cmd[1] = 0xA;
	ret = ilits->wrapper(cmd, sizeof(cmd), NULL, 0, OFF, OFF);
	if (ret < 0) {
		ILI_ERR("Write 0xF6,0xA failed\n");
		goto out;
	}

	for (i = 0; i < timeout; i++) {
		/* Check ready for load code */
		ret = ilits->wrapper(cmd_write, sizeof(cmd_write), cmd, sizeof(u8), ON, OFF);
		ILI_DBG("gesture ready byte = 0x%x\n", cmd[0]);

		if (cmd[0] == 0x91) {
			ILI_INFO("Ready to load gesture code\n");
			break;
		}
		mdelay(2);
	}
	ili_irq_disable();

	if (i >= timeout) {
		ILI_ERR("Gesture is not ready (0x%x), try to run its recovery\n", cmd[0]);
		return ili_gesture_recovery();
	}

	if (ilits->prox_face_mode == PROXIMITY_SUSPEND_RESUME) {
		backup_rec_state = func_ctrl[FUNC_CTRL_PROXIMITY_IDX].rec_state;
		func_ctrl[FUNC_CTRL_PROXIMITY_IDX].rec_state = DISABLE;
		ILI_INFO("Doing gesture dynamic load code, Cannot reset %s CMD, rec_state %d \n", func_ctrl[FUNC_CTRL_PROXIMITY_IDX].name, func_ctrl[FUNC_CTRL_PROXIMITY_IDX].rec_state);
	}

	ret = ili_fw_upgrade_handler(NULL);
	if (ret < 0) {
		ILI_ERR("FW upgrade failed during moving code\n");
		goto out;
	}

	if (ilits->prox_face_mode == PROXIMITY_SUSPEND_RESUME) {
		func_ctrl[FUNC_CTRL_PROXIMITY_IDX].rec_state = backup_rec_state;
		ILI_INFO("Recovery %s rec_state %d \n", func_ctrl[FUNC_CTRL_PROXIMITY_IDX].name, func_ctrl[FUNC_CTRL_PROXIMITY_IDX].rec_state);
	}

	/* Resume gesture loader */
	ret = ili_ic_func_ctrl("lpwg", 0x6);
	if (ilits->prox_face_mode == PROXIMITY_SUSPEND_RESUME && ilits->proxmity_face == true) {
		ret = ili_ic_func_ctrl("lpwg", 0x21);
	}

	if (ret < 0) {
		ILI_ERR("write resume loader error");
		goto out;
	}

out:
	return ret;
}

u8 ili_calc_packet_checksum(u8 *packet, int len)
{
	int i;
	s32 sum = 0;

	for (i = 0; i < len; i++)
		sum += packet[i];

	return (u8) ((-sum) & 0xFF);
}

int ili_touch_esd_gesture_flash(void)
{
	int ret = 0, retry = 100;
	u32 answer = 0, slave_answer = 0;
	int ges_pwd_addr = I2C_ESD_GESTURE_CORE146_PWD_ADDR;
	int ges_pwd = ESD_GESTURE_CORE146_PWD;
	int ges_run = I2C_ESD_GESTURE_CORE146_RUN;
	int pwd_len = 2;
	u8 data_type = P5_X_FW_SIGNAL_DATA_MODE;

	if (ilits->chip->core_ver < CORE_VER_1460) {
		ges_pwd_addr = I2C_ESD_GESTURE_PWD_ADDR;
		ges_pwd = ESD_GESTURE_PWD;
		ges_run = I2C_ESD_GESTURE_RUN;
		pwd_len = 4;
	}

	if (ilits->cascade_info_block.nNum != 0) {
		ret = ili_ice_mode_ctrl_by_mode(ENABLE, OFF, BOTH);
	} else {
		ret = ili_ice_mode_ctrl(ENABLE, OFF);
	}

	if (ret < 0) {
		ILI_ERR("Enable ice mode failed during gesture recovery\n");
		return ret;
	}

	ILI_INFO("ESD Gesture PWD Addr = 0x%X, PWD = 0x%X, GES_RUN = 0%X, core_ver = 0x%X\n",
		ges_pwd_addr, ges_pwd, ges_run, ilits->chip->core_ver);

	/* write a special password to inform FW go back into gesture mode */
	if (ilits->cascade_info_block.nNum != 0) {
		ret = ili_ice_mode_write_by_mode(ges_pwd_addr, ges_pwd, pwd_len, BOTH);
	} else {
		ret = ili_ice_mode_write(ges_pwd_addr, ges_pwd, pwd_len);
	}

	if (ret < 0) {
		ILI_ERR("write password failed\n");
		goto out;
	}

	/* HW reset gives effect to FW receives password successed */
	ilits->actual_tp_mode = P5_X_FW_AP_MODE;

	if (ilits->cascade_info_block.nNum != 0) {
		ret = ili_cascade_reset_ctrl(ilits->reset, false);
	} else {
		ret = ili_reset_ctrl(ilits->reset);
	}

	if (ret < 0) {
		ILI_ERR("TP Reset failed during gesture recovery\n");
		goto out;
	}

	if (ilits->cascade_info_block.nNum != 0) {
		ili_ice_mode_ctrl_by_mode(ENABLE, ON, BOTH);
	} else {
		ret = ili_ice_mode_ctrl(ENABLE, ON);
	}

	if (ret < 0) {
		ILI_ERR("Enable ice mode failed during gesture recovery\n");
		goto out;
	}

	/* polling Cascade Slave another specific register to see if gesutre is enabled properly */
	if (ilits->cascade_info_block.nNum != 0) {
		do {
			ret = ili_ice_mode_read_by_mode(ges_pwd_addr, &slave_answer, pwd_len, SLAVE);

			if (ret < 0) {
				ILI_ERR("Read Slave gesture answer error\n");
				goto out;
			}

			if (slave_answer != ges_run)
				ILI_INFO("ret = 0x%X, slave_answer = 0x%X\n", slave_answer, ges_run);

			mdelay(2);
		} while (slave_answer != ges_run && --retry > 0);
	}

	/* polling another specific register to see if gesutre is enabled properly */
	do {
		ret = ili_ice_mode_read_by_mode(ges_pwd_addr, &answer, pwd_len, MASTER);

		if (ret < 0) {
			ILI_ERR("Read Master gesture answer error\n");
			goto out;
		}

		if (answer != ges_run)
			ILI_INFO("ret = 0x%X, slave_answer = 0x%X\n", answer, ges_run);

		mdelay(2);
	} while (answer != ges_run && --retry > 0);

	if (retry <= 0) {
		ILI_ERR("Enter gesture failed\n");
		ret = -1;
		goto out;
	}

	ILI_INFO("0x%X Enter gesture successfully\n", answer);

out:
	if (ilits->cascade_info_block.nNum != 0) {
		ili_ice_mode_ctrl_by_mode(DISABLE, ON, BOTH);
	} else {
		if (ili_ice_mode_ctrl(DISABLE, ON) < 0) {
			ILI_ERR("Disable ice mode failed during gesture recovery\n");
		}
	}

	if (ret >= 0) {
		ilits->actual_tp_mode = P5_X_FW_GESTURE_MODE;
		ili_set_tp_data_len(ilits->gesture_mode, false, &data_type);
	}

	if (ilits->prox_face_mode == PROXIMITY_SUSPEND_RESUME && ilits->proxmity_face == true) {
		ret = ili_ic_func_ctrl("proximity", 0x01);
		ret = ili_ic_func_ctrl("lpwg", 0x21);
		if (ret < 0) {
			ILI_ERR("Write func ctrl cmd failed\n");
		}
	}

	return ret;
}

int ili_touch_esd_gesture_iram(void)
{
	int ret = 0, retry = 100;
	u32 answer = 0;
	int ges_pwd_addr = SPI_ESD_GESTURE_CORE146_PWD_ADDR;
	int ges_pwd = ESD_GESTURE_CORE146_PWD;
	int ges_run = SPI_ESD_GESTURE_CORE146_RUN;
	int pwd_len = 2;
	u8 data_type = P5_X_FW_SIGNAL_DATA_MODE;
	u8 backup_rec_state = 0;

	if (!ilits->gesture_load_code) {
		ges_run = I2C_ESD_GESTURE_CORE146_RUN;
	}

	if (ilits->chip->core_ver < CORE_VER_1460) {
		if (ilits->chip->core_ver >= CORE_VER_1420)
			ges_pwd_addr = I2C_ESD_GESTURE_PWD_ADDR;
		else
			ges_pwd_addr = SPI_ESD_GESTURE_PWD_ADDR;
		ges_pwd = ESD_GESTURE_PWD;
		ges_run = SPI_ESD_GESTURE_RUN;
		pwd_len = 4;
	}

	ILI_INFO("ESD Gesture PWD Addr = 0x%X, PWD = 0x%X, GES_RUN = 0x%X, core_ver = 0x%X\n",
		ges_pwd_addr, ges_pwd, ges_run, ilits->chip->core_ver);

	ilits->ice_mode_ctrl(ENABLE, OFF, BOTH);

	if (ret < 0) {
		ILI_ERR("Enable ice mode failed during gesture recovery\n");
		goto fail;
	}

	/* write a special password to inform FW go back into gesture mode */
	ret = ili_ice_mode_write(ges_pwd_addr, ges_pwd, pwd_len);
	if (ret < 0) {
		ILI_ERR("write password failed\n");
		goto fail;
	}

	if (ilits->prox_face_mode == PROXIMITY_SUSPEND_RESUME) {
		backup_rec_state = func_ctrl[FUNC_CTRL_PROXIMITY_IDX].rec_state;
		func_ctrl[FUNC_CTRL_PROXIMITY_IDX].rec_state = DISABLE;
		ILI_INFO("Doing gesture dynamic load code, Cannot reset %s CMD, rec_state %d \n", func_ctrl[FUNC_CTRL_PROXIMITY_IDX].name, func_ctrl[FUNC_CTRL_PROXIMITY_IDX].rec_state);
	}

	/* Host download gives effect to FW receives password successed */
	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	ret = ili_fw_upgrade_handler(NULL);
	if (ret < 0) {
		ILI_ERR("FW upgrade failed during gesture recovery\n");
		goto fail;
	}

	/* Wait for fw running code finished. */
	if (ilits->info_from_hex || (ilits->chip->core_ver >= CORE_VER_1410))
		msleep(50);

	ret = ilits->ice_mode_ctrl(ENABLE, ON, BOTH);

	if (ret < 0) {
		ILI_ERR("Enable ice mode failed during gesture recovery\n");
		goto fail;
	}

	if (ilits->cascade_info_block.nNum != 0) {
		/* polling Cascade Slave another specific register to see if gesutre is enabled properly */
		do {
			ili_spi_ice_mode_read(ges_pwd_addr, &answer, sizeof(u32), SLAVE);

			if (answer != ges_run)
				ILI_INFO("ret = 0x%X, slave answer = 0x%X\n", answer, ges_run);

			mdelay(2);
		} while (answer != ges_run && --retry > 0);
	}

	answer = 0;
	/* polling another specific register to see if gesutre is enabled properly */
	do {
		ili_spi_ice_mode_read(ges_pwd_addr, &answer, sizeof(u32), MASTER);

		if (answer != ges_run)
			ILI_INFO("ret = 0x%X, answer = 0x%X\n", answer, ges_run);

		mdelay(2);
	} while (answer != ges_run && --retry > 0);

	if (retry <= 0) {
		ILI_ERR("Enter gesture failed\n");
		ret = -1;
		goto fail;
	}

	ILI_INFO("Enter gesture successfully\n");

	ret = ili_ice_mode_ctrl(DISABLE, ON);
	if (ret < 0) {
		ILI_ERR("Disable ice mode failed during gesture recovery\n");
		goto fail;
	}

	ILI_INFO("Gesture code loaded by %s\n", ilits->gesture_load_code ? "driver" : "firmware");

	if (!ilits->gesture_load_code) {
		ilits->actual_tp_mode = P5_X_FW_GESTURE_MODE;
		ili_set_tp_data_len(ilits->gesture_mode, false, &data_type);
		goto out;
	}

	/* Load gesture code */
	ilits->actual_tp_mode = P5_X_FW_GESTURE_MODE;
	ili_set_tp_data_len(ilits->gesture_mode, false, &data_type);
	ret = ili_fw_upgrade_handler(NULL);
	if (ret < 0) {
		ILI_ERR("Failed to load code during gesture recovery\n");
		goto fail;
	}

	if (ilits->prox_face_mode == PROXIMITY_SUSPEND_RESUME) {
		func_ctrl[FUNC_CTRL_PROXIMITY_IDX].rec_state = backup_rec_state;
		ILI_INFO("Recovery %s rec_state %d \n", func_ctrl[FUNC_CTRL_PROXIMITY_IDX].name, func_ctrl[FUNC_CTRL_PROXIMITY_IDX].rec_state);

		/* Resume gesture loader */
		ret = ili_ic_func_ctrl("lpwg", 0x6);

		if (ilits->proxmity_face == true) {
			ret = ili_ic_func_ctrl("proximity", 0x01);
			ret = ili_ic_func_ctrl("lpwg", 0x21);
		}
	} else {
		/* Resume gesture loader */
		ret = ili_ic_func_ctrl("lpwg", 0x6);
	}

	if (ret < 0) {
		ILI_ERR("write resume loader error");
		goto fail;
	}

out:
	return ret;

fail:
	ilits->actual_tp_mode = P5_X_FW_GESTURE_MODE;
	ili_ice_mode_ctrl(DISABLE, ON);

	return ret;
}

void ili_demo_debug_info_id0(u8 *buf, size_t len)
{
	struct ili_demo_debug_info_id0 id0;

	ipio_memcpy(&id0, buf, sizeof(id0), len);
	ILI_INFO("id0 len = %d,strucy len = %d", (int)len, (int)sizeof(id0));

	ILI_INFO("id = %d\n", id0.id);
	ILI_INFO("app_sys_powr_state_e = %d\n", id0.sys_powr_state_e);
	ILI_INFO("app_sys_state_e = %d\n", id0.sys_state_e);
	ILI_INFO("tp_state_e = %d\n", id0.tp_state_e);

	ILI_INFO("touch_palm_state_e = %d\n", id0.touch_palm_state);
	ILI_INFO("app_an_statu_e = %d\n", id0.app_an_statu_e);
	ILI_INFO("app_sys_bg_err = %d\n", id0.app_sys_bg_err);
	ILI_INFO("g_b_wrong_bg = %d\n", id0.g_b_wrong_bg);
	ILI_INFO("reserved0 = %d\n", id0.reserved0);

	ILI_INFO("normal_mode = %d\n", id0.normal_mode);
	ILI_INFO("charger_mode = %d\n", id0.charger_mode);
	ILI_INFO("glove_mode = %d\n", id0.glove_mode);
	ILI_INFO("stylus_mode = %d\n", id0.stylus_mode);
	ILI_INFO("multi_mode = %d\n", id0.multi_mode);
	ILI_INFO("noise_mode = %d\n", id0.noise_mode);
	ILI_INFO("palm_plus_mode = %d\n", id0.palm_plus_mode);
	ILI_INFO("floating_mode = %d\n", id0.floating_mode);

	ILI_INFO("algo_pt_status0 = %d\n", id0.algo_pt_status0);
	ILI_INFO("algo_pt_status1 = %d\n", id0.algo_pt_status1);
	ILI_INFO("algo_pt_status2 = %d\n", id0.algo_pt_status2);
	ILI_INFO("algo_pt_status3 = %d\n", id0.algo_pt_status3);
	ILI_INFO("algo_pt_status4 = %d\n", id0.algo_pt_status4);
	ILI_INFO("algo_pt_status5 = %d\n", id0.algo_pt_status5);
	ILI_INFO("algo_pt_status6 = %d\n", id0.algo_pt_status6);
	ILI_INFO("algo_pt_status7 = %d\n", id0.algo_pt_status7);
	ILI_INFO("algo_pt_status8 = %d\n", id0.algo_pt_status8);
	ILI_INFO("algo_pt_status9 = %d\n", id0.algo_pt_status9);
	ILI_INFO("reserved2 = %d\n", id0.reserved2);

	ILI_INFO("hopping_flg = %d\n", id0.hopping_flg);
	ILI_INFO("hopping_index = %d\n", id0.hopping_index);
	ILI_INFO("frequency = %d\n", (id0.frequency_h << 8 | id0.frequency_l));
	ILI_INFO("reserved3 = %d\n", id0.reserved3);
	ILI_INFO("reserved4 = %d\n", id0.reserved4);

}

void ili_demo_debug_info_mode(u8 *buf, size_t len)
{
	u8 *info_ptr;
	u8 info_id, info_len;

	ili_report_ap_mode(buf, len);
	if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
		info_ptr = buf + P5_X_DEMO_MODE_PACKET_LEN;
	} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
		info_ptr = buf + P5_X_DEMO_MODE_PACKET_LEN_HIGH_RESOLUTION;
	} else {
		info_ptr = buf + P5_X_DEMO_MODE_PACKET_LEN;
	}
	info_len = info_ptr[0];
	info_id = info_ptr[1];

	ILI_INFO("info len = %d ,id = %d\n", info_len, info_id);
	if (info_id == 0) {
		ilits->demo_debug_info[info_id](&info_ptr[1], info_len);
	} else {
		ILI_INFO("Not support this id %d\n", info_id);
	}
}

static void ilitek_tddi_touch_send_debug_data(u8 *buf, int len)
{
	int index;
	mutex_lock(&ilits->debug_mutex);

	if (!ilits->dnp && !ilits->dlnp)
		goto out;

	/* Sending data to apk via the node of debug_message node */
	if (ilits->dnp) {
		index = ilits->dbf;

		if (!ilits->dbl[ilits->dbf].mark) {
			ilits->dbf = ((ilits->dbf + 1) % TR_BUF_LIST_SIZE);
		} else {
			/*keep the last frame buffer*/
			if (ilits->dbf == 0)
				index = TR_BUF_LIST_SIZE - 1;
			else
				index = ilits->dbf -1;

			ilits->all_frame_full = true;
			ILI_DBG("Frame buf is full. \n");
		}

		if (ilits->dbl[index].data == NULL) {
			ILI_ERR("BUFFER %d error\n", index);
			goto out;
		}
		ipio_memcpy(ilits->dbl[index].data, buf, len, TR_BUF_SIZE);
		ilits->dbl[index].mark = true;

		wake_up(&(ilits->inq));
		goto out;
	}

	if (ilits->dlnp) {
		/* ping pong buffer */
		index = ilits->dbf_in_idx;
		ILI_DBG("ping pong buffer index = %d, mark = %d \n", index, ilits->dbf_in[index].mark);
		if (ilits->dbf_in[index].data == NULL) {
			ILI_ERR("INPUT BUFFER %d error\n", index);
			goto out;
		}

		if (ilits->dbf_in_idx < TR_BUF_LIST_SIZE) {
			ipio_memcpy(ilits->dbf_in[index].data, buf, len, TR_BUF_SIZE);
			ilits->dbf_in_idx++;
			ilits->dbf_in[index].mark = true;
		} else {
			ILI_DBG("input buffer is fully.\n");
		}

		wake_up(&(ilits->inq));
		goto out;
	}

out:
	mutex_unlock(&ilits->debug_mutex);
}

static void ilitek_tddi_touch_customer_data_parsing(u8 *buf)
{
	int i = 0;
	const int others = 16;
	int index = 0;
	struct ilitek_axis_info axis_info[MAX_TOUCH_NUM];
	struct  ilitek_als_info als_info;
	bool palmflag = false;
	ilits->finger = 0;
	if (ilits->rib.nCustomerType == POSITION_CUSTOMER_TYPE_AXIS && ilits->tp_data_format == DATA_FORMAT_DEMO) {
		if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			index = P5_X_5B_LOW_RESOLUTION_LENGTH - others - P5_X_INFO_CHECKSUM_LENGTH;
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				if ((buf[(4 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(4 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(4 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF)) {
					continue;
				}
				axis_info[ilits->finger].degree = buf[(5 * i) + index];
				axis_info[ilits->finger].width_major = (((buf[(5 * i) + 1 + index]) << 8) | (buf[(5 * i) + 2 + index]));
				axis_info[ilits->finger].width_minor = (((buf[(5 * i) + 3 + index]) << 8) | (buf[(5 * i) + 4 + index]));
				ILI_DBG("finger = %d, degree = %d, width_major = %d, width_minor = %d\n", ilits->finger, axis_info[ilits->finger].degree, axis_info[ilits->finger].width_major, axis_info[ilits->finger].width_minor);
				ilits->finger++;
			}
			palmflag = ((buf[index + P5_X_CUSTOMER_AXIS_LENGTH + 2] & 0x08) >> 3);
			ILI_DBG("palmflag = %d\n", palmflag);

			for (i = 0; i < others; i++) {
				ILI_DBG("Other Information byte[%d]:%02x\n", i, buf[index + P5_X_CUSTOMER_AXIS_LENGTH + i]);
			}
		} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
			index = P5_X_DEMO_MODE_PACKET_LEN_HIGH_RESOLUTION - others - P5_X_INFO_CHECKSUM_LENGTH;
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				if ((buf[(5 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(5 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) &&
				    (buf[(5 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(5 * i) + 4 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF)) {
				continue;
			}
				axis_info[ilits->finger].degree = buf[(5 * i) + index];
				axis_info[ilits->finger].width_major = (((buf[(5 * i) + 1 + index]) << 8) | (buf[(5 * i) + 2 + index]));
				axis_info[ilits->finger].width_minor = (((buf[(5 * i) + 3 + index]) << 8) | (buf[(5 * i) + 4 + index]));
				ILI_DBG("finger = %d, degree = %d, width_major = %d, width_minor = %d\n", ilits->finger, axis_info[ilits->finger].degree, axis_info[ilits->finger].width_major, axis_info[ilits->finger].width_minor);
				ilits->finger++;
			}
			palmflag = ((buf[index + P5_X_CUSTOMER_AXIS_LENGTH + 2] & 0x08) >> 3);
			ILI_DBG("palmflag = %d\n", palmflag);

			for (i = 0; i < others; i++) {
				ILI_DBG("Other Information byte[%d]:%02x\n", i, buf[index + P5_X_CUSTOMER_AXIS_LENGTH + i]);
			}
		}
	} else if (ilits->rib.nCustomerType == POSITION_CUSTOMER_TYPE_AXIS && ilits->tp_data_format == DATA_FORMAT_DEBUG) {
		if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			index = P5_X_DEBUG_LOW_RESOLUTION_FINGER_DATA_LENGTH + (2 * ilits->xch_num * ilits->ych_num) + (ilits->stx * 2) + (ilits->srx * 2) + (2 * 2);
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				if ((buf[(3 * i) + 1 + P5_X_DEBUG_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(3 * i) + 2 + P5_X_DEBUG_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(3 * i) + 3 + P5_X_DEBUG_MODE_PACKET_INFO_LEN] == 0xFF)) {
					continue;
				}
				axis_info[ilits->finger].degree = buf[(5 * i) + index];
				axis_info[ilits->finger].width_major = (((buf[(5 * i) + 1 + index]) << 8) | (buf[(5 * i) + 2 + index]));
				axis_info[ilits->finger].width_minor = (((buf[(5 * i) + 3 + index]) << 8) | (buf[(5 * i) + 4 + index]));
				ILI_DBG("finger = %d, degree = %d, width_major = %d, width_minor = %d\n", ilits->finger, axis_info[ilits->finger].degree, axis_info[ilits->finger].width_major, axis_info[ilits->finger].width_minor);
				ilits->finger++;
			}
			palmflag = ((buf[index + P5_X_CUSTOMER_AXIS_LENGTH + 2] & 0x08) >> 3);
			ILI_DBG("palmflag = %d\n", palmflag);

			for (i = 0; i < others; i++) {
				ILI_DBG("Other Information byte[%d]:%02x\n", i, buf[index + P5_X_CUSTOMER_AXIS_LENGTH + i]);
			}
		} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
			index = P5_X_DEBUG_HIGH_RESOLUTION_FINGER_DATA_LENGTH + (2 * ilits->xch_num * ilits->ych_num) + (ilits->stx * 2) + (ilits->srx * 2) + (2 * 2);
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				if ((buf[(4 * i) + 1 + P5_X_DEBUG_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(4 * i) + 2 + P5_X_DEBUG_MODE_PACKET_INFO_LEN] == 0xFF) &&
				    (buf[(4 * i) + 3 + P5_X_DEBUG_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(4 * i) + 4 + P5_X_DEBUG_MODE_PACKET_INFO_LEN] == 0xFF)) {
				continue;
			}
				axis_info[ilits->finger].degree = buf[(5 * i) + index];
				axis_info[ilits->finger].width_major = (((buf[(5 * i) + 1 + index]) << 8) | (buf[(5 * i) + 2 + index]));
				axis_info[ilits->finger].width_minor = (((buf[(5 * i) + 3 + index]) << 8) | (buf[(5 * i) + 4 + index]));
				ILI_DBG("finger = %d, degree = %d, width_major = %d, width_minor = %d\n", ilits->finger, axis_info[ilits->finger].degree, axis_info[ilits->finger].width_major, axis_info[ilits->finger].width_minor);
				ilits->finger++;
			}
			palmflag = ((buf[index + P5_X_CUSTOMER_AXIS_LENGTH + 2] & 0x08) >> 3);
			ILI_DBG("palmflag = %d\n", palmflag);

			for (i = 0; i < others; i++) {
				ILI_DBG("Other Information byte[%d]:%02x\n", i, buf[index + P5_X_CUSTOMER_AXIS_LENGTH + i]);
			}
		}
	} else if (ilits->rib.nCustomerType == POSITION_CUSTOMER_TYPE_ALS && ilits->tp_data_format == DATA_FORMAT_DEMO) {
		if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			index = P5_X_5B_LOW_RESOLUTION_LENGTH - others - P5_X_INFO_CHECKSUM_LENGTH;

			if (((buf[index] << 24) | (buf[1 + index] << 16) | (buf[2 + index] << 8) | buf[3 + index]) != 0xFFFFFFFF &&
				((buf[4 + index] << 8) | buf[5 + index]) != 0xFFFF &&
				((buf[6 + index] << 8) | buf[7 + index]) != 0xFFFF &&
				((buf[8 + index] << 8) | buf[9 + index]) != 0xFFFF ) {

				als_info.illumination = ((buf[index] << 24) | (buf[1 + index] << 16) | (buf[2 + index] << 8) | buf[3 + index]);
				als_info.XR = (buf[4 + index] << 8) | buf[5 + index];
				als_info.XG = (buf[6 + index] << 8) | buf[7 + index];
				als_info.XB = (buf[8 + index] << 8) | buf[9 + index];
				ILI_DBG("illumination = %d.%d, XR = %d, XG = %d, XB = %d\n", als_info.illumination / 10, als_info.illumination % 10, als_info.XR, als_info.XG, als_info.XB);
			}

			for (i = 0; i < others; i++) {
				ILI_DBG("Other Information byte[%d]:%02x\n", i, buf[index + P5_X_CUSTOMER_ALS_LENGTH + i]);
			}
		} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
			index = P5_X_DEMO_MODE_PACKET_LEN_HIGH_RESOLUTION - others - P5_X_INFO_CHECKSUM_LENGTH;

			if (((buf[index] << 24) | (buf[1 + index] << 16) | (buf[2 + index] << 8) | buf[3 + index]) != 0xFFFFFFFF &&
				((buf[4 + index] << 8) | buf[5 + index]) != 0xFFFF &&
				((buf[6 + index] << 8) | buf[7 + index]) != 0xFFFF &&
				((buf[8 + index] << 8) | buf[9 + index]) != 0xFFFF ) {

				als_info.illumination = ((buf[index] << 24) | (buf[1 + index] << 16) | (buf[2 + index] << 8) | buf[3 + index]);
				als_info.XR = (buf[4 + index] << 8) | buf[5 + index];
				als_info.XG = (buf[6 + index] << 8) | buf[7 + index];
				als_info.XB = (buf[8 + index] << 8) | buf[9 + index];
				ILI_DBG("illumination = %d.%d, XR = %d, XG = %d, XB = %d\n", als_info.illumination / 10, als_info.illumination % 10, als_info.XR, als_info.XG, als_info.XB);
			}

			for (i = 0; i < others; i++) {
				ILI_DBG("Other Information byte[%d]:%02x\n", i, buf[index + P5_X_CUSTOMER_ALS_LENGTH + i]);
			}
		}
	} else if (ilits->rib.nCustomerType == POSITION_CUSTOMER_TYPE_ALS && ilits->tp_data_format == DATA_FORMAT_DEBUG) {
		if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			index = P5_X_DEBUG_LOW_RESOLUTION_FINGER_DATA_LENGTH + (2 * ilits->xch_num * ilits->ych_num) + (ilits->stx * 2) + (ilits->srx * 2) + (2 * 2);

			if (((buf[index] << 24) | (buf[1 + index] << 16) | (buf[2 + index] << 8) | buf[3 + index]) != 0xFFFFFFFF &&
				((buf[4 + index] << 8) | buf[5 + index]) != 0xFFFF &&
				((buf[6 + index] << 8) | buf[7 + index]) != 0xFFFF &&
				((buf[8 + index] << 8) | buf[9 + index]) != 0xFFFF ) {

				als_info.illumination = ((buf[index] << 24) | (buf[1 + index] << 16) | (buf[2 + index] << 8) | buf[3 + index]);
				als_info.XR = (buf[4 + index] << 8) | buf[5 + index];
				als_info.XG = (buf[6 + index] << 8) | buf[7 + index];
				als_info.XB = (buf[8 + index] << 8) | buf[9 + index];
				ILI_DBG("illumination = %d.%d, XR = %d, XG = %d, XB = %d\n", als_info.illumination / 10, als_info.illumination % 10, als_info.XR, als_info.XG, als_info.XB);
			}

			for (i = 0; i < others; i++) {
				ILI_DBG("Other Information byte[%d]:%02x\n", i, buf[index + P5_X_CUSTOMER_ALS_LENGTH + i]);
			}
		} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
			index = P5_X_DEBUG_HIGH_RESOLUTION_FINGER_DATA_LENGTH + (2 * ilits->xch_num * ilits->ych_num) + (ilits->stx * 2) + (ilits->srx * 2) + (2 * 2);

			if (((buf[index] << 24) | (buf[1 + index] << 16) | (buf[2 + index] << 8) | buf[3 + index]) != 0xFFFFFFFF &&
				((buf[4 + index] << 8) | buf[5 + index]) != 0xFFFF &&
				((buf[6 + index] << 8) | buf[7 + index]) != 0xFFFF &&
				((buf[8 + index] << 8) | buf[9 + index]) != 0xFFFF ) {

				als_info.illumination = ((buf[index] << 24) | (buf[1 + index] << 16) | (buf[2 + index] << 8) | buf[3 + index]);
				als_info.XR = (buf[4 + index] << 8) | buf[5 + index];
				als_info.XG = (buf[6 + index] << 8) | buf[7 + index];
				als_info.XB = (buf[8 + index] << 8) | buf[9 + index];
				ILI_DBG("illumination = %d.%d, XR = %d, XG = %d, XB = %d\n", als_info.illumination / 10, als_info.illumination % 10, als_info.XR, als_info.XG, als_info.XB);
			}

			for (i = 0; i < others; i++) {
				ILI_DBG("Other Information byte[%d]:%02x\n", i, buf[index + P5_X_CUSTOMER_ALS_LENGTH + i]);
			}
		}
	}
}

void ili_touch_press(u16 x, u16 y, u16 pressure, u16 id)
{
	ILI_DBG("Touch Press: id = %d, x = %d, y = %d, p = %d\n", id, x, y, pressure);

	if (MT_B_TYPE) {
		input_mt_slot(ilits->input, id);
		input_mt_report_slot_state(ilits->input, MT_TOOL_FINGER, true);
		input_report_abs(ilits->input, ABS_MT_POSITION_X, x);
		input_report_abs(ilits->input, ABS_MT_POSITION_Y, y);
		if (MT_PRESSURE)
			input_report_abs(ilits->input, ABS_MT_PRESSURE, pressure);
	} else {
		input_report_key(ilits->input, BTN_TOUCH, 1);
		input_report_abs(ilits->input, ABS_MT_TRACKING_ID, id);
		input_report_abs(ilits->input, ABS_MT_TOUCH_MAJOR, 1);
		input_report_abs(ilits->input, ABS_MT_WIDTH_MAJOR, 1);
		input_report_abs(ilits->input, ABS_MT_POSITION_X, x);
		input_report_abs(ilits->input, ABS_MT_POSITION_Y, y);
		if (MT_PRESSURE)
			input_report_abs(ilits->input, ABS_MT_PRESSURE, pressure);

		input_mt_sync(ilits->input);
	}
}

void ili_touch_release(u16 x, u16 y, u16 id)
{
	ILI_DBG("Touch Release: id = %d, x = %d, y = %d\n", id, x, y);

	if (MT_B_TYPE) {
		input_mt_slot(ilits->input, id);
		input_mt_report_slot_state(ilits->input, MT_TOOL_FINGER, false);
		if (MT_PRESSURE)
			input_report_abs(ilits->input, ABS_MT_PRESSURE, 0);
	} else {
		input_report_key(ilits->input, BTN_TOUCH, 0);
		input_mt_sync(ilits->input);
	}
}

void ili_touch_release_all_point(void)
{
	int i;
	ILI_DBG("Touch Release All\n");
	if (MT_B_TYPE) {
		for (i = 0 ; i < MAX_TOUCH_NUM; i++)
			ili_touch_release(0, 0, i);

		input_report_key(ilits->input, BTN_TOUCH, 0);
		input_report_key(ilits->input, BTN_TOOL_FINGER, 0);
	} else {
		ili_touch_release(0, 0, 0);
	}
	input_sync(ilits->input);
}

static struct ilitek_touch_info touch_info[MAX_TOUCH_NUM + MAX_PEN_NUM];

void ili_report_ap_mode(u8 *buf, int len)
{
	int i = 0;
	u32 xop = 0, yop = 0;

	memset(touch_info, 0x0, sizeof(touch_info));

	ilits->finger = 0;

	for (i = 0; i < MAX_TOUCH_NUM; i++) {

		if (ilits->rib.nCustomerType == ilits->customertype_off && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			if ((buf[(4 * i) + 1] == 0xFF) && (buf[(4 * i) + 2] == 0xFF) && (buf[(4 * i) + 3] == 0xFF)) {
				if (MT_B_TYPE)
					ilits->curt_touch[i] = 0;
				continue;
			}
			xop = (((buf[(4 * i) + 1] & 0xF0) << 4) | (buf[(4 * i) + 2]));
			yop = (((buf[(4 * i) + 1] & 0x0F) << 8) | (buf[(4 * i) + 3]));
		} else if (ilits->rib.nCustomerType != ilits->customertype_off && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			if ((buf[(4 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(4 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(4 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF)) {
				if (MT_B_TYPE)
					ilits->curt_touch[i] = 0;
				continue;
			}
			xop = (((buf[(4 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] & 0xF0) << 4) | (buf[(4 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN]));
			yop = (((buf[(4 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] & 0x0F) << 8) | (buf[(4 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN]));
		} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
			if ((buf[(5 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(5 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) &&
			    (buf[(5 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(5 * i) + 4 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF)) {
				if (MT_B_TYPE)
					ilits->curt_touch[i] = 0;
				continue;
			}
			xop = ((buf[(5 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] << 8) | (buf[(5 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN]));
			yop = ((buf[(5 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN] << 8) | (buf[(5 * i) + 4 + P5_X_DEMO_MODE_PACKET_INFO_LEN]));
		}

		if (ilits->trans_xy) {
			touch_info[ilits->finger].x = xop;
			touch_info[ilits->finger].y = yop;
		} else {
			if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
				touch_info[ilits->finger].x = xop * ilits->panel_wid / TPD_WIDTH;
				touch_info[ilits->finger].y = yop * ilits->panel_hei / TPD_HEIGHT;
			} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
				touch_info[ilits->finger].x = xop * ilits->panel_wid / ilits->max_x;
				touch_info[ilits->finger].y = yop * ilits->panel_hei / ilits->max_y;
			}
		}

		touch_info[ilits->finger].id = i;

		if (MT_PRESSURE) {
			if (ilits->rib.nCustomerType == ilits->customertype_off && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
				touch_info[ilits->finger].pressure = buf[(4 * i) + 4];
			} else if (ilits->rib.nCustomerType != ilits->customertype_off && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
				touch_info[ilits->finger].pressure = buf[(4 * i) + 4 + P5_X_DEMO_MODE_PACKET_INFO_LEN];
			} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
				touch_info[ilits->finger].pressure = buf[(5 * i) + 5 + P5_X_DEMO_MODE_PACKET_INFO_LEN];
			}
		} else {
			touch_info[ilits->finger].pressure = 1;
		}

		ILI_DBG("original x = %d, y = %d\n", xop, yop);
		ilits->finger++;
		if (MT_B_TYPE)
			ilits->curt_touch[i] = 1;
	}

#ifdef ROI
	ili_read_knuckle_roi_data();
#endif
	ILI_DBG("figner number = %d, LastTouch = %d\n", ilits->finger, ilits->last_touch);

	if (ilits->finger) {
		if (MT_B_TYPE) {
			for (i = 0; i < ilits->finger; i++) {
				input_report_key(ilits->input, BTN_TOUCH, 1);
				ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
				input_report_key(ilits->input, BTN_TOOL_FINGER, 1);
			}
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1)
					ili_touch_release(0, 0, i);
				ilits->prev_touch[i] = ilits->curt_touch[i];
			}
		} else {
			for (i = 0; i < ilits->finger; i++)
				ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
		}
		input_sync(ilits->input);
		ilits->last_touch = ilits->finger;
	} else {
		if (ilits->last_touch) {
			if (MT_B_TYPE) {
				for (i = 0; i < MAX_TOUCH_NUM; i++) {
					if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1)
						ili_touch_release(0, 0, i);
					ilits->prev_touch[i] = ilits->curt_touch[i];
				}
				input_report_key(ilits->input, BTN_TOUCH, 0);
				input_report_key(ilits->input, BTN_TOOL_FINGER, 0);
			} else {
				ili_touch_release(0, 0, 0);
			}
			input_sync(ilits->input);
			ilits->last_touch = 0;
		}
	}
	ilitek_tddi_touch_customer_data_parsing(buf);
	ilitek_tddi_touch_send_debug_data(buf, len);
}

static void ili_pen_down(u16 x, u16 y, u16 pressure)
{
	ILI_DBG("Pen Down: x = %d, y = %d, pressure = %d, TipSwitch = %d, InRange = %d\n", x, y, pressure, ilits->pen_info.TipSwitch, ilits->pen_info.InRange);
	input_report_abs(ilits->input_pen, ABS_X, x);
	input_report_abs(ilits->input_pen, ABS_Y, y);
	input_report_abs(ilits->input_pen, ABS_PRESSURE, pressure);
	input_report_key(ilits->input_pen, BTN_TOUCH, ilits->pen_info.TipSwitch);
	input_report_key(ilits->input_pen, BTN_TOOL_PEN, ilits->pen_info.InRange);
	input_report_key(ilits->input_pen, BTN_STYLUS, ilits->pen_info.BarelSwitch);
	input_report_key(ilits->input_pen, BTN_STYLUS2, ilits->pen_info.Eraser);
	input_report_abs(ilits->input_pen, ABS_TILT_X, ilits->pen_info.tilt_x);
	input_report_abs(ilits->input_pen, ABS_TILT_Y, ilits->pen_info.tilt_y);

	/* Hover Report Rate Test */
	if (ilits->pen_info.TipSwitch) {
		input_report_abs(ilits->input_pen, ABS_DISTANCE, 0);
	} else if ( ilits->pen_info.TipSwitch == 0 && ilits->pen_info.InRange == 1 ) {
		if (ilits->pen_info.distance_cnt == 1) {
			ilits->pen_info.distance_cnt = 2;
		} else {
			ilits->pen_info.distance_cnt = 1;
	}
		input_report_abs(ilits->input_pen, ABS_DISTANCE, ilits->pen_info.distance_cnt);
	}

	input_sync(ilits->input_pen);
}

void ili_pen_release(void)
{
#if ENABLE_PEN_MODE
	ILI_DBG("Pen Release \n");
	ilits->pen_info.active = false;
	input_report_key(ilits->input_pen, BTN_TOUCH, false);
	input_report_key(ilits->input_pen, BTN_TOOL_PEN, false);
	input_report_key(ilits->input_pen, BTN_STYLUS, false);
	input_report_key(ilits->input_pen, BTN_STYLUS2, false);
	input_report_abs(ilits->input_pen, ABS_DISTANCE, 1);
	input_sync(ilits->input_pen);
#endif
}

void ili_report_separating_finger_pen_ap_mode(u8 *buf, int len)
{
	int i = 0, pen_id_index = 0, FigType = 0;
	u32 xop = 0, yop = 0, PenStartIdx = P5_X_INFO_HEADER_LENGTH + 1;

	memset(touch_info, 0x0, sizeof(touch_info));

	ilits->finger = 0;
	ilits->pen = 0;

	if ((buf[len - P5_X_INFO_CHECKSUM_LENGTH - 2] & BIT(1)) != ONLY_REPORT_HAND) {
		ILI_DBG("Pen Switch = 0x%X\n", buf[PenStartIdx]);
		if ((buf[PenStartIdx] & TIPSWITCH_HOVER_BIT) == RELEASE_PEN) {
			ilits->pen_info.active = false;
		} else {
			ilits->pen_info.active = true;
		}
	} else {
		ilits->pen_info.active = false;
	}
	ILI_DBG("Pen is active : %s\n", ilits->pen_info.active ? "True" : "False");

	if(buf[0] == P5_X_DEMO_FINGER_PACKET_ID) {
		FigType = buf[3] & 0x7;
		for (i = 0; i < MAX_TOUCH_NUM; i++) {
			if (FigType == 3) {
				if ((buf[(6 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(6 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) &&
					(buf[(6 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(6 * i) + 4 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF)) {
					if (MT_B_TYPE)
						ilits->curt_touch[i] = 0;
					continue;
				}
				xop = ((buf[(6 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] << 8) | (buf[(6 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN]));
				yop = ((buf[(6 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN] << 8) | (buf[(6 * i) + 4 + P5_X_DEMO_MODE_PACKET_INFO_LEN]));
			} else if (FigType == 5) {
				if ((buf[(8 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(8 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) &&
					(buf[(8 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(8 * i) + 4 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF)) {
					if (MT_B_TYPE)
						ilits->curt_touch[i] = 0;
					continue;
				}
				xop = ((buf[(8 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] << 8) | (buf[(8 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN]));
				yop = ((buf[(8 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN] << 8) | (buf[(8 * i) + 4 + P5_X_DEMO_MODE_PACKET_INFO_LEN]));
			} else {
				continue;
			}

			if (ilits->trans_xy) {
				touch_info[ilits->finger].x = xop;
				touch_info[ilits->finger].y = yop;
			} else {
				/*POSITION_HIGH_RESOLUTION*/
				touch_info[ilits->finger].x = xop * ilits->panel_wid / ilits->max_x;
				touch_info[ilits->finger].y = yop * ilits->panel_hei / ilits->max_y;
			}
			touch_info[ilits->finger].id = i;

			if (MT_PRESSURE) {
				if (FigType == 3) {
				touch_info[ilits->finger].pressure = buf[(6 * i) + 5 + P5_X_DEMO_MODE_PACKET_INFO_LEN];
				} else if (FigType == 5) {
					touch_info[ilits->finger].pressure = buf[(8 * i) + 5 + P5_X_DEMO_MODE_PACKET_INFO_LEN];
				}
			} else {
				touch_info[ilits->finger].pressure = 1;
			}

			ilits->finger++;
			if (MT_B_TYPE)
				ilits->curt_touch[i] = 1;

			ILI_DBG("original x = %d, y = %d\n", xop, yop);
		}
	} else {
		if (ilits->pen_info.active) {
			xop = ((buf[PenStartIdx + 2] << 8) | (buf[PenStartIdx + 3]));
			yop = ((buf[PenStartIdx + 4] << 8) | (buf[PenStartIdx + 5]));

			ilits->pen_info.TipSwitch = buf[PenStartIdx] & BIT(0);
			ilits->pen_info.BarelSwitch = (buf[PenStartIdx] & BIT(1)) >> 1;
			ilits->pen_info.Eraser = (buf[PenStartIdx] & BIT(2)) >> 2;
			ilits->pen_info.InRange = (buf[PenStartIdx] & BIT(4)) >> 4;

			if ( buf[PenStartIdx + TOUCH_TILT_OFFSET] & NEGITIVE_FLAG ) {
				/* Transfer Tilt to Negitive */
				ilits->pen_info.tilt_x = (buf[PenStartIdx + TOUCH_TILT_OFFSET] << 8) | buf[PenStartIdx + TOUCH_TILT_OFFSET + 1] | 0xFFFF0000 ;
			} else {
				ilits->pen_info.tilt_x = (buf[PenStartIdx + TOUCH_TILT_OFFSET] << 8) | buf[PenStartIdx + TOUCH_TILT_OFFSET + 1];
			}

			if ( buf[PenStartIdx + TOUCH_TILT_OFFSET + 2] & NEGITIVE_FLAG ) {
				/* Transfer Tilt to Negitive */
				ilits->pen_info.tilt_y = (buf[PenStartIdx + TOUCH_TILT_OFFSET + 2] << 8) | buf[PenStartIdx + TOUCH_TILT_OFFSET + 3] | 0xFFFFFF00;
			} else {
				ilits->pen_info.tilt_y = (buf[PenStartIdx + TOUCH_TILT_OFFSET + 2] << 8) | buf[PenStartIdx + TOUCH_TILT_OFFSET + 3];
			}

			/* Pen ID */
			for (pen_id_index = 0; pen_id_index < P5_X_PEN_ID_LEN; pen_id_index++) {
				ilits->pen_info.pen_id[pen_id_index] = buf[PenStartIdx + PEN_ID_OFFSET + 2 + pen_id_index];
			}

			if (ilits->trans_xy) {
				touch_info[PEN_INDEX].x = xop;
				touch_info[PEN_INDEX].y = yop;
			} else {
				/*POSITION_HIGH_RESOLUTION*/
				touch_info[PEN_INDEX].x = xop * ilits->panel_wid / ilits->max_x;
				touch_info[PEN_INDEX].y = yop * ilits->panel_hei / ilits->max_y;
			}
			touch_info[PEN_INDEX].id = i;
			touch_info[PEN_INDEX].pressure = ( buf[PenStartIdx + TOUCH_PRESS_OFFSET + 2] << 8 ) + buf[PenStartIdx + TOUCH_PRESS_OFFSET + 3];

			ilits->pen++;
			ilits->curt_touch[PEN_INDEX] = 1;
			ilits->prev_touch[PEN_INDEX] = 1;
		} else {
			ilits->curt_touch[PEN_INDEX] = 0;
			ILI_DBG("Report Switch is Only Report Hand\n");
		}
	}

#ifdef ROI
	ili_read_knuckle_roi_data();
#endif
	ILI_DBG("figner number = %d, LastTouch = %d\n", ilits->finger, ilits->last_touch);

	if (ilits->pen) {
		if (ilits->pen_info.finger_touch) {
			ilits->pen_info.finger_touch = false;
			ili_touch_release_all_point();

			if (MT_PRESSURE)
				input_report_abs(ilits->input, ABS_MT_PRESSURE, 0);
		}
		/* Pen Down */
		ili_pen_down(touch_info[PEN_INDEX].x, touch_info[PEN_INDEX].y, touch_info[PEN_INDEX].pressure);
	} else {
		/* Pen Up */
		if (ilits->curt_touch[PEN_INDEX] == 0 && ilits->prev_touch[PEN_INDEX] == 1) {
			ili_pen_release();
		}
		ilits->prev_touch[PEN_INDEX] = ilits->curt_touch[PEN_INDEX];
	}

	if (ilits->pen == 0) {
		if (ilits->finger) {
			if (MT_B_TYPE) {
				/* Finger Touch */
				for (i = 0; i < ilits->finger; i++) {
					if (touch_info[i].id < MAX_TOUCH_NUM) {
						ilits->pen_info.finger_touch = true;
						input_report_key(ilits->input, BTN_TOUCH, 1);
						ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
						input_report_key(ilits->input, BTN_TOOL_FINGER, 1);
					}
				}
				/* Finger Release */
				for (i = 0; i < MAX_TOUCH_NUM; i++) {
					if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1) {
						ili_touch_release(0, 0, i);
					}
					ilits->prev_touch[i] = ilits->curt_touch[i];
				}
			} else {
				for (i = 0; i < ilits->finger; i++) {
					if (touch_info[i].id < MAX_TOUCH_NUM) {
						ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
					}
				}
			}
			input_sync(ilits->input);
			ilits->last_touch = ilits->finger;
		} else {
			if (ilits->last_touch) {
				if (MT_B_TYPE) {
					for (i = 0; i < MAX_TOUCH_NUM; i++) {
						if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1)
							ili_touch_release(0, 0, i);
						ilits->prev_touch[i] = ilits->curt_touch[i];
					}

					if (MT_PRESSURE)
						input_report_abs(ilits->input, ABS_MT_PRESSURE, 0);

					input_report_key(ilits->input, BTN_TOUCH, 0);
					input_report_key(ilits->input, BTN_TOOL_FINGER, 0);
				} else {
					ili_touch_release(0, 0, 0);
				}
				input_sync(ilits->input);
				ilits->last_touch = 0;
				ilits->pen_info.finger_touch = false;
			}
		}
	}

	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_pen_demo_mode_report_point(u8 *buf, int len)
{
	int i = 0, pen_id_index = 0;
	u32 xop = 0, yop = 0, PenStartIdx = 0;

	memset(touch_info, 0x0, sizeof(touch_info));

	ilits->finger = 0;
	ilits->pen = 0;

	if (ilits->rib.nCustomerType == ilits->customertype_off && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
		PenStartIdx = P5_X_DEMO_LOW_RESOLUTION_FINGER_DATA_LENGTH + P5_X_P_SENSOR_KEY_LENGTH;
	} else if (ilits->rib.nCustomerType == POSITION_CUSTOMER_TYPE_AXIS && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
		PenStartIdx = P5_X_DEMO_LOW_RESOLUTION_FINGER_DATA_LENGTH + P5_X_P_SENSOR_KEY_LENGTH + P5_X_CUSTOMER_AXIS_LENGTH;
	} else if (ilits->rib.nCustomerType == POSITION_CUSTOMER_TYPE_AXIS && ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
		PenStartIdx = P5_X_DEMO_HIGH_RESOLUTION_FINGER_DATA_LENGTH + P5_X_P_SENSOR_KEY_LENGTH + P5_X_CUSTOMER_AXIS_LENGTH;
	} else if (ilits->rib.nCustomerType == POSITION_CUSTOMER_TYPE_ALS && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
		PenStartIdx = P5_X_DEMO_LOW_RESOLUTION_FINGER_DATA_LENGTH + P5_X_P_SENSOR_KEY_LENGTH + P5_X_CUSTOMER_ALS_LENGTH;
	} else if (ilits->rib.nCustomerType == POSITION_CUSTOMER_TYPE_ALS && ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
		PenStartIdx = P5_X_DEMO_HIGH_RESOLUTION_FINGER_DATA_LENGTH + P5_X_P_SENSOR_KEY_LENGTH + P5_X_CUSTOMER_ALS_LENGTH;
	} else if (ilits->rib.nCustomerType == ilits->customertype_off && ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
		PenStartIdx = P5_X_DEMO_HIGH_RESOLUTION_FINGER_DATA_LENGTH + P5_X_P_SENSOR_KEY_LENGTH;
	}

	if (buf[len - P5_X_INFO_CHECKSUM_LENGTH - 1] != ONLY_REPORT_HAND) {
		ILI_DBG("Pen Switch = 0x%X\n", buf[PenStartIdx]);
		if ((buf[PenStartIdx] & TIPSWITCH_HOVER_BIT) == RELEASE_PEN) {
			ilits->pen_info.active = false;
		} else {
			ilits->pen_info.active = true;
		}
	} else {
		ilits->pen_info.active = false;
	}
	ILI_DBG("Pen is active : %s\n", ilits->pen_info.active ? "True" : "False");

	for (i = 0; i < ilits->touch_num; i++) {
		if (i >= MAX_TOUCH_NUM) {
			if (ilits->pen_info.active) {
				xop = ((buf[PenStartIdx + 2] << 8) | (buf[PenStartIdx + 3]));
				yop = ((buf[PenStartIdx + 4] << 8) | (buf[PenStartIdx + 5]));

				ilits->pen_info.TipSwitch = buf[PenStartIdx] & BIT(0);
				ilits->pen_info.BarelSwitch = (buf[PenStartIdx] & BIT(1)) >> 1;
				ilits->pen_info.Eraser = (buf[PenStartIdx] & BIT(2)) >> 2;
				ilits->pen_info.InRange = (buf[PenStartIdx] & BIT(4)) >> 4;

				if ( buf[PenStartIdx + TOUCH_TILT_OFFSET] & NEGITIVE_FLAG ) {
					/* Transfer Tilt to Negitive */
					ilits->pen_info.tilt_x = buf[PenStartIdx + TOUCH_TILT_OFFSET] | 0xFFFFFF00;
				} else {
					ilits->pen_info.tilt_x = buf[PenStartIdx + TOUCH_TILT_OFFSET];
				}

				if ( buf[PenStartIdx + TOUCH_TILT_OFFSET + 1] & NEGITIVE_FLAG ) {
					/* Transfer Tilt to Negitive */
					ilits->pen_info.tilt_y = buf[PenStartIdx + TOUCH_TILT_OFFSET + 1] | 0xFFFFFF00;
				} else {
					ilits->pen_info.tilt_y = buf[PenStartIdx + TOUCH_TILT_OFFSET + 1];
				}

				/* Pen ID */
				for (pen_id_index = 0; pen_id_index < P5_X_PEN_ID_LEN; pen_id_index++) {
					ilits->pen_info.pen_id[pen_id_index] = buf[PenStartIdx + PEN_ID_OFFSET + pen_id_index];
				}
			} else {
				ilits->curt_touch[PEN_INDEX] = 0;
				ILI_DBG("Report Switch is Only Report Hand\n");
				continue;
			}
		} else {
			if (buf[len - P5_X_INFO_CHECKSUM_LENGTH - 1] != ONLY_REPORT_PEN) {
				if (ilits->rib.nCustomerType == ilits->customertype_off && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
					if ((buf[(4 * i) + 1] == 0xFF) && (buf[(4 * i) + 2] == 0xFF) && (buf[(4 * i) + 3] == 0xFF)) {
						if (MT_B_TYPE)
							ilits->curt_touch[i] = 0;
						continue;
					}
					xop = (((buf[(4 * i) + 1] & 0xF0) << 4) | (buf[(4 * i) + 2]));
					yop = (((buf[(4 * i) + 1] & 0x0F) << 8) | (buf[(4 * i) + 3]));
				} else if (ilits->rib.nCustomerType != ilits->customertype_off && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
					if ((buf[(4 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(4 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(4 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF)) {
						if (MT_B_TYPE)
							ilits->curt_touch[i] = 0;
						continue;
					}
					xop = (((buf[(4 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] & 0xF0) << 4) | (buf[(4 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN]));
					yop = (((buf[(4 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] & 0x0F) << 8) | (buf[(4 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN]));
				} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
					if ((buf[(5 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(5 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) &&
						(buf[(5 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF) && (buf[(5 * i) + 4 + P5_X_DEMO_MODE_PACKET_INFO_LEN] == 0xFF)) {
						if (MT_B_TYPE)
							ilits->curt_touch[i] = 0;
						continue;
					}
					xop = ((buf[(5 * i) + 1 + P5_X_DEMO_MODE_PACKET_INFO_LEN] << 8) | (buf[(5 * i) + 2 + P5_X_DEMO_MODE_PACKET_INFO_LEN]));
					yop = ((buf[(5 * i) + 3 + P5_X_DEMO_MODE_PACKET_INFO_LEN] << 8) | (buf[(5 * i) + 4 + P5_X_DEMO_MODE_PACKET_INFO_LEN]));
				}
			} else {
				continue;
			}
		}

		if (ilits->pen_info.active == true) {
			if (ilits->trans_xy) {
				touch_info[PEN_INDEX].x = xop;
				touch_info[PEN_INDEX].y = yop;
			} else {
				if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
					touch_info[PEN_INDEX].x = xop * ilits->panel_wid / TPD_WIDTH;
					touch_info[PEN_INDEX].y = yop * ilits->panel_hei / TPD_HEIGHT;
				} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
					touch_info[PEN_INDEX].x = xop * ilits->panel_wid / ilits->max_x;
					touch_info[PEN_INDEX].y = yop * ilits->panel_hei / ilits->max_y;
				}
			}
			touch_info[PEN_INDEX].id = i;
			touch_info[PEN_INDEX].pressure = ( buf[PenStartIdx + TOUCH_PRESS_OFFSET] << 8 ) + buf[PenStartIdx + TOUCH_PRESS_OFFSET + 1];

			ilits->pen++;
			ilits->curt_touch[PEN_INDEX] = 1;
			ilits->prev_touch[PEN_INDEX] = 1;
		} else {
			if (ilits->trans_xy) {
				touch_info[ilits->finger].x = xop;
				touch_info[ilits->finger].y = yop;
			} else {
				if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
					touch_info[ilits->finger].x = xop * ilits->panel_wid / TPD_WIDTH;
					touch_info[ilits->finger].y = yop * ilits->panel_hei / TPD_HEIGHT;
				} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
					touch_info[ilits->finger].x = xop * ilits->panel_wid / ilits->max_x;
					touch_info[ilits->finger].y = yop * ilits->panel_hei / ilits->max_y;
				}
			}
			touch_info[ilits->finger].id = i;

			if (MT_PRESSURE) {
				if (ilits->rib.nCustomerType == ilits->customertype_off && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
					touch_info[ilits->finger].pressure = buf[(4 * i) + 4];
				} else if (ilits->rib.nCustomerType != ilits->customertype_off && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
					touch_info[ilits->finger].pressure = buf[(4 * i) + 4 + P5_X_DEMO_MODE_PACKET_INFO_LEN];
				} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
					touch_info[ilits->finger].pressure = buf[(5 * i) + 5 + P5_X_DEMO_MODE_PACKET_INFO_LEN];
				}
			} else {
				touch_info[ilits->finger].pressure = 1;
			}

			ilits->finger++;
			if (MT_B_TYPE)
				ilits->curt_touch[i] = 1;
		}

		ILI_DBG("original x = %d, y = %d\n", xop, yop);
	}

#ifdef ROI
	ili_read_knuckle_roi_data();
#endif
	ILI_DBG("figner number = %d, LastTouch = %d\n", ilits->finger, ilits->last_touch);

	if (ilits->pen) {
		if (ilits->pen_info.finger_touch) {
			ilits->pen_info.finger_touch = false;
			ili_touch_release_all_point();

			if (MT_PRESSURE)
				input_report_abs(ilits->input, ABS_MT_PRESSURE, 0);
		}
		/* Pen Down */
		ili_pen_down(touch_info[PEN_INDEX].x, touch_info[PEN_INDEX].y, touch_info[PEN_INDEX].pressure);
	} else {
		/* Pen Up */
		if (ilits->curt_touch[PEN_INDEX] == 0 && ilits->prev_touch[PEN_INDEX] == 1) {
			ili_pen_release();
		}
		ilits->prev_touch[PEN_INDEX] = ilits->curt_touch[PEN_INDEX];
	}

	if (ilits->pen == 0) {
		if (ilits->finger) {
			if (MT_B_TYPE) {
				/* Finger Touch */
				for (i = 0; i < ilits->finger; i++) {
					if (touch_info[i].id < MAX_TOUCH_NUM) {
						ilits->pen_info.finger_touch = true;
						input_report_key(ilits->input, BTN_TOUCH, 1);
						ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
						input_report_key(ilits->input, BTN_TOOL_FINGER, 1);
					}
				}
				/* Finger Release */
				for (i = 0; i < MAX_TOUCH_NUM; i++) {
					if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1) {
						ili_touch_release(0, 0, i);
					}
					ilits->prev_touch[i] = ilits->curt_touch[i];
				}
			} else {
				for (i = 0; i < ilits->finger; i++) {
					if (touch_info[i].id < MAX_TOUCH_NUM) {
						ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
					}
				}
			}
			input_sync(ilits->input);
			ilits->last_touch = ilits->finger;
		} else {
			if (ilits->last_touch) {
				if (MT_B_TYPE) {
					for (i = 0; i < MAX_TOUCH_NUM; i++) {
						if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1)
							ili_touch_release(0, 0, i);
						ilits->prev_touch[i] = ilits->curt_touch[i];
					}

					if (MT_PRESSURE)
						input_report_abs(ilits->input, ABS_MT_PRESSURE, 0);

					input_report_key(ilits->input, BTN_TOUCH, 0);
					input_report_key(ilits->input, BTN_TOOL_FINGER, 0);
				} else {
					ili_touch_release(0, 0, 0);
				}
				input_sync(ilits->input);
				ilits->last_touch = 0;
				ilits->pen_info.finger_touch = false;
			}
		}
	}
}

void ili_report_pen_ap_mode(u8 *buf, int len)
{
	ili_pen_demo_mode_report_point(buf, len);
	ilitek_tddi_touch_customer_data_parsing(buf);
	ilitek_tddi_touch_send_debug_data(buf, len);
}

/*Pen report single event, Finger report Mulit event*/
void ili_pen_debug_mode_report_point(u8 *buf, int len, u8 offset)
{
	int i = 0, pen_id_index = 0;
	u32 xop = 0, yop = 0, PenStartIdx = 0;
	static u8 p[MAX_TOUCH_NUM];

	memset(touch_info, 0x0, sizeof(touch_info));

	ilits->finger = 0;
	ilits->pen = 0;

	for (i = 0; i < ilits->touch_num; i++) {
		if (ilits->rib.nCustomerType == ilits->customertype_off && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			PenStartIdx = P5_X_DEBUG_LOW_RESOLUTION_FINGER_DATA_LENGTH + ilits->cdc_data_len;
		} else if (ilits->rib.nCustomerType == POSITION_CUSTOMER_TYPE_AXIS && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			PenStartIdx = P5_X_DEBUG_LOW_RESOLUTION_FINGER_DATA_LENGTH + ilits->cdc_data_len + P5_X_CUSTOMER_AXIS_LENGTH;
		} else if (ilits->rib.nCustomerType == POSITION_CUSTOMER_TYPE_AXIS && ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
			PenStartIdx = P5_X_DEBUG_HIGH_RESOLUTION_FINGER_DATA_LENGTH + ilits->cdc_data_len + P5_X_CUSTOMER_AXIS_LENGTH;
		} else if (ilits->rib.nCustomerType == POSITION_CUSTOMER_TYPE_ALS && ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			PenStartIdx = P5_X_DEBUG_LOW_RESOLUTION_FINGER_DATA_LENGTH + ilits->cdc_data_len + P5_X_CUSTOMER_ALS_LENGTH;
		} else if (ilits->rib.nCustomerType == POSITION_CUSTOMER_TYPE_ALS && ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
			PenStartIdx = P5_X_DEBUG_HIGH_RESOLUTION_FINGER_DATA_LENGTH + ilits->cdc_data_len + P5_X_CUSTOMER_ALS_LENGTH;
		} else if (ilits->rib.nCustomerType == ilits->customertype_off && ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
			PenStartIdx = P5_X_DEBUG_HIGH_RESOLUTION_FINGER_DATA_LENGTH + ilits->cdc_data_len;
		}

		/* cdc debug lite */
		if (ilits->tp_data_format == DATA_FORMAT_DEBUG_LITE_ROI || ilits->tp_data_format == DATA_FORMAT_DEBUG_LITE_WINDOW
			|| ilits->tp_data_format == DATA_FORMAT_DEBUG_LITE_AREA) {
			/* PenStartIdx = P5_X_DEBUG_LITE_LENGTH - Pendata - other len - checksum */
			PenStartIdx = 284;
		} else if (ilits->tp_data_format == DATA_FORMAT_DEBUG_LITE_PEN || ilits->tp_data_format == DATA_FORMAT_DEBUG_LITE_PEN_AREA) {
			/* PenStartIdx = P5_X_DEBUG_LITE_PEN_LENGTH - Pendata - PenCDCData - other len - checksum */
			PenStartIdx = 39;
			/* debug lite pen only support pen and 1 finger */
			if (i > 0 && i < MAX_TOUCH_NUM)
				continue;
		}

		PenStartIdx -= offset;

		if (buf[len - offset - P5_X_INFO_CHECKSUM_LENGTH - 1] != ONLY_REPORT_HAND) {
				ILI_DBG("Pen Switch = 0x%X\n", buf[PenStartIdx]);
				if ((buf[PenStartIdx] & TIPSWITCH_HOVER_BIT) == RELEASE_PEN) {
					ilits->pen_info.active = false;
				} else {
					ilits->pen_info.active = true;
				}
		} else {
			ilits->pen_info.active = false;
		}
		ILI_DBG("Pen is active : %s\n", ilits->pen_info.active ? "True" : "False");

		if (i >= MAX_TOUCH_NUM) {
			if (ilits->pen_info.active) {
				xop = ((buf[PenStartIdx + 2] << 8) | (buf[PenStartIdx + 3]));
				yop = ((buf[PenStartIdx + 4] << 8) | (buf[PenStartIdx + 5]));

				ilits->pen_info.TipSwitch = buf[PenStartIdx] & BIT(0);
				ilits->pen_info.BarelSwitch = (buf[PenStartIdx] & BIT(1)) >> 1;
				ilits->pen_info.Eraser = (buf[PenStartIdx] & BIT(2)) >> 2;
				ilits->pen_info.InRange = (buf[PenStartIdx] & BIT(4)) >> 4;

				if ( buf[PenStartIdx + TOUCH_TILT_OFFSET] & NEGITIVE_FLAG ) {
					/* Transfer Tilt to Negitive */
					ilits->pen_info.tilt_x = buf[PenStartIdx + TOUCH_TILT_OFFSET]  | 0xFFFFFF00;
				} else {
					ilits->pen_info.tilt_x = buf[PenStartIdx + TOUCH_TILT_OFFSET];
				}

				if ( buf[PenStartIdx + TOUCH_TILT_OFFSET + 1] & NEGITIVE_FLAG ) {
					/* Transfer Tilt to Negitive */
					ilits->pen_info.tilt_y = buf[PenStartIdx + TOUCH_TILT_OFFSET + 1] | 0xFFFFFF00;
				} else {
					ilits->pen_info.tilt_y = buf[PenStartIdx + TOUCH_TILT_OFFSET + 1];
				}

				if ( ilits->pen_info.TipSwitch ) {
					/* Pen ID */
					for (pen_id_index = 0; pen_id_index < P5_X_PEN_ID_LEN; pen_id_index++) {
						ilits->pen_info.pen_id[pen_id_index] = buf[PenStartIdx + PEN_ID_OFFSET + pen_id_index];
					}
					ILI_DBG("\n");
				}
			} else {
				ilits->curt_touch[PEN_INDEX] = 0;
				ILI_DBG("Report Switch is Only Report Hand\n");
				continue;
			}
		} else {
			if (buf[len - P5_X_INFO_CHECKSUM_LENGTH - 1] != ONLY_REPORT_PEN) {
				if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
					if ((buf[(3 * i)] == 0xFF) && (buf[(3 * i) + 1] == 0xFF) && (buf[(3 * i) + 2] == 0xFF)) {
						if (MT_B_TYPE)
							ilits->curt_touch[i] = 0;
						continue;
					}
					xop = (((buf[(3 * i)] & 0xF0) << 4) | (buf[(3 * i) + 1]));
					yop = (((buf[(3 * i)] & 0x0F) << 8) | (buf[(3 * i) + 2]));
				} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
					if ((buf[(4 * i)] == 0xFF) && (buf[(4 * i) + 1] == 0xFF) && (buf[(4 * i) + 2] == 0xFF) && (buf[(4 * i) + 3] == 0xFF)) {
						if (MT_B_TYPE)
							ilits->curt_touch[i] = 0;
						continue;
					}
					xop = ((buf[(4 * i) + 0] << 8) | (buf[(4 * i) + 1]));
					yop = ((buf[(4 * i) + 2] << 8) | (buf[(4 * i) + 3]));
				}
			} else {
				continue;
			}
		}

		if (ilits->pen_info.active == true) {
			if (ilits->trans_xy) {
				touch_info[PEN_INDEX].x = xop;
				touch_info[PEN_INDEX].y = yop;
			} else {
				if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
					touch_info[PEN_INDEX].x = xop * ilits->panel_wid / TPD_WIDTH;
					touch_info[PEN_INDEX].y = yop * ilits->panel_hei / TPD_HEIGHT;
				} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
					touch_info[PEN_INDEX].x = xop * ilits->panel_wid / ilits->max_x;
					touch_info[PEN_INDEX].y = yop * ilits->panel_hei / ilits->max_y;
				}
			}

			touch_info[PEN_INDEX].id = i;
			touch_info[PEN_INDEX].pressure = ( buf[PenStartIdx + TOUCH_PRESS_OFFSET] << 8 ) + buf[PenStartIdx + TOUCH_PRESS_OFFSET + 1];

			ilits->pen++;
			ilits->curt_touch[PEN_INDEX] = 1;
			ilits->prev_touch[PEN_INDEX] = 1;
		} else {
			if (ilits->trans_xy) {
				touch_info[ilits->finger].x = xop;
				touch_info[ilits->finger].y = yop;
			} else {
				if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
					touch_info[ilits->finger].x = xop * ilits->panel_wid / TPD_WIDTH;
					touch_info[ilits->finger].y = yop * ilits->panel_hei / TPD_HEIGHT;
				} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
					touch_info[ilits->finger].x = xop * ilits->panel_wid / ilits->max_x;
					touch_info[ilits->finger].y = yop * ilits->panel_hei / ilits->max_y;
				}
			}
			touch_info[ilits->finger].id = i;

			if (MT_PRESSURE) {
				/*
				* Since there's no pressure data in debug mode, we make fake values
				* for android system if pressure needs to be reported.
				*/
				if (p[ilits->finger] == 1)
					touch_info[ilits->finger].pressure = p[ilits->finger] = 2;
				else
					touch_info[ilits->finger].pressure = p[ilits->finger] = 1;
			} else {
				touch_info[ilits->finger].pressure = 1;
			}

			ilits->finger++;
			if (MT_B_TYPE)
				ilits->curt_touch[i] = 1;
		}

		ILI_DBG("original x = %d, y = %d\n", xop, yop);
	}

	ILI_DBG("figner number = %d, LastTouch = %d\n", ilits->finger, ilits->last_touch);

	if (ilits->pen) {
		if (ilits->pen_info.finger_touch) {
			ilits->pen_info.finger_touch = false;
			ili_touch_release_all_point();

			if (MT_PRESSURE)
				input_report_abs(ilits->input, ABS_MT_PRESSURE, 0);
		}
		/* Pen Down */
		ili_pen_down(touch_info[PEN_INDEX].x, touch_info[PEN_INDEX].y, touch_info[PEN_INDEX].pressure);
	} else {
		/* Pen Up */
		if (ilits->curt_touch[PEN_INDEX] == 0 && ilits->prev_touch[PEN_INDEX] == 1) {
			ili_pen_release();
		}
		ilits->prev_touch[PEN_INDEX] = ilits->curt_touch[PEN_INDEX];
	}

	if (ilits->pen == 0) {
		if (ilits->finger) {
			if (MT_B_TYPE) {
				/* Finger Touch */
				for (i = 0; i < ilits->finger; i++) {
					if (touch_info[i].id < MAX_TOUCH_NUM) {
						ilits->pen_info.finger_touch = true;
						input_report_key(ilits->input, BTN_TOUCH, 1);
						ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
						input_report_key(ilits->input, BTN_TOOL_FINGER, 1);
					}
				}
				/* Finger Release */
				for (i = 0; i < MAX_TOUCH_NUM; i++) {
					if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1) {
						ili_touch_release(0, 0, i);
					}
					ilits->prev_touch[i] = ilits->curt_touch[i];
				}
			} else {
				for (i = 0; i < ilits->finger; i++) {
					if (touch_info[i].id < MAX_TOUCH_NUM) {
						ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
					}
				}
			}
			input_sync(ilits->input);
			ilits->last_touch = ilits->finger;
		} else {
			if (ilits->last_touch) {
				if (MT_B_TYPE) {
					for (i = 0; i < MAX_TOUCH_NUM; i++) {
						if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1)
							ili_touch_release(0, 0, i);
						ilits->prev_touch[i] = ilits->curt_touch[i];
					}

					if (MT_PRESSURE)
						input_report_abs(ilits->input, ABS_MT_PRESSURE, 0);

					input_report_key(ilits->input, BTN_TOUCH, 0);
					input_report_key(ilits->input, BTN_TOOL_FINGER, 0);
				} else {
					ili_touch_release(0, 0, 0);
				}
				input_sync(ilits->input);
				ilits->last_touch = 0;
				ilits->pen_info.finger_touch = false;
			}
		}
	}
}

void ili_debug_mode_report_point(u8 *buf, int len)
{
	int i = 0;
	u32 xop = 0, yop = 0;
	static u8 p[MAX_TOUCH_NUM];

	memset(touch_info, 0x0, sizeof(touch_info));

	ilits->finger = 0;

	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			if ((buf[(3 * i)] == 0xFF) && (buf[(3 * i) + 1] == 0xFF) && (buf[(3 * i) + 2] == 0xFF)) {
				if (MT_B_TYPE)
					ilits->curt_touch[i] = 0;
				continue;
			}
			xop = (((buf[(3 * i)] & 0xF0) << 4) | (buf[(3 * i) + 1]));
			yop = (((buf[(3 * i)] & 0x0F) << 8) | (buf[(3 * i) + 2]));
		} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
			if ((buf[(4 * i)] == 0xFF) && (buf[(4 * i) + 1] == 0xFF) && (buf[(4 * i) + 2] == 0xFF) && (buf[(4 * i) + 3] == 0xFF)) {
				if (MT_B_TYPE)
					ilits->curt_touch[i] = 0;
				continue;
			}
			xop = ((buf[(4 * i) + 0] << 8) | (buf[(4 * i) + 1]));
			yop = ((buf[(4 * i) + 2] << 8) | (buf[(4 * i) + 3]));
		}

		if (ilits->trans_xy) {
			touch_info[ilits->finger].x = xop;
			touch_info[ilits->finger].y = yop;
		} else {
			if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
				touch_info[ilits->finger].x = xop * ilits->panel_wid / TPD_WIDTH;
				touch_info[ilits->finger].y = yop * ilits->panel_hei / TPD_HEIGHT;
			} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
				touch_info[ilits->finger].x = xop * ilits->panel_wid / ilits->max_x;
				touch_info[ilits->finger].y = yop * ilits->panel_hei / ilits->max_y;
			}
		}

		touch_info[ilits->finger].id = i;

		if (MT_PRESSURE) {
			/*
			* Since there's no pressure data in debug mode, we make fake values
			* for android system if pressure needs to be reported.
			*/
			if (p[ilits->finger] == 1)
				touch_info[ilits->finger].pressure = p[ilits->finger] = 2;
			else
				touch_info[ilits->finger].pressure = p[ilits->finger] = 1;
		} else {
			touch_info[ilits->finger].pressure = 1;
		}

		ILI_DBG("original x = %d, y = %d\n", xop, yop);
		ilits->finger++;
		if (MT_B_TYPE)
			ilits->curt_touch[i] = 1;
	}

	ILI_DBG("figner number = %d, LastTouch = %d\n", ilits->finger, ilits->last_touch);

	if (ilits->finger) {
		if (MT_B_TYPE) {
			for (i = 0; i < ilits->finger; i++) {
				input_report_key(ilits->input, BTN_TOUCH, 1);
				ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
				input_report_key(ilits->input, BTN_TOOL_FINGER, 1);
			}
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1)
					ili_touch_release(0, 0, i);
				ilits->prev_touch[i] = ilits->curt_touch[i];
			}
		} else {
			for (i = 0; i < ilits->finger; i++)
				ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
		}
		input_sync(ilits->input);
		ilits->last_touch = ilits->finger;
	} else {
		if (ilits->last_touch) {
			if (MT_B_TYPE) {
				for (i = 0; i < MAX_TOUCH_NUM; i++) {
					if (ilits->curt_touch[i] == 0 && ilits->prev_touch[i] == 1)
						ili_touch_release(0, 0, i);
					ilits->prev_touch[i] = ilits->curt_touch[i];
				}
				input_report_key(ilits->input, BTN_TOUCH, 0);
				input_report_key(ilits->input, BTN_TOOL_FINGER, 0);
			} else {
				ili_touch_release(0, 0, 0);
			}
			input_sync(ilits->input);
			ilits->last_touch = 0;
		}
	}
}

void ili_report_debug_mode(u8 *buf, int len)
{
	ili_debug_mode_report_point(buf + 5, len);

	ilitek_tddi_touch_customer_data_parsing(buf);

	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_report_pen_debug_mode(u8 *buf, int len)
{
	u8 offset = 5;
	ilits->touch_num = MAX_TOUCH_NUM + MAX_PEN_NUM;
	ILI_DBG("Pen Type = 0x%X, Max Touch Num = %d, Pen Data Mode = %d\n",ilits->pen_info.report_type, ilits->touch_num, ilits->pen_info.pen_data_mode);

	ili_pen_debug_mode_report_point(buf + offset, len, offset);

	if (ilits->pen_info.report_type != P5_X_ONLY_PEN_TYPE) {
		ilitek_tddi_touch_customer_data_parsing(buf);
	}

	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_report_debug_lite_mode(u8 *buf, int len)
{
	ili_debug_mode_report_point(buf + 4, len);

	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_report_proximity_mode(u8 *buf, int len)
{
	struct input_dev *input = ilits->input;
	u8 proxmity_face_status = PROXIMITY_NONE_STATE_0;

	if (!ilits->prox_face_mode) {
		ILI_ERR("proximity face mode Error\n");
		return;
	}

	ili_irq_disable();

	if (buf[1] == PROXIMITY_NEAR_STATE_1 || buf[1] == PROXIMITY_NEAR_STATE_2 || buf[1] == PROXIMITY_NEAR_STATE_3) {
		proxmity_face_status = PROXIMITY_NEAR_STATE;
		ILI_DBG("Set proximity State : Near, cmd : %X\n", buf[1]);
	} else if(buf[1] == PROXIMITY_FAR_STATE_5) {
		proxmity_face_status = PROXIMITY_FAR_STATE;
		ILI_DBG("Set proximity State : Far, cmd : %X\n", buf[1]);
	} else {
		proxmity_face_status = PROXIMITY_IGNORE_STATE;
		ILI_DBG("Set proximity State : Ignore, cmd : %X\n", buf[1]);
	}

	if ((ilits->actual_tp_mode == P5_X_FW_AP_MODE) && (proxmity_face_status == PROXIMITY_NEAR_STATE) && (ilits->power_status == true) && (ilits->proxmity_face == false)) {
		input_report_key(input, KEY_GESTURE_POWER, 1);
		input_sync(input);
		input_report_key(input, KEY_GESTURE_POWER, 0);
		input_sync(input);
		ilits->proxmity_face = true;
	} else if ((ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE) && (proxmity_face_status == PROXIMITY_FAR_STATE) && (ilits->power_status == false) && (ilits->proxmity_face == true)) {
		input_report_key(input, KEY_GESTURE_POWER, 1);
		input_sync(input);
		input_report_key(input, KEY_GESTURE_POWER, 0);
		input_sync(input);
		ilits->proxmity_face = false;
	}

	ILI_DBG("TP mode (0x%x)\n", ilits->actual_tp_mode);
	ILI_DBG("power_status %s\n", ilits->power_status ? "True" : "False");
	ILI_DBG("proxmity face status : %s, proxmity face : %s\n", (proxmity_face_status == PROXIMITY_NEAR_STATE) ? "Near" : (proxmity_face_status == PROXIMITY_FAR_STATE) ? "Far" : "Ignore", ilits->proxmity_face ? "True" : "False");

	ili_irq_enable();
}

void ili_report_gesture_mode(u8 *buf, int len)
{
	int lu_x = 0, lu_y = 0, rd_x = 0, rd_y = 0, score = 0;
	struct gesture_coordinate *gc = ilits->gcoord;
	struct input_dev *input = ilits->input;
	bool transfer = ilits->trans_xy;
	u8 ges[P5_X_GESTURE_INFO_LENGTH_HIGH_RESOLUTION] = {0};

	if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
		ipio_memcpy(ges, buf, len, P5_X_GESTURE_INFO_LENGTH);
	} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
		ipio_memcpy(ges, buf, len, P5_X_GESTURE_INFO_LENGTH_HIGH_RESOLUTION);
	}

	memset(gc, 0x0, sizeof(struct gesture_coordinate));

	gc->code = ges[1];

	/* Parsing gesture coordinate & Score*/
	if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
		score = ges[36];
		gc->pos_start.x = ((ges[4] & 0xF0) << 4) | ges[5];
		gc->pos_start.y = ((ges[4] & 0x0F) << 8) | ges[6];
		gc->pos_end.x   = ((ges[7] & 0xF0) << 4) | ges[8];
		gc->pos_end.y   = ((ges[7] & 0x0F) << 8) | ges[9];
		gc->pos_1st.x   = ((ges[16] & 0xF0) << 4) | ges[17];
		gc->pos_1st.y   = ((ges[16] & 0x0F) << 8) | ges[18];
		gc->pos_2nd.x   = ((ges[19] & 0xF0) << 4) | ges[20];
		gc->pos_2nd.y   = ((ges[19] & 0x0F) << 8) | ges[21];
		gc->pos_3rd.x   = ((ges[22] & 0xF0) << 4) | ges[23];
		gc->pos_3rd.y   = ((ges[22] & 0x0F) << 8) | ges[24];
		gc->pos_4th.x   = ((ges[25] & 0xF0) << 4) | ges[26];
		gc->pos_4th.y   = ((ges[25] & 0x0F) << 8) | ges[27];
	} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
		score = ges[46];
		gc->pos_start.x = (ges[4] << 8) | ges[5];
		gc->pos_start.y = (ges[6] << 8) | ges[7];
		gc->pos_end.x   = (ges[8] << 8) | ges[9];
		gc->pos_end.y   = (ges[10] << 8) | ges[11];
		gc->pos_1st.x   = (ges[20] << 8) | ges[21];
		gc->pos_1st.y   = (ges[22] << 8) | ges[23];
		gc->pos_2nd.x   = (ges[24] << 4) | ges[25];
		gc->pos_2nd.y   = (ges[26] << 8) | ges[27];
		gc->pos_3rd.x   = (ges[28] << 8) | ges[29];
		gc->pos_3rd.y   = (ges[30] << 8) | ges[31];
		gc->pos_4th.x   = (ges[32] << 8) | ges[33];
		gc->pos_4th.y   = (ges[34] << 8) | ges[35];
	}
	ILI_INFO("gesture code = 0x%x, score = %d\n", gc->code, score);

	switch (gc->code) {
	case GESTURE_SINGLECLICK:
		ILI_INFO("Single Click key event\n");
		input_report_key(input, KEY_GESTURE_POWER, 1);
		input_sync(input);
		input_report_key(input, KEY_GESTURE_POWER, 0);
		input_sync(input);
		gc->type  = GESTURE_SINGLECLICK;
		gc->clockwise = 1;
		gc->pos_end.x = gc->pos_start.x;
		gc->pos_end.y = gc->pos_start.y;
		break;
	case GESTURE_DOUBLECLICK:
		ILI_INFO("Double Click key event\n");
		input_report_key(input, GESTURE_DOUBLE_TAP, 1);
		input_sync(input);
		input_report_key(input, GESTURE_DOUBLE_TAP, 0);
		input_sync(input);
		gc->type  = GESTURE_DOUBLECLICK;
		gc->clockwise = 1;
		gc->pos_end.x = gc->pos_start.x;
		gc->pos_end.y = gc->pos_start.y;
		break;
	case GESTURE_LEFT:
		gc->type  = GESTURE_LEFT;
		gc->clockwise = 1;
		break;
	case GESTURE_RIGHT:
		gc->type  = GESTURE_RIGHT;
		gc->clockwise = 1;
		break;
	case GESTURE_UP:
		gc->type  = GESTURE_UP;
		gc->clockwise = 1;
		break;
	case GESTURE_DOWN:
		gc->type  = GESTURE_DOWN;
		gc->clockwise = 1;
		break;
	case GESTURE_O:
		gc->type  = GESTURE_O;
		if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			gc->clockwise = (ges[34] > 1) ? 0 : ges[34];
			lu_x = (((ges[28] & 0xF0) << 4) | (ges[29]));
			lu_y = (((ges[28] & 0x0F) << 8) | (ges[30]));
			rd_x = (((ges[31] & 0xF0) << 4) | (ges[32]));
			rd_y = (((ges[31] & 0x0F) << 8) | (ges[33]));
		} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
			gc->clockwise = (ges[44] > 1) ? 0 : ges[44];
			lu_x = ((ges[36] << 8) | (ges[37]));
			lu_y = ((ges[38] << 8) | (ges[39]));
			rd_x = ((ges[40] << 8) | (ges[41]));
			rd_y = ((ges[42] << 8) | (ges[43]));
		}

		gc->pos_1st.x = ((rd_x + lu_x) / 2);
		gc->pos_1st.y = lu_y;
		gc->pos_2nd.x = lu_x;
		gc->pos_2nd.y = ((rd_y + lu_y) / 2);
		gc->pos_3rd.x = ((rd_x + lu_x) / 2);
		gc->pos_3rd.y = rd_y;
		gc->pos_4th.x = rd_x;
		gc->pos_4th.y = ((rd_y + lu_y) / 2);
		break;
	case GESTURE_W:
		gc->type  = GESTURE_W;
		gc->clockwise = 1;
		break;
	case GESTURE_M:
		gc->type  = GESTURE_M;
		gc->clockwise = 1;
		break;
	case GESTURE_V:
		gc->type  = GESTURE_V;
		gc->clockwise = 1;
		break;
	case GESTURE_C:
		gc->type  = GESTURE_C;
		gc->clockwise = 1;
		break;
	case GESTURE_E:
		gc->type  = GESTURE_E;
		gc->clockwise = 1;
		break;
	case GESTURE_S:
		gc->type  = GESTURE_S;
		gc->clockwise = 1;
		break;
	case GESTURE_Z:
		gc->type  = GESTURE_Z;
		gc->clockwise = 1;
		break;
	case GESTURE_TWOLINE_DOWN:
		gc->type  = GESTURE_TWOLINE_DOWN;
		gc->clockwise = 1;
		if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			gc->pos_1st.x  = (((ges[10] & 0xF0) << 4) | (ges[11]));
			gc->pos_1st.y  = (((ges[10] & 0x0F) << 8) | (ges[12]));
			gc->pos_2nd.x  = (((ges[13] & 0xF0) << 4) | (ges[14]));
			gc->pos_2nd.y  = (((ges[13] & 0x0F) << 8) | (ges[15]));
		} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
			gc->pos_1st.x  = ((ges[12] << 8) | (ges[13]));
			gc->pos_1st.y  = ((ges[14] << 8) | (ges[15]));
			gc->pos_2nd.x  = ((ges[16] << 8) | (ges[17]));
			gc->pos_2nd.y  = ((ges[18] << 8) | (ges[19]));
		}
		break;
	default:
		ILI_ERR("Unknown gesture code\n");
		break;
	}

	if (!transfer) {
		if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			gc->pos_start.x	= gc->pos_start.x * ilits->panel_wid / TPD_WIDTH;
			gc->pos_start.y = gc->pos_start.y * ilits->panel_hei / TPD_HEIGHT;
			gc->pos_end.x   = gc->pos_end.x * ilits->panel_wid / TPD_WIDTH;
			gc->pos_end.y   = gc->pos_end.y * ilits->panel_hei / TPD_HEIGHT;
			gc->pos_1st.x   = gc->pos_1st.x * ilits->panel_wid / TPD_WIDTH;
			gc->pos_1st.y   = gc->pos_1st.y * ilits->panel_hei / TPD_HEIGHT;
			gc->pos_2nd.x   = gc->pos_2nd.x * ilits->panel_wid / TPD_WIDTH;
			gc->pos_2nd.y   = gc->pos_2nd.y * ilits->panel_hei / TPD_HEIGHT;
			gc->pos_3rd.x   = gc->pos_3rd.x * ilits->panel_wid / TPD_WIDTH;
			gc->pos_3rd.y   = gc->pos_3rd.y * ilits->panel_hei / TPD_HEIGHT;
			gc->pos_4th.x   = gc->pos_4th.x * ilits->panel_wid / TPD_WIDTH;
			gc->pos_4th.y   = gc->pos_4th.y * ilits->panel_hei / TPD_HEIGHT;
		} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
			gc->pos_start.x	= gc->pos_start.x * ilits->panel_wid / ilits->max_x;
			gc->pos_start.y = gc->pos_start.y * ilits->panel_hei / ilits->max_y;
			gc->pos_end.x   = gc->pos_end.x * ilits->panel_wid / ilits->max_x;
			gc->pos_end.y   = gc->pos_end.y * ilits->panel_hei / ilits->max_y;
			gc->pos_1st.x   = gc->pos_1st.x * ilits->panel_wid / ilits->max_x;
			gc->pos_1st.y   = gc->pos_1st.y * ilits->panel_hei / ilits->max_y;
			gc->pos_2nd.x   = gc->pos_2nd.x * ilits->panel_wid / ilits->max_x;
			gc->pos_2nd.y   = gc->pos_2nd.y * ilits->panel_hei / ilits->max_y;
			gc->pos_3rd.x   = gc->pos_3rd.x * ilits->panel_wid / ilits->max_x;
			gc->pos_3rd.y   = gc->pos_3rd.y * ilits->panel_hei / ilits->max_y;
			gc->pos_4th.x   = gc->pos_4th.x * ilits->panel_wid / ilits->max_x;
			gc->pos_4th.y   = gc->pos_4th.y * ilits->panel_hei / ilits->max_y;
		}

	}

	ILI_INFO("Transfer = %d, Type = %d, clockwise = %d\n", transfer, gc->type, gc->clockwise);
	ILI_INFO("Gesture Points: (%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)\n",
			gc->pos_start.x, gc->pos_start.y,
			gc->pos_end.x, gc->pos_end.y,
			gc->pos_1st.x, gc->pos_1st.y,
			gc->pos_2nd.x, gc->pos_2nd.y,
			gc->pos_3rd.x, gc->pos_3rd.y,
			gc->pos_4th.x, gc->pos_4th.y);

	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_report_i2cuart_mode(u8 *buf, int len)
{
	int type = buf[3] & 0x0F;
	int need_read_len = 0, one_data_bytes = 0;
	int actual_len = len - 5;
	int uart_len;
	u8 *uart_buf = NULL, *total_buf = NULL;

	ILI_DBG("data[3] = %x, type = %x, actual_len = %d\n",
					buf[3], type, actual_len);

	need_read_len = buf[1] * buf[2];

	if (type == 0 || type == 1 || type == 6) {
		one_data_bytes = 1;
	} else if (type == 2 || type == 3) {
		one_data_bytes = 2;
	} else if (type == 4 || type == 5) {
		one_data_bytes = 4;
	}

	need_read_len =  need_read_len * one_data_bytes + 1;
	ILI_DBG("need_read_len = %d  one_data_bytes = %d\n", need_read_len, one_data_bytes);

	if (need_read_len <= actual_len) {
		ilitek_tddi_touch_send_debug_data(buf, len);
		goto out;
	}

	uart_len = need_read_len - actual_len;
	ILI_DBG("uart len = %d\n", uart_len);

	uart_buf = kcalloc(uart_len, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(uart_buf)) {
		ILI_ERR("Failed to allocate uart_buf memory %ld\n", PTR_ERR(uart_buf));
		goto out;
	}

	if (ilits->wrapper(NULL, 0, uart_buf, uart_len, OFF, OFF) < 0) {
		ILI_ERR("i2cuart read data failed\n");
		goto out;
	}

	total_buf = kcalloc(len + uart_len, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(total_buf)) {
		ILI_ERR("Failed to allocate total_buf memory %ld\n", PTR_ERR(total_buf));
		goto out;
	}

	memcpy(total_buf, buf, len);
	memcpy(total_buf + len, uart_buf, uart_len);
	ilitek_tddi_touch_send_debug_data(total_buf, len + uart_len);

out:
	ipio_kfree((void **)&uart_buf);
	ipio_kfree((void **)&total_buf);
	return;
}

#ifdef ROI
int ili_read_knuckle_roi_data(void)
{
	int i = 0;
	int ret = 0;
	struct ts_kit_device_data *ts_dev_data = ilits->ts_dev_data;

	ILI_DBG("roi_switch = %d, roi_support =%d\n",
		ts_dev_data->ts_platform_data->feature_info.roi_info.roi_switch,
		ts_dev_data->ts_platform_data->feature_info.roi_info.roi_supported);

	ILI_INFO("last_fingers = %d, cur_fingers =%d\n",
		ilits->last_touch,
		ilits->finger);

	if (ts_dev_data->ts_platform_data->feature_info.roi_info.roi_switch && ts_dev_data->ts_platform_data->feature_info.roi_info.roi_supported) {
		if (ilits->last_touch != ilits->finger && ilits->finger <= ILITEK_KNUCKLE_ROI_FINGERS) {
			ret = ili_config_knuckle_roi_ctrl(CMD_ROI_DATA);
			if (ret) {
				ILI_ERR("write data failed, ret = %d\n", ret);
				return ret;
			}
			mdelay(1);
			ret = ilits->wrapper(NULL, 0, ilits->knuckle_roi_data, ROI_DATA_READ_LENGTH, OFF, OFF);
			if (ret) {
				ILI_ERR("read data failed, ret = %d\n", ret);
				return ret;
			}

			ILI_DBG("index = %d, fixed = %d, peak_row = %d, peak_colum = %d\n",
				ilits->knuckle_roi_data[0],
				ilits->knuckle_roi_data[1],
				ilits->knuckle_roi_data[2],
				ilits->knuckle_roi_data[3]);
			if (debug_en) {
				printk("=================== roi data in bytes ===================\n");
				for (i = 4; i < ROI_DATA_READ_LENGTH; i++) {
				printk(KERN_CONT "%3d ", ilits->knuckle_roi_data[i]);
					if ((i - 3) % 14 == 0) { /* 14 * 14  rawdata bytes */
						printk("\n");
					}
				}
			}
		}
	}
	return 0;
}
#endif
