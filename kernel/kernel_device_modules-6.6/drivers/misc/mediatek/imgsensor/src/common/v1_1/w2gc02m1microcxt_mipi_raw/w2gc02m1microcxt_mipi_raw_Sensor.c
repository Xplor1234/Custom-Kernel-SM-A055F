 /*
 *
 * Filename:
 * ---------
 *     W2GC02M1DEPSJ_mipi_raw_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *-----------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 */
#define PFX "w2gc02m1microcxt_camera_sensor"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "w2gc02m1microcxt_mipi_raw_Sensor.h"

#define MULTI_WRITE 1
#define IBURST_ENABLE 0
#define OTP_PORTING 1

#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = W2GC02M1MICROCXT_SENSOR_ID,
	.checksum_value = 0xf7375923,
	.pre = {
		.pclk = 84000000,
		.linelength = 2192,
		.framelength = 1276,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 67200000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 84000000,
		.linelength = 2192,
		.framelength = 1276,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 67200000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 84000000,
		.linelength = 2192,
		.framelength = 1276,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 67200000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 84000000,
		.linelength = 2192,
		.framelength = 1276,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 67200000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 84000000,
		.linelength = 2192,
		.framelength = 1276,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1600,
		.grabwindow_height = 1200,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 67200000,
		.max_framerate = 300,
	},

	.margin = 16,
	.min_shutter = 4,
	.min_gain = 64,/* 1x */
	.max_gain = 768,/* 12x */
	.min_gain_iso = 100,
	.exp_step = 1,
	.gain_step = 1,
	.gain_type = 4,
	.max_frame_length = 0x3fff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.temperature_support = 0,/* 1, support; 0,not support */
	.sensor_mode_num = 5,	/* support sensor mode num */
	.cap_delay_frame = 2,	/* enter capture delay frame num */
	.pre_delay_frame = 2,	/* enter preview delay frame num */
	.video_delay_frame = 2,	/* enter video delay frame num */
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,	/* enter slim video delay frame num */
	.frame_time_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_1_LANE,
	.i2c_addr_table = {0x6E, 0xff},
	/* record sensor support all write id addr,
	 * only supprt 4 must end with 0xff
	 */
	.i2c_speed = 400,
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter = 0x3ED,
	.gain = 0x40,
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x6E,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200}, /* Preview */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200}, /* capture */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200}, /* video */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200}, /* HS video */
	{1600, 1200, 0, 0, 1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 0, 0, 1600, 1200}  /* slim video */
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[1] = {(char)(addr & 0xff)};

	iReadRegI2C(pu_send_cmd, 1, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[2] = {(char)(addr & 0xff), (char)(para & 0xff)};
	iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
}
#if IBURST_ENABLE
#if MULTI_WRITE
#define I2C_BUFFER_LEN    510
#else
#define I2C_BUFFER_LEN    2
#endif

static void gc02m1_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend = 0, IDX = 0;
	kal_uint16 addr = 0, addr_last = 0, data;

	while (len > IDX) {
		addr = para[IDX];

		puSendCmd[tosend++] = (char)(addr & 0xff);
		data = para[IDX + 1];
		puSendCmd[tosend++] = (char)(data & 0xff);
		IDX += 2;
		addr_last = addr;

#if MULTI_WRITE
		if ((I2C_BUFFER_LEN - tosend) < 2 || IDX == len || addr != addr_last) {
			LOG_INF("burst write\n");
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id, 2,
						 imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2CTiming(puSendCmd, 2, imgsensor.i2c_write_id, imgsensor_info.i2c_speed);
		tosend = 0;
#endif
	}
}
#endif

static void set_dummy(void)
{
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x41, (imgsensor.frame_length >> 8) & 0x3f);
	write_cmos_sensor(0x42, imgsensor.frame_length & 0xff);
}

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0xf0) << 8) | read_cmos_sensor(0xf1));
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
		? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}

static void set_mirror_flip(kal_uint8 image_mirror)
{
}

static void set_shutter(kal_uint16 shutter)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;
    /* kal_uint32 frame_length = 0; */
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* if shutter bigger than frame_length, should extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

	if (imgsensor.autoflicker_en) {
		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else{
			write_cmos_sensor(0xfe, 0x00);
			write_cmos_sensor(0x41, (imgsensor.frame_length >> 8) & 0x3f);
			write_cmos_sensor(0x42, imgsensor.frame_length & 0xff);
		}
	} else{
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x41, (imgsensor.frame_length >> 8) & 0x3f);
		write_cmos_sensor(0x42, imgsensor.frame_length & 0xff);
	}
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x03, (shutter >> 8) & 0x3f);
	write_cmos_sensor(0x04, shutter  & 0xff);
	LOG_INF("shutter = %d, framelength = %d\n", shutter, imgsensor.frame_length);
}

/*************************************************************************
 * FUNCTION
 *	set_shutter_frame_length
 *
 * DESCRIPTION
 *	for frame & 3A sync
 *
 *************************************************************************/
static void set_shutter_frame_length(kal_uint32 shutter,
				     kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* if shutter bigger than frame_length, should extend frame length first*/
	spin_lock(&imgsensor_drv_lock);
	if (frame_length > 1)
		dummy_line = frame_length - imgsensor.frame_length;

	imgsensor.frame_length = imgsensor.frame_length + dummy_line;

	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter)
			? imgsensor_info.min_shutter : shutter;
	shutter =
	(shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin)
		: shutter;
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10
				/ imgsensor.frame_length;
	if (imgsensor.autoflicker_en) {
		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor(0xfe, 0x00);
			write_cmos_sensor(0x41, (imgsensor.frame_length >> 8) & 0x3f);
			write_cmos_sensor(0x42, imgsensor.frame_length & 0xff);
		}
	} else {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x41, (imgsensor.frame_length >> 8) & 0x3f);
		write_cmos_sensor(0x42, imgsensor.frame_length & 0xff);
	}

	/* Update Shutter */
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x03, (shutter >> 8) & 0x3f);
	write_cmos_sensor(0x04, shutter  & 0xff);
	pr_debug(
		"Exit! shutter =%d, framelength =%d/%d, dummy_line=%d,\n",
		shutter, imgsensor.frame_length, frame_length,
		dummy_line);

}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = gain << 4;

	if (reg_gain < GC02M1_SENSOR_GAIN_BASE)
		reg_gain = GC02M1_SENSOR_GAIN_BASE;
	else if (reg_gain > GC02M1_SENSOR_GAIN_MAX)
		reg_gain = GC02M1_SENSOR_GAIN_MAX;

	return (kal_uint16)reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;
	kal_uint32 temp_gain;
	kal_int16 gain_index;
	kal_uint16 GC02M1_AGC_Param[GC02M1_SENSOR_GAIN_MAX_VALID_INDEX][2] = {
		{  1024,  0 },
		{  1536,  1 },
		{  2035,  2 },
		{  2519,  3 },
		{  3165,  4 },
		{  3626,  5 },
		{  4147,  6 },
		{  4593,  7 },
		{  5095,  8 },
		{  5697,  9 },
		{  6270, 10 },
		{  6714, 11 },
		{  7210, 12 },
		{  7686, 13 },
		{  8214, 14 },
		{ 10337, 15 },
	};

	reg_gain = gain2reg(gain);

	for (gain_index = GC02M1_SENSOR_GAIN_MAX_VALID_INDEX - 1; gain_index >= 0; gain_index--)
		if (reg_gain >= GC02M1_AGC_Param[gain_index][0])
			break;

	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xb6, GC02M1_AGC_Param[gain_index][1]);
	temp_gain = reg_gain * GC02M1_SENSOR_DGAIN_BASE / GC02M1_AGC_Param[gain_index][0];
	write_cmos_sensor(0xb1, (temp_gain >> 8) & 0x1f);
	write_cmos_sensor(0xb2, temp_gain & 0xff);
	LOG_INF("GC02M1_AGC_Param[gain_index][1] = 0x%x, temp_gain = 0x%x, reg_gain = %d\n",
		GC02M1_AGC_Param[gain_index][1], temp_gain, reg_gain);

	return reg_gain;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x3e, 0x90);
	} else {
		write_cmos_sensor(0xfe, 0x00);
		write_cmos_sensor(0x3e, 0X00);
	}
	return ERROR_NONE;
}
#if IBURST_ENABLE
kal_uint16 addr_data_pair_init_gc02m1micro[] = {
	/*system*/
	0xfc, 0x01,
	0xf4, 0x41,
	0xf5, 0xe3, // c0->e3 For YH&YJ&YK&YC
	0xf6, 0x44,
	0xf8, 0x38,
	0xf9, 0x82,
	0xfa, 0x00,
	0xfd, 0x80,
	0xfc, 0x81,
	0xfe, 0x03,
	0x01, 0x0b,
	0xf7, 0x01,
	0xfc, 0x80,
	0xfc, 0x80,
	0xfc, 0x80,
	0xfc, 0x8e,

	/*CISCTL*/
	0xfe, 0x00,
	0x87, 0x09,
	0xee, 0x72,
	0xfe, 0x01,
	0x8c, 0x90,
	0xfe, 0x00,
	0x90, 0x00,
	0x03, 0x04,
	0x04, 0x7d,
	0x41, 0x04,
	0x42, 0xf4,
	0x05, 0x04,
	0x06, 0x48,
	0x07, 0x00,
	0x08, 0x18,
	0x9d, 0x18,
	0x09, 0x00,
	0x0a, 0x02,
	0x0d, 0x04,
	0x0e, 0xbc,
	0x17, GC02M1_MIRROR,
	0x19, 0x04,
	0x24, 0x00,
	0x56, 0x20,
	0x5b, 0x00,
	0x5e, 0x01,

	/*analog Register width*/
	0x21, 0x3c,
	0x44, 0x20,
	0xcc, 0x01,

	/*analog mode*/
	0x1a, 0x04,
	0x1f, 0x11,
	0x27, 0x30,
	0x2b, 0x00,
	0x33, 0x00,
	0x53, 0x90,
	0xe6, 0x50,

	/*analog voltage*/
	0x39, 0x07,
	0x43, 0x04,
	0x46, 0x4a,  // 2a->4a For YH&YJ&YK&YC
	0x7c, 0xa0,
	0xd0, 0xbe,
	0xd1, 0x60,
	0xd2, 0x40,
	0xd3, 0xf3,
	0xde, 0x1d,

	/*analog current*/
	0xcd, 0x05,
	0xce, 0x6f,

	/*CISCTL RESET*/
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfe, 0x00,
	0xfc, 0x88,
	0xfe, 0x10,
	0xfe, 0x00,
	0xfc, 0x8e,
	0xfe, 0x04,
	0xe0, 0x01,
	0xfe, 0x00,

	/*ISP*/
	0xfe, 0x01,
	0x53, 0x54,  // 44->54 For YH&YJ&YK&YC
	0x87, 0x53,
	0x89, 0x03,

	/*Gain*/
	0xfe, 0x00,
	0xb0, 0x74,
	0xb1, 0x04,
	0xb2, 0x00,
	0xb6, 0x00,
	0xfe, 0x04,
	0xd8, 0x00,
	0xc0, 0x40,
	0xc0, 0x00,
	0xc0, 0x00,
	0xc0, 0x00,
	0xc0, 0x60,
	0xc0, 0x00,
	0xc0, 0xc0,
	0xc0, 0x2a,
	0xc0, 0x80,
	0xc0, 0x00,
	0xc0, 0x00,
	0xc0, 0x40,
	0xc0, 0xa0,
	0xc0, 0x00,
	0xc0, 0x90,
	0xc0, 0x19,
	0xc0, 0xc0,
	0xc0, 0x00,
	0xc0, 0xD0,
	0xc0, 0x2F,
	0xc0, 0xe0,
	0xc0, 0x00,
	0xc0, 0x90,
	0xc0, 0x39,
	0xc0, 0x00,
	0xc0, 0x01,
	0xc0, 0x20,
	0xc0, 0x04,
	0xc0, 0x20,
	0xc0, 0x01,
	0xc0, 0xe0,
	0xc0, 0x0f,
	0xc0, 0x40,
	0xc0, 0x01,
	0xc0, 0xe0,
	0xc0, 0x1a,
	0xc0, 0x60,
	0xc0, 0x01,
	0xc0, 0x20,
	0xc0, 0x25,
	0xc0, 0x80,
	0xc0, 0x01,
	0xc0, 0xa0,
	0xc0, 0x2c,
	0xc0, 0xa0,
	0xc0, 0x01,
	0xc0, 0xe0,
	0xc0, 0x32,
	0xc0, 0xc0,
	0xc0, 0x01,
	0xc0, 0x20,
	0xc0, 0x38,
	0xc0, 0xe0,
	0xc0, 0x01,
	0xc0, 0x60,
	0xc0, 0x3c,
	0xc0, 0x00,
	0xc0, 0x02,
	0xc0, 0xa0,
	0xc0, 0x40,
	0xc0, 0x80,
	0xc0, 0x02,
	0xc0, 0x18,
	0xc0, 0x5c,
	0xfe, 0x00,
	0x9f, 0x10,

	/*BLK*/
	0xfe, 0x00,
	0x26, 0x20,
	0xfe, 0x01,
	0x40, 0x22,
	0x46, 0x7f,
	0x49, 0x0f,
	0x4a, 0xf0,
	0xfe, 0x04,
	0x14, 0x80,
	0x15, 0x80,
	0x16, 0x80,
	0x17, 0x80,

	/*ant _blooming*/
	0xfe, 0x01,
	0x41, 0x20,
	0x4c, 0x00,
	0x4d, 0x0c,
	0x44, 0x08,
	0x48, 0x03,

	/*Window 1600X1200*/
	0xfe, 0x01,
	0x90, 0x01,
	0x91, 0x00,
	0x92, 0x06,
	0x93, 0x00,
	0x94, 0x06,
	0x95, 0x04,
	0x96, 0xb0,
	0x97, 0x06,
	0x98, 0x40,

	/*mipi*/
	0xfe, 0x03,
	0x01, 0x23,
	0x03, 0xce,
	0x04, 0x48,
	0x15, 0x00,
	0x21, 0x10,
	0x22, 0x05,
	0x23, 0x20,
	0x25, 0x20,
	0x26, 0x08,
	0x29, 0x06,
	0x2a, 0x0a,
	0x2b, 0x08,

	/*out*/
	0xfe, 0x01,
	0x8c, 0x10,
	0xfe, 0x00,
	0x3e, 0x00,
};

kal_uint16 addr_data_pair_preview_gc02m1micro[] = {
	0xfe, 0x00,
};

kal_uint16 addr_data_pair_capture_gc02m1micro[] = {
	0xfe, 0x00,
};

kal_uint16 addr_data_pair_normal_video_gc02m1micro[] = {
	0xfe, 0x00,
};

kal_uint16 addr_data_pair_hs_video_gc02m1micro[] = {
	0xfe, 0x00,
};

kal_uint16 addr_data_pair_slim_video_gc02m1micro[] = {
	0xfe, 0x00,
};
#endif

static void sensor_init(void)
{
	LOG_INF("E\n");
#if IBURST_ENABLE
	gc02m1_table_write_cmos_sensor(
		addr_data_pair_init_gc02m1micro,
		sizeof(addr_data_pair_init_gc02m1micro) /
		sizeof(kal_uint16));
#else
	/*system*/
	write_cmos_sensor(0xfc, 0x01);
	write_cmos_sensor(0xf4, 0x41);
	write_cmos_sensor(0xf5, 0xe3); // c0->e3 For YH&YJ&YK&YC
	write_cmos_sensor(0xf6, 0x44);
	write_cmos_sensor(0xf8, 0x38);
	write_cmos_sensor(0xf9, 0x82);
	write_cmos_sensor(0xfa, 0x00);
	write_cmos_sensor(0xfd, 0x80);
	write_cmos_sensor(0xfc, 0x81);
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x01, 0x0b);
	write_cmos_sensor(0xf7, 0x01);
	write_cmos_sensor(0xfc, 0x80);
	write_cmos_sensor(0xfc, 0x80);
	write_cmos_sensor(0xfc, 0x80);
	write_cmos_sensor(0xfc, 0x8e);

	/*CISCTL*/
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x87, 0x09);
	write_cmos_sensor(0xee, 0x72);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x90);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x90, 0x00);
	write_cmos_sensor(0x03, 0x04);
	write_cmos_sensor(0x04, 0x7d);
	write_cmos_sensor(0x41, 0x04);
	write_cmos_sensor(0x42, 0xf4);
	write_cmos_sensor(0x05, 0x04);
	write_cmos_sensor(0x06, 0x48);
	write_cmos_sensor(0x07, 0x00);
	write_cmos_sensor(0x08, 0x18);
	write_cmos_sensor(0x9d, 0x18);
	write_cmos_sensor(0x09, 0x00);
	write_cmos_sensor(0x0a, 0x02);
	write_cmos_sensor(0x0d, 0x04);
	write_cmos_sensor(0x0e, 0xbc);
	write_cmos_sensor(0x17, GC02M1_MIRROR); //1_MIRROR,
	write_cmos_sensor(0x19, 0x04);
	write_cmos_sensor(0x24, 0x00);
	write_cmos_sensor(0x56, 0x20);
	write_cmos_sensor(0x5b, 0x00);
	write_cmos_sensor(0x5e, 0x01);

	/*analog Register width*/
	write_cmos_sensor(0x21, 0x3c);
	write_cmos_sensor(0x44, 0x20);
	write_cmos_sensor(0xcc, 0x01);

	/*analog mode*/
	write_cmos_sensor(0x1a, 0x04);
	write_cmos_sensor(0x1f, 0x11);
	write_cmos_sensor(0x27, 0x30);
	write_cmos_sensor(0x2b, 0x00);
	write_cmos_sensor(0x33, 0x00);
	write_cmos_sensor(0x53, 0x90);
	write_cmos_sensor(0xe6, 0x50);

	/*analog voltage*/
	write_cmos_sensor(0x39, 0x07);
	write_cmos_sensor(0x43, 0x04);
	write_cmos_sensor(0x46, 0x4a);  // 2a->4a For YH&YJ&YK&YC
	write_cmos_sensor(0x7c, 0xa0);
	write_cmos_sensor(0xd0, 0xbe);
	write_cmos_sensor(0xd1, 0x60);
	write_cmos_sensor(0xd2, 0x40);
	write_cmos_sensor(0xd3, 0xf3);
	write_cmos_sensor(0xde, 0x1d);

	/*analog current*/
	write_cmos_sensor(0xcd, 0x05);
	write_cmos_sensor(0xce, 0x6f);

	/*CISCTL RESET*/
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x88);
	write_cmos_sensor(0xfe, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x04);
	write_cmos_sensor(0xe0, 0x01);
	write_cmos_sensor(0xfe, 0x00);

	/*ISP*/
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x53, 0x54);  // 44->54 For YH&YJ&YK&YC
	write_cmos_sensor(0x87, 0x53);
	write_cmos_sensor(0x89, 0x03);

	/*Gain*/
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xb0, 0x74);
	write_cmos_sensor(0xb1, 0x04);
	write_cmos_sensor(0xb2, 0x00);
	write_cmos_sensor(0xb6, 0x00);
	write_cmos_sensor(0xfe, 0x04);
	write_cmos_sensor(0xd8, 0x00);
	write_cmos_sensor(0xc0, 0x40);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xc0, 0x60);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xc0, 0xc0);
	write_cmos_sensor(0xc0, 0x2a);
	write_cmos_sensor(0xc0, 0x80);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xc0, 0x40);
	write_cmos_sensor(0xc0, 0xa0);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xc0, 0x90);
	write_cmos_sensor(0xc0, 0x19);
	write_cmos_sensor(0xc0, 0xc0);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xc0, 0xD0);
	write_cmos_sensor(0xc0, 0x2F);
	write_cmos_sensor(0xc0, 0xe0);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xc0, 0x90);
	write_cmos_sensor(0xc0, 0x39);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xc0, 0x01);
	write_cmos_sensor(0xc0, 0x20);
	write_cmos_sensor(0xc0, 0x04);
	write_cmos_sensor(0xc0, 0x20);
	write_cmos_sensor(0xc0, 0x01);
	write_cmos_sensor(0xc0, 0xe0);
	write_cmos_sensor(0xc0, 0x0f);
	write_cmos_sensor(0xc0, 0x40);
	write_cmos_sensor(0xc0, 0x01);
	write_cmos_sensor(0xc0, 0xe0);
	write_cmos_sensor(0xc0, 0x1a);
	write_cmos_sensor(0xc0, 0x60);
	write_cmos_sensor(0xc0, 0x01);
	write_cmos_sensor(0xc0, 0x20);
	write_cmos_sensor(0xc0, 0x25);
	write_cmos_sensor(0xc0, 0x80);
	write_cmos_sensor(0xc0, 0x01);
	write_cmos_sensor(0xc0, 0xa0);
	write_cmos_sensor(0xc0, 0x2c);
	write_cmos_sensor(0xc0, 0xa0);
	write_cmos_sensor(0xc0, 0x01);
	write_cmos_sensor(0xc0, 0xe0);
	write_cmos_sensor(0xc0, 0x32);
	write_cmos_sensor(0xc0, 0xc0);
	write_cmos_sensor(0xc0, 0x01);
	write_cmos_sensor(0xc0, 0x20);
	write_cmos_sensor(0xc0, 0x38);
	write_cmos_sensor(0xc0, 0xe0);
	write_cmos_sensor(0xc0, 0x01);
	write_cmos_sensor(0xc0, 0x60);
	write_cmos_sensor(0xc0, 0x3c);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xc0, 0x02);
	write_cmos_sensor(0xc0, 0xa0);
	write_cmos_sensor(0xc0, 0x40);
	write_cmos_sensor(0xc0, 0x80);
	write_cmos_sensor(0xc0, 0x02);
	write_cmos_sensor(0xc0, 0x18);
	write_cmos_sensor(0xc0, 0x5c);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x9f, 0x10);

	/*BLK*/
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x26, 0x20);
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x40, 0x22);
	write_cmos_sensor(0x46, 0x7f);
	write_cmos_sensor(0x49, 0x0f);
	write_cmos_sensor(0x4a, 0xf0);
	write_cmos_sensor(0xfe, 0x04);
	write_cmos_sensor(0x14, 0x80);
	write_cmos_sensor(0x15, 0x80);
	write_cmos_sensor(0x16, 0x80);
	write_cmos_sensor(0x17, 0x80);

	/*ant _blog ming*/
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x41, 0x20);
	write_cmos_sensor(0x4c, 0x00);
	write_cmos_sensor(0x4d, 0x0c);
	write_cmos_sensor(0x44, 0x08);
	write_cmos_sensor(0x48, 0x03);

	/*Window 1600X1200*/
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x90, 0x01);
	write_cmos_sensor(0x91, 0x00);
	write_cmos_sensor(0x92, 0x06);
	write_cmos_sensor(0x93, 0x00);
	write_cmos_sensor(0x94, 0x06);
	write_cmos_sensor(0x95, 0x04);
	write_cmos_sensor(0x96, 0xb0);
	write_cmos_sensor(0x97, 0x06);
	write_cmos_sensor(0x98, 0x40);

	/*mipi*/
	write_cmos_sensor(0xfe, 0x03);
	write_cmos_sensor(0x01, 0x23);
	write_cmos_sensor(0x03, 0xce);
	write_cmos_sensor(0x04, 0x48);
	write_cmos_sensor(0x15, 0x00);
	write_cmos_sensor(0x21, 0x10);
	write_cmos_sensor(0x22, 0x05);
	write_cmos_sensor(0x23, 0x20);
	write_cmos_sensor(0x25, 0x20);
	write_cmos_sensor(0x26, 0x08);
	write_cmos_sensor(0x29, 0x06);
	write_cmos_sensor(0x2a, 0x0a);
	write_cmos_sensor(0x2b, 0x08);

	/*out*/
	write_cmos_sensor(0xfe, 0x01);
	write_cmos_sensor(0x8c, 0x10);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x3e, 0x00);
#endif
	LOG_INF( "<< sensor_init()\n" );
}

static void preview_setting(void)
{
	LOG_INF("E\n");
#if IBURST_ENABLE
	gc02m1_table_write_cmos_sensor(
		addr_data_pair_preview_gc02m1micro,
		sizeof(addr_data_pair_preview_gc02m1micro) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0xfe, 0x00);
#endif
}

static void capture_setting(void)
{
	LOG_INF("E\n");
#if IBURST_ENABLE
	gc02m1_table_write_cmos_sensor(
		addr_data_pair_capture_gc02m1micro,
		sizeof(addr_data_pair_capture_gc02m1micro) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0xfe, 0x00);
#endif
}

static void normal_video_setting(void)
{
	LOG_INF("E\n");
#if IBURST_ENABLE
	gc02m1_table_write_cmos_sensor(
		addr_data_pair_normal_video_gc02m1micro,
		sizeof(addr_data_pair_normal_video_gc02m1micro) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0xfe, 0x00);
#endif
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
#if IBURST_ENABLE
	gc02m1_table_write_cmos_sensor(
		addr_data_pair_hs_video_gc02m1micro,
		sizeof(addr_data_pair_hs_video_gc02m1micro) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0xfe, 0x00);
#endif
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
#if IBURST_ENABLE
	gc02m1_table_write_cmos_sensor(
		addr_data_pair_slim_video_gc02m1micro,
		sizeof(addr_data_pair_slim_video_gc02m1micro) /
		sizeof(kal_uint16));
#else
	write_cmos_sensor(0xfe, 0x00);
#endif
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0x8c, 0x11);
		write_cmos_sensor(0xfe, 0x00);
	} else {
		write_cmos_sensor(0xfe, 0x01);
		write_cmos_sensor(0x8c, 0x10);
		write_cmos_sensor(0xfe, 0x00);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

#if OTP_PORTING
//+bug767771, chenming.wt, ADD, 2022/07/13, add gc02m1 otp bringup.
#include "cam_cal_define.h"
#include <linux/slab.h>
static struct stCAM_CAL_DATAINFO_STRUCT w2gc02m1microcxt_eeprom_data ={
	.sensorID= W2GC02M1MICROCXT_SENSOR_ID,
	.deviceID = 0x10,
	.dataLength = 0x0778,
	.sensorVendorid = 0x16020000,
	.vendorByte = {1,2,3,4},
	.dataBuffer = NULL,
};
static struct stCAM_CAL_CHECKSUM_STRUCT w2gc02m1microcxt_checksum[6] =
{
	{MODULE_ITEM,0x0000,0x0000,0x0007,0x0008,0x55},
	{SN_DATA,0x0009,0x0009,0x0015,0x0016,0x55},
	{AWB_ITEM,0x0017,0x0017,0x0027,0x0028,0x55},
	{LSC_ITEM,0x0029,0x0029,0x0775,0x0776,0x55},
	{TOTAL_ITEM,0x0000,0x0000,0x0776,0x0777,0x55},
	{MAX_ITEM,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0x55},  // this line must haved
};

extern bool imgsensor_set_eeprom_data_bywing(struct stCAM_CAL_DATAINFO_STRUCT* pData, struct stCAM_CAL_CHECKSUM_STRUCT* checksumData);


#define EEPROM_FM24C64S_ID 0xA4

static kal_uint16 read_eeprom(kal_uint32 addr)
{
	kal_uint16 get_byte=0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, EEPROM_FM24C64S_ID);

	return get_byte;
}
//-bug767771, chenming.wt, ADD, 2022/07/13, add gc02m1 otp bringup.
#endif

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 5;


	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id() + 1;

			if (*sensor_id == imgsensor_info.sensor_id) {
#if OTP_PORTING
				//+bug767771, chenming.wt, ADD, 2022/07/13, add gc02m1 otp bringup.
				imgsensor_set_eeprom_data_bywing(&w2gc02m1microcxt_eeprom_data,w2gc02m1microcxt_checksum);
				if((w2gc02m1microcxt_eeprom_data.sensorVendorid >> 24)!= read_eeprom(0x0001)) {
					if(w2gc02m1microcxt_eeprom_data.dataBuffer != NULL) {
						kfree(w2gc02m1microcxt_eeprom_data.dataBuffer);
						w2gc02m1microcxt_eeprom_data.dataBuffer = NULL;
					}
					pr_err("cwd w2gc02m1microcxt get eeprom data failed, dataLength = %d, w2gc02m1microcxt_eeprom_data.sensorVendorid >> 24 = %d, read_eeprom(0x0001) = %d\n",w2gc02m1microcxt_eeprom_data.dataLength,(w2gc02m1microcxt_eeprom_data.sensorVendorid >> 24),read_eeprom(0x0001));
					*sensor_id = 0xFFFFFFFF;
					return ERROR_SENSOR_CONNECT_FAIL;
				} else {
					LOG_INF("cwd w2gc02m1microcxt get eeprom data success\n");
				}
				//-bug767771, chenming.wt, ADD, 2022/07/13, add gc02m1 otp bringup.
#endif
				LOG_INF("GC02M1(YH&YJ&YK&YC) i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, I2C write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);

			retry--;
		} while (retry > 0);
		i++;
		retry = 5;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
        LOG_INF("w2gc02m1microcxt Read id fail,sensor id: 0x%x\n", *sensor_id);
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 5;
	kal_uint32 sensor_id = 0;

	LOG_INF("%s +\n", __func__);
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id() + 1;
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}
			 LOG_INF("Read sensor id fail, id: 0x%x\n", sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
	    retry = 5;
	}

	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3ED;
	imgsensor.gain = 0x40;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;

	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 close(void)
{
	LOG_INF("E\n");
	/* No Need to implement this function */
	streaming_control(KAL_FALSE);
	return ERROR_NONE;
}

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
		LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;

	spin_unlock(&imgsensor_drv_lock);
	capture_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;
	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;
	sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;
	sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;
	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; /* inverse with datasheet */
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	/*This Function not used after ROME*/
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	/***********
	 *if (framerate == 0)	 //Dynamic frame rate
	 *	return ERROR_NONE;
	 *spin_lock(&imgsensor_drv_lock);
	 *if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
	 *	imgsensor.current_fps = 296;
	 *else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
	 *	imgsensor.current_fps = 146;
	 *else
	 *	imgsensor.current_fps = framerate;
	 *spin_unlock(&imgsensor_drv_lock);
	 *set_max_framerate(imgsensor.current_fps, 1);
	 ********/
	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else /* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ?
			(frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				framerate, imgsensor_info.cap.max_framerate / 10);
		frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ?
			(frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ?
			(frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		break;
	default:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		if (imgsensor.frame_length > imgsensor.shutter)
			set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	default:
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/* pr_debug("feature_id = %d\n", feature_id); */
	switch (feature_id) {
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		*(feature_data + 2) = imgsensor_info.exp_step;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		 set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		 /* night_mode((BOOL) *feature_data); */
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr,
				    sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/*get the lens driver ID from EEPROM
		 * or just return LENS_DRIVER_ID_DO_NOT_CARE
		 * if EEPROM does not exist in camera module.
		 */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16,
				      *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		 set_max_framerate_by_scenario(
				(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
				*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		 get_default_framerate_by_scenario(
				(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
				(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	//+bug727089, liangyiyi.wt,MODIFY, 2022.10.27, fix KASAN opened to Monitor memory, camera  memory access error.
	case SENSOR_FEATURE_SET_FRAMERATE:
		pr_debug("current fps :%d\n", *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		pr_debug("ihdr enable :%d\n", (BOOL)*feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (BOOL)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	//-bug727089, liangyiyi.wt,MODIFY, 2022.10.27, fix KASAN opened to Monitor memory, camera  memory access error.
	case SENSOR_FEATURE_GET_CROP_INFO:
		pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32)*feature_data);
		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[3],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[4],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;

		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[0],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length((UINT16) (*feature_data),
					(UINT16) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * 1, if driver support new sw frame sync
		 * set_shutter_frame_length() support third para auto_extend_en
		 */
		*(feature_data + 1) = 1;
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		pr_debug("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
		#if 0
		ihdr_write_shutter((UINT16)*feature_data,
				   (UINT16)*(feature_data+1));
		#endif
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(*feature_data);
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		*feature_return_para_32 = 1; /* BINNING_AVERAGED */
		pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
	case SENSOR_FEATURE_SET_LSC_TBL:
	default:
		break;
	}

	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 W2GC02M1MICROCXT_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
