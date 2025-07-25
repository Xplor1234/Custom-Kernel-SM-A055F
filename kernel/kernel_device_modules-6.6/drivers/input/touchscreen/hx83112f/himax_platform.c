/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for QCT platform
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#ifndef	HX_SMART_WAKEUP
#define HX_SMART_WAKEUP
#endif

#include "himax_platform.h"
#include "himax_common.h"
#include<linux/hardware_info.h>
#include <asm/setup.h>
int t_lcm_name = 0;
#include <linux/power_supply.h>
int i2c_error_count;
bool ic_boot_done;
struct spi_device *spi;
extern bool w2_gesture_status;
bool  himax_glove_status = DISABLE;
static struct notifier_block himax_charger_notifier;
static uint8_t *g_xfer_data;
/*  Turn on it when there is necessary. */
#if defined(HX_RW_FILE)
MODULE_IMPORT_NS(
VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_IMPORT_NS(ANDROID_GKI_VFS_EXPORT_ONLY);

mm_segment_t g_fs;
struct file *g_fn;
loff_t g_pos;

int hx_open_file(char *file_name)
{

	int ret = -EFAULT;

	g_fn = NULL;
	g_pos = 0;


	I("Now file name=%s\n", file_name);
	g_fn = filp_open(file_name,
		O_TRUNC|O_CREAT|O_RDWR, 0660);
	if (IS_ERR(g_fn))
		E("%s Open file failed!\n", __func__);
	else {
		ret = NO_ERR;
		g_fs = get_fs();
		set_fs(KERNEL_DS);
	}
	return ret;
}
int hx_write_file(char *write_data, uint32_t write_size, loff_t pos)
{
	int ret = NO_ERR;

	g_pos = pos;
	kernel_write(g_fn, write_data,
	write_size * sizeof(char), &g_pos);


	return ret;
}
int hx_close_file(void)
{
	int ret = NO_ERR;

	filp_close(g_fn, NULL);
	set_fs(g_fs);

	return ret;
}
#endif

int himax_dev_set(struct himax_ts_data *ts)
{
	int ret = 0;

	ts->input_dev = input_allocate_device();

	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		E("%s: Failed to allocate input device\n", __func__);
		return ret;
	}

	ts->input_dev->name = "himax-touchscreen";

	if (!ic_data->HX_STYLUS_FUNC)
		goto skip_stylus_operation;

	ts->stylus_dev = input_allocate_device();

	if (ts->stylus_dev == NULL) {
		ret = -ENOMEM;
		E("%s: Failed to allocate input device-stylus_dev\n", __func__);
		input_free_device(ts->input_dev);
		return ret;
	}

	ts->stylus_dev->name = "himax-stylus";
skip_stylus_operation:

	return ret;
}
int himax_input_register_device(struct input_dev *input_dev)
{
	return input_register_device(input_dev);
}

int himax_parse_dt(struct himax_ts_data *ts, struct himax_platform_data *pdata)
{
	int rc, coords_size = 0;
	uint32_t coords[4] = {0};
	struct property *prop;
	struct device_node *dt = ts->dev->of_node;
	u32 data = 0;
	int ret = 0;

	prop = of_find_property(dt, "himax,panel-coords", NULL);
	if (prop) {
		coords_size = prop->length / sizeof(u32);
		if (coords_size != 4)
			D(" %s:Invalid panel coords size %d\n",
				__func__, coords_size);
	}

	ret = of_property_read_u32_array(dt, "himax,panel-coords",
			coords, coords_size);
	if (ret == 0) {
		pdata->abs_x_min = coords[0];
		pdata->abs_x_max = (coords[1] - 1);
		pdata->abs_y_min = coords[2];
		pdata->abs_y_max = (coords[3] - 1);
		I(" DT-%s:panel-coords = %d, %d, %d, %d\n", __func__,
				pdata->abs_x_min,
				pdata->abs_x_max,
				pdata->abs_y_min,
				pdata->abs_y_max);
	}

	prop = of_find_property(dt, "himax,display-coords", NULL);
	if (prop) {
		coords_size = prop->length / sizeof(u32);
		if (coords_size != 4)
			D(" %s:Invalid display coords size %d\n",
				__func__, coords_size);
	}

	rc = of_property_read_u32_array(dt, "himax,display-coords",
			coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		D(" %s:Fail to read display-coords %d\n", __func__, rc);
		return rc;
	}
	pdata->screenWidth  = coords[1];
	pdata->screenHeight = coords[3];
	I(" DT-%s:display-coords = (%d, %d)\n", __func__,
			pdata->screenWidth,
			pdata->screenHeight);

	pdata->gpio_irq = of_get_named_gpio(dt, "focaltech,irq-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_irq))
		I(" DT:gpio_irq value is not valid\n");

	pdata->gpio_reset = of_get_named_gpio(dt, "focaltech,reset-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_reset))
		I(" DT:gpio_rst value is not valid\n");

/* 	pdata->gpio_3v3_en = of_get_named_gpio(dt, "himax,3v3-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_3v3_en))
		I(" DT:gpio_3v3_en value is not valid\n"); */

	I(" DT:gpio_irq=%d, gpio_rst=%d\n",
			pdata->gpio_irq,
			pdata->gpio_reset);

#if defined(HX_TP_TRIGGER_LCM_RST)
	pdata->lcm_rst = of_get_named_gpio(dt, "himax,lcm-rst", 0);

	if (!gpio_is_valid(pdata->lcm_rst))
		I(" DT:tp-rst value is not valid\n");

	I(" DT:pdata->lcm_rst=%d\n",
		pdata->lcm_rst);
#endif

	if (of_property_read_u32(dt, "report_type", &data) == 0) {
		pdata->protocol_type = data;
		I(" DT:protocol_type=%d\n", pdata->protocol_type);
	}
#if defined(HX_FIRMWARE_HEADER)
	mapping_panel_id_from_dt(dt);
#endif

	return 0;
}
EXPORT_SYMBOL(himax_parse_dt);

#if defined(HX_PARSE_FROM_DT)
static void hx_generate_ic_info_from_dt(
	uint32_t proj_id, char *buff, char *main_str, char *item_str)
{
	if (proj_id == 0xffff)
		snprintf(buff, 128, "%s,%s", main_str, item_str);
	else
		snprintf(buff, 128, "%s_%04X,%s", main_str, proj_id, item_str);

}
static void hx_generate_fw_name_from_dt(
	uint32_t proj_id, char *buff, char *main_str, char *item_str)
{
	if (proj_id == 0xffff)
		snprintf(buff, 128, "%s_%s", main_str, item_str);
	else
		snprintf(buff, 128, "%s_%04X_%s", main_str, proj_id, item_str);

}

void himax_parse_dt_ic_info(struct himax_ts_data *ts,
		struct himax_platform_data *pdata)
{
	struct device_node *dt = ts->dev->of_node;
	u32 data = 0;
	char *str_rx_num = "rx-num";
	char *str_tx_num = "tx-num";
	char *str_bt_num = "bt-num";
	char *str_max_pt = "max-pt";
	char *str_int_edge = "int-edge";
	char *str_stylus_func = "stylus-func";
	char *str_firmware_name_tail = "firmware.bin";
	char *str_mp_firmware_name_tail = "mpfw.bin";
	char buff[128] = {0};

	hx_generate_ic_info_from_dt(g_proj_id, buff, ts->chip_name, str_rx_num);
	if (of_property_read_u32(dt, buff, &data) == 0) {
		ic_data->HX_RX_NUM = data;
		I("%s,Now parse:%s=%d\n", __func__, buff, ic_data->HX_RX_NUM);
	} else
		I("%s, No definition: %s!\n", __func__, buff);

	hx_generate_ic_info_from_dt(g_proj_id, buff, ts->chip_name, str_tx_num);
	if (of_property_read_u32(dt, buff, &data) == 0) {
		ic_data->HX_TX_NUM = data;
		I("%s,Now parse:%s=%d\n", __func__, buff, ic_data->HX_TX_NUM);
	} else
		I("%s, No definition: %s!\n", __func__, buff);

	hx_generate_ic_info_from_dt(g_proj_id, buff, ts->chip_name, str_bt_num);
	if (of_property_read_u32(dt, buff, &data) == 0) {
		ic_data->HX_BT_NUM = data;
		I("%s,Now parse:%s=%d\n", __func__, buff, ic_data->HX_BT_NUM);
	} else
		I("%s, No definition: %s!\n", __func__, buff);

	hx_generate_ic_info_from_dt(g_proj_id, buff, ts->chip_name, str_max_pt);
	if (of_property_read_u32(dt, buff, &data) == 0) {
		ic_data->HX_MAX_PT = data;
		I("%s,Now parse:%s=%d\n", __func__, buff, ic_data->HX_MAX_PT);
	} else
		I("%s, No definition: %s!\n", __func__, buff);

	hx_generate_ic_info_from_dt(
			g_proj_id, buff, ts->chip_name, str_int_edge);
	if (of_property_read_u32(dt, buff, &data) == 0) {
		ic_data->HX_INT_IS_EDGE = data;
		I("%s,Now parse:%s=%d\n",
			__func__, buff, ic_data->HX_INT_IS_EDGE);
	} else
		I("%s, No definition: %s!\n", __func__, buff);

	hx_generate_ic_info_from_dt(g_proj_id, buff,
		ts->chip_name, str_stylus_func);
	if (of_property_read_u32(dt, buff, &data) == 0) {
		ic_data->HX_STYLUS_FUNC = data;
		I("%s,Now parse:%s=%d\n",
			__func__, buff, ic_data->HX_STYLUS_FUNC);
	} else
		I("%s, No definition: %s!\n", __func__, buff);

	hx_generate_fw_name_from_dt(g_proj_id, buff, ts->chip_name,
		str_firmware_name_tail);
	I("%s,buff=%s!\n", __func__, buff);
#if defined(HX_BOOT_UPGRADE)
	g_fw_boot_upgrade_name = kzalloc(sizeof(buff), GFP_KERNEL);
	memcpy(&g_fw_boot_upgrade_name[0], &buff[0], sizeof(buff));
	I("%s,g_fw_boot_upgrade_name=%s!\n",
		__func__, g_fw_boot_upgrade_name);
#endif

	hx_generate_fw_name_from_dt(g_proj_id, buff, ts->chip_name,
		str_mp_firmware_name_tail);
	I("%s,buff=%s!\n", __func__, buff);
#if defined(HX_ZERO_FLASH)
	g_fw_mp_upgrade_name = kzalloc(sizeof(buff), GFP_KERNEL);
	memcpy(&g_fw_mp_upgrade_name[0], &buff[0], sizeof(buff));
	I("%s,g_fw_mp_upgrade_name=%s!\n", __func__, g_fw_mp_upgrade_name);
#endif

	I(" DT:rx, tx, bt, pt, int, stylus\n");
	I(" DT:%d, %d, %d, %d, %d, %d\n", ic_data->HX_RX_NUM,
		ic_data->HX_TX_NUM, ic_data->HX_BT_NUM, ic_data->HX_MAX_PT,
		ic_data->HX_INT_IS_EDGE, ic_data->HX_STYLUS_FUNC);
}
EXPORT_SYMBOL(himax_parse_dt_ic_info);
#endif

static int himax_spi_read(uint8_t *cmd, uint8_t cmd_len, uint8_t *buf,
	uint32_t len)
{
	struct spi_message m;
	int result = NO_ERR;
	int retry;
	int error;
	struct spi_transfer	t = {
		.len	= cmd_len + len,
	};

//	memset(g_xfer_data, 0, cmd_len + len);
//	memcpy(g_xfer_data, cmd, cmd_len);

//	t.tx_buf = cmd;
	t.tx_buf = g_xfer_data;
	t.rx_buf = g_xfer_data;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	for (retry = 0; retry < HIMAX_BUS_RETRY_TIMES; retry++) {
		error = spi_sync(private_ts->spi, &m);
		if (unlikely(error))
			E("SPI read error: %d\n", error);
		else
			break;
	}

	if (retry == HIMAX_BUS_RETRY_TIMES) {
		E("%s: SPI read error retry over %d\n",
			__func__, HIMAX_BUS_RETRY_TIMES);
		result = -EIO;
		goto END;
	} else {
		memcpy(buf, g_xfer_data+cmd_len, len);
	}

END:
	return result;
}

static int himax_spi_write(uint8_t *buf, uint32_t length)
{
	int status;
	struct spi_message	m;
	struct spi_transfer	t = {
			.tx_buf		= buf,
			.len		= length,
	};

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	status = spi_sync(private_ts->spi, &m);

	if (status == 0) {
		status = m.status;
		if (status == 0)
			status = m.actual_length;
	}
	return status;

}

int himax_bus_read(uint8_t cmd, uint8_t *buf, uint32_t len)
{
	int result = -1;
	uint8_t hw_addr = 0x00;
//	uint8_t spi_format_buf[BUS_R_HLEN] = {0};

	if (len > BUS_R_DLEN) {
		E("%s: len[%d] is over %d\n", __func__, len, BUS_R_DLEN);
		return result;
	}

	mutex_lock(&(private_ts->rw_lock));

//	spi_format_buf[0] = 0xF3;
//	spi_format_buf[1] = cmd;
//	spi_format_buf[2] = 0x00;

//	result = himax_spi_read(spi_format_buf, BUS_R_HLEN, buf, len);
	if (private_ts->select_slave_reg) {
		hw_addr = private_ts->slave_read_reg;
		I("%s: now addr=0x%02X!\n", __func__, hw_addr);
	} else
		hw_addr = 0xF3;

	memset(g_xfer_data, 0, BUS_R_HLEN+len);
	g_xfer_data[0] = hw_addr;
	g_xfer_data[1] = cmd;
	g_xfer_data[2] = 0x00;
	result = himax_spi_read(g_xfer_data, BUS_R_HLEN, buf, len);

	mutex_unlock(&(private_ts->rw_lock));

	return result;
}
EXPORT_SYMBOL(himax_bus_read);

int himax_bus_write(uint8_t cmd, uint8_t *addr, uint8_t *data, uint32_t len)
{
	int result = -1;
	uint8_t offset = 0;
	uint32_t tmp_len = len;
	uint8_t hw_addr = 0x00;

	if (len > BUS_W_DLEN) {
		E("%s: len[%d] is over %d\n", __func__, len, BUS_W_DLEN);
		return -EFAULT;
	}

	mutex_lock(&(private_ts->rw_lock));

	if (private_ts->select_slave_reg) {
		hw_addr = private_ts->slave_write_reg;
		I("%s: now addr=0x%02X!\n", __func__, hw_addr);
	} else
		hw_addr = 0xF2;

	g_xfer_data[0] = hw_addr;
	g_xfer_data[1] = cmd;
	offset = BUS_W_HLEN;

	if (addr != NULL) {
		memcpy(g_xfer_data+offset, addr, 4);
		offset += 4;
		tmp_len -= 4;
	}

	if (data != NULL)
		memcpy(g_xfer_data+offset, data, tmp_len);

	result = himax_spi_write(g_xfer_data, len+BUS_W_HLEN);

	mutex_unlock(&(private_ts->rw_lock));

	return result;
}
EXPORT_SYMBOL(himax_bus_write);

void himax_int_enable(int enable)
{
	struct himax_ts_data *ts = private_ts;
	unsigned long irqflags = 0;
	int irqnum = ts->hx_irq;

	spin_lock_irqsave(&ts->irq_lock, irqflags);
	I("%s: Entering! irqnum = %d\n", __func__, irqnum);
	if (enable == 1 && atomic_read(&ts->irq_state) == 0) {
		atomic_set(&ts->irq_state, 1);
		enable_irq(irqnum);
		private_ts->irq_enabled = 1;
	} else if (enable == 0 && atomic_read(&ts->irq_state) == 1) {
		atomic_set(&ts->irq_state, 0);
		disable_irq_nosync(irqnum);
		private_ts->irq_enabled = 0;
	}

	I("enable = %d\n", enable);
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}
EXPORT_SYMBOL(himax_int_enable);

#if defined(HX_RST_PIN_FUNC)
void himax_rst_gpio_set(int pinnum, uint8_t value)
{
	gpio_direction_output(pinnum, value);
}
EXPORT_SYMBOL(himax_rst_gpio_set);
#endif

uint8_t himax_int_gpio_read(int pinnum)
{
	return gpio_get_value(pinnum);
}

#if defined(CONFIG_HMX_DB)
static int himax_regulator_configure(struct himax_platform_data *pdata)
{
	int retval;
	/* struct i2c_client *client = private_ts->client; */

	pdata->vcc_dig = regulator_get(private_ts->dev, "vdd");

	if (IS_ERR(pdata->vcc_dig)) {
		E("%s: Failed to get regulator vdd\n",
		  __func__);
		retval = PTR_ERR(pdata->vcc_dig);
		return retval;
	}

	pdata->vcc_ana = regulator_get(private_ts->dev, "avdd");

	if (IS_ERR(pdata->vcc_ana)) {
		E("%s: Failed to get regulator avdd\n",
		  __func__);
		retval = PTR_ERR(pdata->vcc_ana);
		regulator_put(pdata->vcc_dig);
		return retval;
	}

	return 0;
};

static void himax_regulator_deinit(struct himax_platform_data *pdata)
{
	I("%s: entered.\n", __func__);

	if (!IS_ERR(pdata->vcc_ana))
		regulator_put(pdata->vcc_ana);

	if (!IS_ERR(pdata->vcc_dig))
		regulator_put(pdata->vcc_dig);

	I("%s: regulator put, completed.\n", __func__);
};

static int himax_power_on(struct himax_platform_data *pdata, bool on)
{
	int retval;

	if (on) {
		retval = regulator_enable(pdata->vcc_dig);

		if (retval) {
			E("%s: Failed to enable regulator vdd\n",
					__func__);
			return retval;
		}

		/*msleep(100);*/
		usleep_range(1000, 1001);
		retval = regulator_enable(pdata->vcc_ana);

		if (retval) {
			E("%s: Failed to enable regulator avdd\n",
					__func__);
			regulator_disable(pdata->vcc_dig);
			return retval;
		}
	} else {
		regulator_disable(pdata->vcc_dig);
		regulator_disable(pdata->vcc_ana);
	}

	return 0;
}

int himax_gpio_power_config(struct himax_platform_data *pdata)
{
	int error;
	/* struct i2c_client *client = private_ts->client; */

	error = himax_regulator_configure(pdata);

	if (error) {
		E("Failed to intialize hardware\n");
		goto err_regulator_not_on;
	}

#if defined(HX_RST_PIN_FUNC)

	if (gpio_is_valid(pdata->gpio_reset)) {
		/* configure touchscreen reset out gpio */
		error = gpio_request(pdata->gpio_reset, "hmx_reset_gpio");

		if (error) {
			E("unable to request reset gpio [%d]\n", pdata->gpio_reset);
			goto err_gpio_reset_req;
		}

		error = gpio_direction_output(pdata->gpio_reset, 0);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
				pdata->gpio_reset);
			goto err_gpio_reset_dir;
		}
	}

#endif
#if defined(HX_TP_TRIGGER_LCM_RST)
	if (pdata->lcm_rst >= 0) {
		error = gpio_direction_output(pdata->lcm_rst, 0);
		if (error) {
			E("unable to set direction for lcm_rst [%d]\n",
				pdata->lcm_rst);
		}
	}
#endif
	error = himax_power_on(pdata, true);

	if (error) {
		E("Failed to power on hardware\n");
		goto err_power_on;
	}

	if (gpio_is_valid(pdata->gpio_irq)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->gpio_irq, "hmx_gpio_irq");

		if (error) {
			E("unable to request irq gpio [%d]\n",
			  pdata->gpio_irq);
			goto err_req_irq_gpio;
		}

		error = gpio_direction_input(pdata->gpio_irq);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_irq);
			goto err_set_gpio_irq;
		}

		private_ts->hx_irq = gpio_to_irq(pdata->gpio_irq);
	} else {
		E("irq gpio not provided\n");
		goto err_req_irq_gpio;
	}
#if defined(HX_TP_TRIGGER_LCM_RST)
	if (pdata->lcm_rst >= 0) {
		error = gpio_direction_output(pdata->lcm_rst, 1);

		if (error) {
			E("lcm_rst unable to set direction for gpio [%d]\n",
				pdata->lcm_rst);
		}
	}
	gpio_free(pdata->lcm_rst);
#endif
	/*msleep(20);*/
	usleep_range(2000, 2001);
#if defined(HX_RST_PIN_FUNC)

	if (gpio_is_valid(pdata->gpio_reset)) {
		error = gpio_direction_output(pdata->gpio_reset, 1);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
					pdata->gpio_reset);
			goto err_gpio_reset_set_high;
		}
	}

#endif
	return 0;
#if defined(HX_RST_PIN_FUNC)
err_gpio_reset_set_high:
#endif
err_set_gpio_irq:
	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);

err_req_irq_gpio:
	himax_power_on(pdata, false);
err_power_on:
#if defined(HX_RST_PIN_FUNC)
err_gpio_reset_dir:
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);

err_gpio_reset_req:
#endif
	himax_regulator_deinit(pdata);
err_regulator_not_on:
	return error;
}

#else
int himax_gpio_power_config(struct himax_platform_data *pdata)
{
	int error = 0;
	/* struct i2c_client *client = private_ts->client; */
#if defined(HX_RST_PIN_FUNC)

	if (pdata->gpio_reset >= 0) {
		error = gpio_request(pdata->gpio_reset, "himax-reset");

		if (error < 0) {
			E("%s: request reset pin failed\n", __func__);
			goto err_gpio_reset_req;
		}

		error = gpio_direction_output(pdata->gpio_reset, 0);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
					pdata->gpio_reset);
			goto err_gpio_reset_dir;
		}
	}

#endif

#if defined(HX_TP_TRIGGER_LCM_RST)
	if (pdata->lcm_rst >= 0) {
		error = gpio_direction_output(pdata->lcm_rst, 0);
		if (error) {
			E("unable to set direction for lcm_rst [%d]\n",
				pdata->lcm_rst);
		}
	}

#endif


/* 	if (pdata->gpio_3v3_en >= 0) {
		error = gpio_request(pdata->gpio_3v3_en, "himax-3v3_en");

		if (error < 0) {
			E("%s: request 3v3_en pin failed\n", __func__);
			goto err_gpio_3v3_req;
		}

		gpio_direction_output(pdata->gpio_3v3_en, 1);
		I("3v3_en set 1 get pin = %d\n",
			gpio_get_value(pdata->gpio_3v3_en));
	}
 */
	if (gpio_is_valid(pdata->gpio_irq)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->gpio_irq, "himax_gpio_irq");

		if (error) {
			E("unable to request gpio [%d]\n", pdata->gpio_irq);
			goto err_gpio_irq_req;
		}

		error = gpio_direction_input(pdata->gpio_irq);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
				pdata->gpio_irq);
			goto err_gpio_irq_set_input;
		}

		private_ts->hx_irq = gpio_to_irq(pdata->gpio_irq);
	} else {
		E("irq gpio not provided\n");
		goto err_gpio_irq_req;
	}

	usleep_range(2000, 2001);

#if defined(HX_TP_TRIGGER_LCM_RST)
	msleep(20);

	if (pdata->lcm_rst >= 0) {
		error = gpio_direction_output(pdata->lcm_rst, 1);

		if (error) {
			E("lcm_rst unable to set direction for gpio [%d]\n",
				pdata->lcm_rst);
		}
	}
	msleep(20);
	gpio_free(pdata->lcm_rst);
#endif

#if defined(HX_RST_PIN_FUNC)

	if (pdata->gpio_reset >= 0) {
		error = gpio_direction_output(pdata->gpio_reset, 1);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_gpio_reset_set_high;
		}
	}
#endif

	return error;

#if defined(HX_RST_PIN_FUNC)
err_gpio_reset_set_high:
#endif
err_gpio_irq_set_input:
	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);
err_gpio_irq_req:
	if (pdata->gpio_irq >= 0)
		gpio_free(pdata->gpio_irq);
//err_gpio_3v3_req:
#if defined(HX_RST_PIN_FUNC)
err_gpio_reset_dir:
	if (pdata->gpio_reset >= 0)
		gpio_free(pdata->gpio_reset);
err_gpio_reset_req:
#endif
	return error;
}

#endif

void himax_gpio_power_deconfig(struct himax_platform_data *pdata)
{
	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);

#if defined(HX_RST_PIN_FUNC)
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);
#endif

#if defined(CONFIG_HMX_DB)
	himax_power_on(pdata, false);
	himax_regulator_deinit(pdata);
/* #else
	if (pdata->gpio_3v3_en >= 0)
		gpio_free(pdata->gpio_3v3_en); */

#endif
}

static void himax_ts_isr_func(struct himax_ts_data *ts)
{
	himax_ts_work(ts);
}

irqreturn_t himax_ts_thread(int irq, void *ptr)
{
	himax_ts_isr_func((struct himax_ts_data *)ptr);

	return IRQ_HANDLED;
}

static void himax_ts_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work,
		struct himax_ts_data, work);

	himax_ts_work(ts);
}

int himax_int_register_trigger(void)
{
	int ret = 0;
	struct himax_ts_data *ts = private_ts;

	if (ic_data->HX_INT_IS_EDGE) {
		I("%s edge triiger falling\n", __func__);
		ret = request_threaded_irq(ts->hx_irq, NULL, himax_ts_thread,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			HIMAX_common_NAME, ts);

	} else {
		I("%s level trigger low\n", __func__);
		ret = request_threaded_irq(ts->hx_irq, NULL, himax_ts_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, HIMAX_common_NAME, ts);
	}

	return ret;
}

int himax_int_en_set(void)
{
	int ret = NO_ERR;

	ret = himax_int_register_trigger();
	return ret;
}

int himax_ts_register_interrupt(void)
{
	struct himax_ts_data *ts = private_ts;
	/* struct i2c_client *client = private_ts->client; */
	int ret = 0;

	ts->irq_enabled = 0;

	/* Work functon */
	if (private_ts->hx_irq) {/*INT mode*/
		ts->use_irq = 1;
		ret = himax_int_register_trigger();

		if (ret == 0) {
			ts->irq_enabled = 1;
			atomic_set(&ts->irq_state, 1);
			I("%s: irq enabled at gpio: %d\n", __func__,
				private_ts->hx_irq);
#if defined(HX_SMART_WAKEUP)
			irq_set_irq_wake(private_ts->hx_irq, 1);
#endif
		} else {
			ts->use_irq = 0;
			E("%s: request_irq failed\n", __func__);
		}
	} else {
		I("%s: private_ts->hx_irq is empty, use polling mode.\n",
			__func__);
	}

	/*if use polling mode need to disable HX_ESD_RECOVERY function*/
	if (!ts->use_irq) {
		ts->himax_wq = create_singlethread_workqueue("himax_touch");
		INIT_WORK(&ts->work, himax_ts_work_func);
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = himax_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		I("%s: polling mode enabled\n", __func__);
	}

	return ret;
}

int himax_ts_unregister_interrupt(void)
{
	struct himax_ts_data *ts = private_ts;
	int ret = 0;

	I("%s: entered.\n", __func__);

	/* Work functon */
	if (private_ts->hx_irq && ts->use_irq) {/*INT mode*/
#if defined(HX_SMART_WAKEUP)
		irq_set_irq_wake(ts->hx_irq, 0);
#endif
		free_irq(ts->hx_irq, ts);
		I("%s: irq disabled at qpio: %d\n", __func__,
			private_ts->hx_irq);
	}

	/*if use polling mode need to disable HX_ESD_RECOVERY function*/
	if (!ts->use_irq) {
		hrtimer_cancel(&ts->timer);
		cancel_work_sync(&ts->work);
		if (ts->himax_wq != NULL)
			destroy_workqueue(ts->himax_wq);
		I("%s: polling mode destroyed", __func__);
	}

	return ret;
}

#ifdef SEC_TSP_FACTORY_TEST
static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[32] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s fw:%02X",
			private_ts->chip_name,
			ic_data->vendor_touch_cfg_ver);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	I("%s: %s\n", __func__, buff);
}
static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[32] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s fw:%02X",
			private_ts->chip_name,
			ic_data->vendor_touch_cfg_ver);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	I("%s: %s\n", __func__, buff);
}
static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct himax_ts_data *info = container_of(sec, struct himax_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned int buf[4];

	I("aot_enable Enter.\n");
	sec_cmd_set_default_result(sec);

	buf[0] = sec->cmd_param[0];

	if(buf[0] < 0 || buf[0] > 1){
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}else{
		if(buf[0] == 1){
			info->SMWP_enable = 1;
			info->gesture_cust_en[0] = 1;
		} else {
			info->SMWP_enable = 0;
			info->gesture_cust_en[0] = 0;
		}
	}

#if defined(HX_SMART_WAKEUP)
	if(info->SMWP_enable == 1){
		w2_gesture_status = ENABLE;
	}
	else{
		w2_gesture_status = DISABLE;
	}
	g_core_fp.fp_set_SMWP_enable(info->SMWP_enable, private_ts->suspended);
#endif

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	I( "%s: %s,sec->cmd_state == %d\n", __func__, buff, sec->cmd_state);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);
}
static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct himax_ts_data *info = container_of(sec, struct himax_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	unsigned int buf[4];
	I("glove_mode Enter.\n");
	sec_cmd_set_default_result(sec);
	buf[0] = sec->cmd_param[0];
	if(buf[0] > 1){
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}else{
		if(buf[0] == 1){
			info->HSEN_enable = 1;
		} else {
			info->HSEN_enable = 0;
		}
	}
#if defined(HX_HIGH_SENSE)
	if(info->HSEN_enable == 1)
		himax_glove_status = ENABLE;
	else
		himax_glove_status = DISABLE;
	g_core_fp.fp_set_HSEN_enable(info->HSEN_enable, private_ts->suspended);
#endif
	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	I( "%s: %s,sec->cmd_state == %d\n", __func__, buff, sec->cmd_state);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);
}
static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s", "NA");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	I("%s: %s\n", __func__, buff);
}
struct sec_cmd hx_commands[] = {
	/* the get_fw_ver_ic is same  whit the get_fw_ver_bin, because the TP's+
	* ic whitout flash in this project*/
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("aot_enable", aot_enable),},
	{SEC_CMD("glove_mode", glove_mode),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};
/*
static ssize_t himax_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 feature = 0;

	feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d\n", feature);
}

static DEVICE_ATTR(support_feature, 0444, himax_support_feature, NULL);

static struct attribute *hx_cmd_attributes[] = {
	&dev_attr_support_feature.attr,
	NULL,
};

static struct attribute_group hx_cmd_attr_group = {
	.attrs = hx_cmd_attributes,
};*/
#endif

static int himax_common_suspend(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter\n", __func__);
	if (!ts->initialized) {
		E("%s: init not ready, skip!\n", __func__);
		return -ECANCELED;
	}
	himax_chip_common_suspend(ts);
	return 0;
}
#if !defined(HX_CONTAINER_SPEED_UP)
static int himax_common_resume(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter\n", __func__);
	/*
	 *	wait until device resume for TDDI
	 *	TDDI: Touch and display Driver IC
	 */
	if (!ts->initialized) {
#if defined(HX_CONFIG_DRM) && !defined(HX_CONFIG_FB)
		if (himax_chip_common_init())
			return -ECANCELED;
#else
		E("%s: init not ready, skip!\n", __func__);
		return -ECANCELED;
#endif
	}
	himax_chip_common_resume(ts);
	return 0;
}
#endif

#if defined(HX_CONFIG_DRM_MTK)
/**
 * himax_ts_disp_notifier_callback - mtk display notifier callback
 * Called by kernel during framebuffer blanck/unblank phrase
 */
int himax_disp_notifier_callback(struct notifier_block *nb,
	unsigned long value, void *v)
{
	struct himax_ts_data *ts =
		container_of(nb, struct himax_ts_data, fb_notif);
	int *data = (int *)v;
	I("disp notifier event:%lu, data:%d\n", value, *data);
	if (ts && v) {
		if (value == MTK_DISP_EARLY_EVENT_BLANK) {
			if (*data == MTK_DISP_BLANK_POWERDOWN)
				himax_common_suspend(ts->dev);
		} else if (value == MTK_DISP_EVENT_BLANK) {
			if (*data == MTK_DISP_BLANK_UNBLANK)
				himax_common_resume(ts->dev);
		}
	} else {
		I("HIMAX touch IC can not suspend or resume");
		return -ECANCELED;
	}
	return 0;
}

static void himax_fb_register(struct work_struct *work)
{
	int ret = 0;

	struct himax_ts_data *ts = container_of(work, struct himax_ts_data,
			work_att.work);

	I("%s in\n", __func__);
#if defined(HX_CONFIG_DRM_MTK)
	ts->fb_notif.notifier_call = himax_disp_notifier_callback;
	ret = mtk_disp_notifier_register("HIMAX Touch", &ts->fb_notif);
	if (ret)
		E("Failed to register disp notifier client:%d\n", ret);
#endif
	if (ret)
		E("Unable to register fb_notifier: %d\n", ret);
}
#endif

static bool himax_assigned_fw_name(void)
{
	if (t_lcm_name == 3) {
		g_fw_boot_upgrade_name = "Himax_83112f_fw_dsbj.bin";
		g_fw_mp_upgrade_name = "Himax_83112f_mp_dsbj.bin";
	} else if(t_lcm_name == 4) {
		g_fw_boot_upgrade_name = "Himax_83112f_fw_djn.bin";
		g_fw_mp_upgrade_name = "Himax_83112f_mp_djn.bin";
	}

	return 0;
}

int himax_charger_notifier_callback(
	struct notifier_block *self,
	unsigned long event,
	void *data)
{
	struct power_supply *psy = data;
	union power_supply_propval pval;
	int ret;
	ret = power_supply_get_property(psy,POWER_SUPPLY_PROP_ONLINE, &pval);
	if (ret < 0) {
               printk("failed to get online prop\n");
               return NOTIFY_DONE;
       }
//	E("himax_charger_nodifier_callback start pval.intval = %d\n",pval.intval);
	if(pval.intval){
		USB_detect_flag = 1;
	}
	else{
		USB_detect_flag = 0;
	}
//	E("himax_charger_nodifier_callback end\n");
	return 0;
}

int himax_chip_common_probe(struct spi_device *spi)
{
	struct himax_ts_data *ts;
	int ret = 0;

	I("Enter %s\n", __func__);
	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
		dev_err(&spi->dev,
			"%s: Full duplex not supported by host\n",
			__func__);
		return -EIO;
	}

	g_xfer_data = kzalloc(BUS_RW_MAX_LEN, GFP_KERNEL);
	if (g_xfer_data == NULL) {
		E("%s: allocate g_xfer_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_g_xfer_data_failed;
	}

	ts = kzalloc(sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		E("%s: allocate himax_ts_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	private_ts = ts;
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_3;
	spi->chip_select = 0;

	ts->spi = spi;
	mutex_init(&ts->rw_lock);
	mutex_init(&ts->reg_lock);
	ts->dev = &spi->dev;
	dev_set_drvdata(&spi->dev, ts);
	spi_set_drvdata(spi, ts);

	ts->probe_finish = false;
	ts->initialized = false;
#if (defined(HX_BOOT_UPGRADE) || defined(HX_ZERO_FLASH))
	 himax_assigned_fw_name();
#endif
	ret = himax_chip_common_init();
	if (ret < 0)
		goto err_common_init_failed;

#if defined(HX_CONFIG_DRM_MTK)
	ts->himax_att_wq = create_singlethread_workqueue("HMX_ATT_request");
	if (!ts->himax_att_wq) {
		E(" allocate himax_att_wq failed\n");
		ret = -ENOMEM;
		goto err_get_intr_bit_failed;
	}

	INIT_DELAYED_WORK(&ts->work_att, himax_fb_register);
	queue_delayed_work(ts->himax_att_wq, &ts->work_att,
			msecs_to_jiffies(0));
#endif

#ifdef SEC_TSP_FACTORY_TEST
	ret = sec_cmd_init(&private_ts->sec, hx_commands, ARRAY_SIZE(hx_commands), SEC_CLASS_DEVT_TSP);
	if (ret < 0) {
		E("%s: Failed to sec_cmd_init\n", __func__);
		return -ENODEV;
	}else{
		I("%s: successful to sec_cmd_init\n", __func__);
	}
/*
	ret = sysfs_create_group(&private_ts->sec.fac_dev->kobj, &hx_cmd_attr_group);
	if (ret) {
		E( "%s: failed to create sysfs attributes\n", __func__);
		goto out;
	}else{
		I("%s: successful to create sysfs attributes\n", __func__);
	}*/
#endif
	I("%s: exit.\n", __func__);
	ts->probe_finish = true;

	himax_charger_notifier.notifier_call = himax_charger_notifier_callback;
	ret = power_supply_reg_notifier(&himax_charger_notifier);
	if (ret) {
		printk("fail to register notifer\n");
//		return ret;
	}

	return ret;

#if defined(HX_CONFIG_DRM_MTK)
err_get_intr_bit_failed:
#endif
err_common_init_failed:
	kfree(ts);
err_alloc_data_failed:
	kfree(g_xfer_data);
	g_xfer_data = NULL;
err_alloc_g_xfer_data_failed:

	return ret;
/*
#ifdef SEC_TSP_FACTORY_TEST
out:
	sec_cmd_exit(&private_ts->sec, SEC_CLASS_DEVT_TSP);
	return 0;
#endif
*/

}

static void himax_chip_common_remove(struct spi_device *spi)
{
	struct himax_ts_data *ts = spi_get_drvdata(spi);

		if (ts->probe_finish) {
#if defined(HX_CONFIG_DRM_MTK)
		if (mtk_disp_notifier_unregister(&ts->fb_notif))
			E("Error occurred while unregistering notifier.\n");
#endif
		power_supply_unreg_notifier(&himax_charger_notifier);
		himax_chip_common_deinit();
	}

	ts->spi = NULL;
	/* spin_unlock_irq(&ts->spi_lock); */
	spi_set_drvdata(spi, NULL);
	kfree(g_xfer_data);

#ifdef SEC_TSP_FACTORY_TEST
	//sysfs_remove_group(&private_ts->sec.fac_dev->kobj, &hx_cmd_attr_group);
	sec_cmd_exit(&private_ts->sec, SEC_CLASS_DEVT_TSP);
#endif

	I("%s: completed.\n", __func__);

//	return 0;
}

#if defined(HX_PATCH_COMERR_PM)
static int himax_common_pm_suspend(struct device *dev)
{
	struct himax_ts_data *ts = private_ts;
	I("system enters into pm_suspend");
	ts->pm_suspend = true;
	reinit_completion(&ts->pm_completion);
	return 0;
}

static int himax_common_pm_resume(struct device *dev)
{
	struct himax_ts_data *ts = private_ts;
	I("system resumes from pm_suspend");
	ts->pm_suspend = false;
	complete(&ts->pm_completion);
	return 0;
}
static const struct dev_pm_ops himax_common_pm_ops = {
	.suspend = himax_common_pm_suspend,
	.resume  = himax_common_pm_resume,
};
#endif

static const struct of_device_id himax_match_table[] = {
	{.compatible = "himax,common" },
	{},
};

static struct spi_driver himax_common_driver = {
	.probe =	himax_chip_common_probe,
	.remove =	himax_chip_common_remove,
	.driver = {
		.name =		HIMAX_common_NAME,
		.owner =	THIS_MODULE,
		.of_match_table = himax_match_table,
#if defined(HX_PATCH_COMERR_PM)
		.pm =	&himax_common_pm_ops,
#endif
	},

};
extern char wt_saved_command_line[COMMAND_LINE_SIZE];
static int __init himax_common_init(void)
{
	pr_err("Himax common touch panel driver init\n");
	D("Himax check double loading\n");

	if (strstr(wt_saved_command_line,"hx83112f_fhdp_wt_dsi_vdo_cphy_90hz_dsbj")) {
		t_lcm_name = 3;//
	}
	else if (strstr(wt_saved_command_line,"hx83112f_fhdp_wt_dsi_vdo_cphy_90hz_djn")) {
		t_lcm_name = 4;
	}
	else
	{
		pr_err("TP match hx83112f fail\n");
		return -1;
	}

	if (g_mmi_refcnt++ > 0) {
		I("Himax driver has been loaded! ignoring....\n");
		return 0;
	}
	spi_register_driver(&himax_common_driver);

	return 0;
}

static void __exit himax_common_exit(void)
{
/* 	if (spi) {
		spi_unregister_device(spi);
		spi = NULL;
	} */
	spi_unregister_driver(&himax_common_driver);

}

#if defined(__HIMAX_MOD__)
module_init(himax_common_init);
#else
late_initcall(himax_common_init);
#endif
module_exit(himax_common_exit);

MODULE_DESCRIPTION("Himax_common driver");
MODULE_LICENSE("GPL");
