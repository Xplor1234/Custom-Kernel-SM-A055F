/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     w2s5kjn1rearst_mipi_Sensor.c
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
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "cam_cal_define.h"
#include <linux/slab.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "w2s5kjn1rearst_mipiraw_Sensor.h"
#include "w2s5kjn1rearst_mipiraw_Sensor_setting.h"
#include "w2s5kjn1rearst_mipiraw_otp.h"
//#include "imgsensor_common.h"

#define VENDOR_EDIT 1

/***************Modify Following Strings for Debug**********************/
#define PFX "w2s5kjn1rearst_camera_sensor"
#define LOG_1 LOG_INF("w2s5kjn1rearst,MIPI 4LANE\n")
/****************************   Modify end    **************************/
#define LOG_INF(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)

#define DEVICE_VERSION_S5KJN103     "w2s5kjn1rearst"
extern void register_imgsensor_deviceinfo(char *name, char *version, u8 module_id);
static kal_uint32 streaming_control(kal_bool enable);

#define MODULE_ID_OFFSET 0x0000
#define I2C_BUFFER_LEN 225    /* trans# max is 255, each 3 bytes */
#define W2_TXD_S5KJN1_OTP 1
#define USE_BURST_MODE 1
#define JN1_HW_GGC_SIZE 173
#define JN1_HW_GGC_START_ADDR 0x3027

#define TRULY_S5KJN1_OTP_XTC_OFFSET   0x0D5E
#define TRULY_S5KJN1_OTP_SENSORXTC_OFFSET   0x1B0E
#define TRULY_S5KJN1_OTP_PDXTC_OFFSET   0x1E10
#define TRULY_S5KJN1_OTP_SWGCC_OFFSET   0x2DB2

#define S5KJN1_OTP_XTC_LEN   3502
#define S5KJN1_OTP_SENSORXTC_LEN   768
#define S5KJN1_OTP_PDXTC_LEN   4000
#define S5KJN1_OTP_SWGCC_LEN   626
#define S5KJN1_OTP_XTC_TOTAL_LEN   9490
#define EEPROM_BL24SA64D_ID 0xA0


kal_uint16 s5kst_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len);
static bool bNeedSetNormalMode = KAL_FALSE;
static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
    .sensor_id = W2S5KJN1REARST_SENSOR_ID,
    .module_id = 0x19,
    .checksum_value = 0x8ac2d94a,

    .pre = {
        .pclk = 600000000,
        .linelength = 5616,
        .framelength = 3560,
        .startx =0,
        .starty = 0,
        .grabwindow_width = 2040,
        .grabwindow_height = 1536,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
        .mipi_pixel_rate = 320000000,
    },

    .cap = {
        .pclk = 560000000,
        .linelength = 5096,
        .framelength = 3654,
        .startx =0,
        .starty = 0,
        .grabwindow_width = 4080,
        .grabwindow_height = 3072,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
        .mipi_pixel_rate = 672000000,
    },

    .normal_video = { /*4080*2296@30fps*/
        .pclk = 560000000,
        .linelength = 5888,
        .framelength = 3168,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 4080,
        .grabwindow_height = 2296,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 588800000,
        .max_framerate = 300,
    },

    .hs_video = { /* 2000x1132@120fps */
        .pclk = 600000000,
        .linelength = 4160,
        .framelength = 1200,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2000,
        .grabwindow_height = 1132,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 432000000,
        .max_framerate = 1200,
    },

    .slim_video = { /* 1280x720@120fps */
        .pclk = 600000000,
        .linelength = 2096,
        .framelength = 2384,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1280,
        .grabwindow_height = 720,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 547200000,
        .max_framerate = 1200,
    },

    .custom1 = {
        .pclk = 560000000,
        .linelength = 6678,
        .framelength = 3494,
        .startx =0,
        .starty = 0,
        .grabwindow_width = 4080,
        .grabwindow_height = 3072,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 240,
        .mipi_pixel_rate = 494400000,
    },

    .custom2 = {
        .pclk = 560000000,
        .linelength = 8688,
        .framelength = 6400,
        .startx =0,
        .starty = 0,
        .grabwindow_width = 8160,
        .grabwindow_height = 6144,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 100,
        .mipi_pixel_rate = 556800000,
    },

    .custom3 = {
        .pclk = 560000000,
        .linelength = 4584,
        .framelength = 4054,
        .startx =0,
        .starty = 0,
        .grabwindow_width = 4080,
        .grabwindow_height = 3072,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
        .mipi_pixel_rate = 662400000,
    },

    .margin = 16,//10,        /* sensor framelength & shutter margin */
    .min_shutter = 4,    /* min shutter */

    .min_gain = 64,
    .max_gain = 4096,
    .min_gain_iso = 100,
    .gain_step = 2,
    .gain_type = 2, //0-SONY; 1-OV; 2 - SUMSUN; 3 -HYNIX; 4 -GC

    .max_frame_length = 0xfffd,
    .ae_shut_delay_frame = 0,
    .ae_sensor_gain_delay_frame = 0,
    .ae_ispGain_delay_frame = 2,    /* isp gain delay frame for AE cycle */
    .ihdr_support = 0,    /* 1, support; 0,not support */
    .ihdr_le_firstline = 0,    /* 1,le first ; 0, se first */
    .sensor_mode_num = 8,    /* support sensor mode num */

    .cap_delay_frame = 1,    /* enter capture delay frame num */
    .pre_delay_frame = 1,    /* enter preview delay frame num */
    .video_delay_frame = 1,    /* enter video delay frame num */
    .hs_video_delay_frame = 3,
    .slim_video_delay_frame = 3,    /* enter slim video delay frame num */
    .custom1_delay_frame = 2,    /* enter custom1 delay frame num */
    .custom2_delay_frame = 1,    /* enter custom2 delay frame num */
    .custom3_delay_frame = 1,    /* enter custom3 delay frame num */
    .frame_time_delay_frame = 1,

    .isp_driving_current = ISP_DRIVING_4MA,
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2, /* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2 */
    .mipi_settle_delay_mode = 1,
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_Gb,
    .mclk = 24, /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
    .mipi_lane_num = SENSOR_MIPI_4_LANE,
    .i2c_addr_table = {0x7A, 0xff},
    .i2c_speed = 1000, /* i2c read/write speed */
};

static struct imgsensor_struct imgsensor = {
    .mirror = IMAGE_HV_MIRROR,    /* NORMAL information */
    .sensor_mode = IMGSENSOR_MODE_INIT,
    .shutter = 0x3D0,    /* current shutter */
    .gain = 0x100,        /* current gain */
    .dummy_pixel = 0,    /* current dummypixel */
    .dummy_line = 0,    /* current dummyline */
    .current_fps = 300,
    .autoflicker_en = KAL_FALSE,
    .test_pattern = KAL_FALSE,
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
    .ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
    .i2c_write_id = 0x20, /* record current sensor's i2c write id */
    .current_ae_effective_frame = 2,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[8] = {
    {8160, 6144,   0,    0,   8160, 6144, 2040, 1536,  0,   0,   2040, 1536,    0,    0, 2040, 1536}, /* Preview*/
    {8160, 6144,   0,    0,   8160, 6144, 4080, 3072,  0,   0,   4080, 3072,    0,    0, 4080, 3072}, /* capture */
    {8160, 6144,   0,    0,   8160, 6144, 4080, 3072,  0,   388, 4080, 2296,    0,    0, 4080, 2296}, /* normal video */
    {8160, 6144,   0,    0,   8160, 6144, 2040, 1536,  20, 202, 2000, 1132,     0,    0, 2000,  1132}, /* hs_video */
    {8160, 6144,   0,    0,   8160, 6144, 2040, 1536,  380, 408, 1280, 720,     0,    0, 1280,  720}, /* slim video */
    {8160, 6144,   0,    0,   8160, 6144, 4080, 3072,  0,   0,   4080, 3072,    0,    0, 4080, 3072}, /* custom1 */
    {8160, 6144,   0,    0,   8160, 6144, 8160, 6144,  0,   0,   8160, 6144,    0,    0, 8160, 6144}, /* custom2 */
    {8160, 6144,   0,    0,   8160, 6144, 4080, 3072,  0,   0,   4080, 3072,    0,    0, 4080, 3072}, /* custom3 */
};

/*VC1 for HDR(DT=0X35), VC2 for PDAF(DT=0X30), unit : 10bit */
static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[2] = {
    /* Capture mode setting */
    /* Custom1 mode setting */
    {0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
     0x00, 0x2B, 0x0FF0, 0x0C00, 0x01, 0x00, 0x0000, 0x0000,
     0x01, 0x30, 0x027C, 0x0BF0, 0x03, 0x00, 0x0000, 0x0000},
    /* Video mode setting */
    {0x02, 0x0A,   0x00,   0x08, 0x40, 0x00,
     0x00, 0x2B, 0x0FF0, 0x08F8, 0x01, 0x00, 0x0000, 0x0000,
     0x01, 0x30, 0x027C, 0x08E8, 0x03, 0x00, 0x0000, 0x0000}
};

/* If mirror flip */
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
    .i4OffsetX =  8,
    .i4OffsetY =  8,
    .i4PitchX  =  8,
    .i4PitchY  =  8,
    .i4PairNum =  4,
    .i4SubBlkW =  8,
    .i4SubBlkH =  2,
    .i4PosL = {{11, 8},{9, 11},{13, 12},{15, 15}},
    .i4PosR = {{10, 8},{8, 11},{12, 12},{14, 15}},
    .i4BlockNumX = 508,
    .i4BlockNumY = 382,
    .iMirrorFlip = 0,
    .i4Crop = { {0,0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
};
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_4080_2296 =
{
    .i4OffsetX = 8,
    .i4OffsetY = 8,
    .i4PitchX  = 8,
    .i4PitchY  = 8,
    .i4PairNum = 4,
    .i4SubBlkW = 8,
    .i4SubBlkH = 2,
    .i4PosL = {{11, 8},{9, 11},{13, 12},{15, 15}},
    .i4PosR = {{10, 8},{8, 11},{12, 12},{14, 15}},
    .i4BlockNumX = 508,
    .i4BlockNumY = 285,
    .iMirrorFlip = 0,
    .i4Crop = { {0,0}, {0, 0}, {0, 388}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
};

#if 0
static kal_uint16 read_module_id(void)
{
    kal_uint16 get_byte=0;
    char pusendcmd[2] = {(char)(MODULE_ID_OFFSET >> 8) , (char)(MODULE_ID_OFFSET & 0xFF) };
    iReadRegI2C(pusendcmd , 2, (u8*)&get_byte,1,0xA0/*EEPROM_READ_ID*/);
    if (get_byte == 0) {
        iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, 0xA0/*EEPROM_READ_ID*/);
    }
    return get_byte;

}
#endif
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte = 0;
    char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF)};

    iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 2, imgsensor.i2c_write_id);
    return ((get_byte<<8)&0xff00) | ((get_byte>>8)&0x00ff);
}

void s5krst_write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
    char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF),
                 (char)(para >> 8), (char)(para & 0xFF)};

    /*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/
    /* Add this func to set i2c speed by each sensor */
    iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}


static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
    kal_uint16 get_byte = 0;
    char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

    iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
    return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
    char pusendcmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF),
            (char)(para & 0xFF)};

    iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}
#if 0
#define  CAMERA_MODULE_INFO_LENGTH  (8)
static kal_uint8 CAM_SN[CAMERA_MODULE_SN_LENGTH];
static kal_uint8 CAM_INFO[CAMERA_MODULE_INFO_LENGTH];
static kal_uint8 CAM_DUAL_DATA[DUALCAM_CALI_DATA_LENGTH_8ALIGN];
/*Henry.Chang@Camera.Driver add for google ARCode Feature verify 20190531*/
static kal_uint16 read_cmos_eeprom_8(kal_uint16 addr)
{
    kal_uint16 get_byte=0;
    char pusendcmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    iReadRegI2C(pusendcmd , 2, (u8*)&get_byte, 1, 0xA0);
    return get_byte;
}

static void read_eepromData(void)
{
    kal_uint16 idx = 0;
    for (idx = 0; idx <DUALCAM_CALI_DATA_LENGTH; idx++) {
        CAM_DUAL_DATA[idx] = read_cmos_eeprom_8(S5KJN103_STEREO_START_ADDR + idx);
    }
    for (idx = 0; idx <CAMERA_MODULE_SN_LENGTH; idx++) {
        CAM_SN[idx] = read_cmos_eeprom_8(0xB0 + idx);
        LOG_INF("CAM_SN[%d]: 0x%x  0x%x\n", idx, CAM_SN[idx]);
    }
    CAM_INFO[0] = read_cmos_eeprom_8(0x0);
    CAM_INFO[1] = read_cmos_eeprom_8(0x1);
    CAM_INFO[2] = read_cmos_eeprom_8(0x6);
    CAM_INFO[3] = read_cmos_eeprom_8(0x7);
    CAM_INFO[4] = read_cmos_eeprom_8(0x8);
    CAM_INFO[5] = read_cmos_eeprom_8(0x9);
    CAM_INFO[6] = read_cmos_eeprom_8(0xA);
    CAM_INFO[7] = read_cmos_eeprom_8(0xB);
}

#define   WRITE_DATA_MAX_LENGTH     (16)
static kal_int32 table_write_eeprom_30Bytes(kal_uint16 addr, kal_uint8 *para, kal_uint32 len)
{
    kal_int32 ret = IMGSENSOR_RETURN_SUCCESS;
    char pusendcmd[WRITE_DATA_MAX_LENGTH+2];
    pusendcmd[0] = (char)(addr >> 8);
    pusendcmd[1] = (char)(addr & 0xFF);

    memcpy(&pusendcmd[2], para, len);

    ret = iBurstWriteReg((kal_uint8 *)pusendcmd , (len + 2), 0xA0);

    return ret;
}

static kal_int32 write_eeprom_protect(kal_uint16 enable)
{
    kal_int32 ret = IMGSENSOR_RETURN_SUCCESS;
    char pusendcmd[3];
    pusendcmd[0] = 0x80;
    pusendcmd[1] = 0x00;
    if (enable)
        pusendcmd[2] = 0x0E;
    else
        pusendcmd[2] = 0x00;

    ret = iBurstWriteReg((kal_uint8 *)pusendcmd , 3, 0xA0);
    return ret;
}

static kal_int32 write_Module_data(ACDK_SENSOR_ENGMODE_STEREO_STRUCT * pStereodata)
{
    kal_int32  ret = IMGSENSOR_RETURN_SUCCESS;
    kal_uint16 data_base, data_length;
    kal_uint32 idx, idy;
    kal_uint8 *pData;
    UINT32 i = 0;
    if(pStereodata != NULL) {
        LOG_INF("SET_SENSOR_OTP: 0x%x %d 0x%x %d\n",
                       pStereodata->uSensorId,
                       pStereodata->uDeviceId,
                       pStereodata->baseAddr,
                       pStereodata->dataLength);

        data_base = pStereodata->baseAddr;
        data_length = pStereodata->dataLength;
        pData = pStereodata->uData;
        if ((pStereodata->uSensorId == W2_S5KJN1_REAR_TXD_SENSOR_ID) && (data_length == DUALCAM_CALI_DATA_LENGTH)
            && (data_base == S5KJN103_STEREO_START_ADDR)) {
            LOG_INF("Write: %x %x %x %x %x %x %x %x\n", pData[0], pData[39], pData[40], pData[1556],
                    pData[1557], pData[1558], pData[1559], pData[1560]);
            idx = data_length/WRITE_DATA_MAX_LENGTH;
            idy = data_length%WRITE_DATA_MAX_LENGTH;
            /* close write protect */
            write_eeprom_protect(0);
            msleep(6);
            for (i = 0; i < idx; i++ ) {
                ret = table_write_eeprom_30Bytes((data_base+WRITE_DATA_MAX_LENGTH*i),
                        &pData[WRITE_DATA_MAX_LENGTH*i], WRITE_DATA_MAX_LENGTH);
                if (ret != IMGSENSOR_RETURN_SUCCESS) {
                    LOG_ERR("write_eeprom error: i= %d\n", i);
                    /* open write protect */
                    write_eeprom_protect(1);
                    msleep(6);
                    return IMGSENSOR_RETURN_ERROR;
                }
                msleep(6);
            }
            ret = table_write_eeprom_30Bytes((data_base+WRITE_DATA_MAX_LENGTH*idx),
                    &pData[WRITE_DATA_MAX_LENGTH*idx], idy);
            if (ret != IMGSENSOR_RETURN_SUCCESS) {
                LOG_ERR("write_eeprom error: idx= %d idy= %d\n", idx, idy);
                /* open write protect */
                write_eeprom_protect(1);
                msleep(6);
                return IMGSENSOR_RETURN_ERROR;
            }
            msleep(6);
            /* open write protect */
            write_eeprom_protect(1);
            msleep(6);
            LOG_INF("com_0:0x%x\n", read_cmos_eeprom_8(S5KJN103_STEREO_START_ADDR));
            LOG_INF("com_39:0x%x\n", read_cmos_eeprom_8(S5KJN103_STEREO_START_ADDR+39));
            LOG_INF("innal_40:0x%x\n", read_cmos_eeprom_8(S5KJN103_STEREO_START_ADDR+40));
            LOG_INF("innal_1556:0x%x\n", read_cmos_eeprom_8(S5KJN103_STEREO_START_ADDR+1556));
            LOG_INF("tail1_1557:0x%x\n", read_cmos_eeprom_8(S5KJN103_STEREO_START_ADDR+1557));
            LOG_INF("tail2_1558:0x%x\n", read_cmos_eeprom_8(S5KJN103_STEREO_START_ADDR+1558));
            LOG_INF("tail3_1559:0x%x\n", read_cmos_eeprom_8(S5KJN103_STEREO_START_ADDR+1559));
            LOG_INF("tail4_1560:0x%x\n", read_cmos_eeprom_8(S5KJN103_STEREO_START_ADDR+1560));
            LOG_INF("write_Module_data Write end\n");
        } else {
            LOG_ERR("Invalid Sensor id:0x%x write eeprom\n", pStereodata->uSensorId);
            return IMGSENSOR_RETURN_ERROR;
        }
    } else {
        LOG_ERR("s5kgh1 write_Module_data pStereodata is null\n");
        return IMGSENSOR_RETURN_ERROR;
    }
    return ret;
}
#endif


static void set_dummy(void)
{
    s5krst_write_cmos_sensor(0x0340, imgsensor.frame_length);
    s5krst_write_cmos_sensor(0x0342, imgsensor.line_length);
    LOG_INF("dummyline = %d, dummypixels = %d \n",
        imgsensor.dummy_line, imgsensor.dummy_pixel);
}    /*    set_dummy  */

static void set_mirror_flip(kal_uint8 image_mirror)
{
    kal_uint8 itemp;
    LOG_INF("image_mirror = %d\n", image_mirror);
    itemp=read_cmos_sensor(0x0101);
    LOG_INF("image_mirror itemp = %d\n", itemp);
    itemp &= ~0x03;

    switch(image_mirror) {
        case IMAGE_NORMAL:
            write_cmos_sensor_8(0x0101, itemp);
            break;

        case IMAGE_V_MIRROR:
            write_cmos_sensor_8(0x0101, itemp | 0x02);
            break;

        case IMAGE_H_MIRROR:
            write_cmos_sensor_8(0x0101, itemp | 0x01);
            break;

        case IMAGE_HV_MIRROR:
            write_cmos_sensor_8(0x0101, itemp | 0x03);
            break;
    }
}

static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
    kal_uint32 frame_length = imgsensor.frame_length;

    LOG_INF("framerate = %d, min framelength should enable %d\n", framerate,
        min_framelength_en);

    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
    spin_lock(&imgsensor_drv_lock);
    if (frame_length >= imgsensor.min_frame_length) {
        imgsensor.frame_length = frame_length;
    } else {
        imgsensor.frame_length = imgsensor.min_frame_length;
    }
    imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
        imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    }
    if (min_framelength_en) {
        imgsensor.min_frame_length = imgsensor.frame_length;
    }
    spin_unlock(&imgsensor_drv_lock);
    set_dummy();
}    /*    set_max_framerate  */

static void write_shutter(kal_uint32 shutter)
{
    kal_uint16 realtime_fps = 0;
    kal_uint64 CintR = 0;
    kal_uint64 Time_Farme = 0;

    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin) {
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    } else {
        imgsensor.frame_length = imgsensor.min_frame_length;
    }
    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    }
    spin_unlock(&imgsensor_drv_lock);
    if (shutter < imgsensor_info.min_shutter) {
        shutter = imgsensor_info.min_shutter;
    }

    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if (realtime_fps >= 297 && realtime_fps <= 305) {
            set_max_framerate(296,0);
        } else if (realtime_fps >= 147 && realtime_fps <= 150) {
            set_max_framerate(146,0);
        } else {
            // Extend frame length
            s5krst_write_cmos_sensor(0x0340, imgsensor.frame_length);
        }
    } else {
        // Extend frame length
        s5krst_write_cmos_sensor(0x0340, imgsensor.frame_length);
    }

    if (shutter >= 0xFFF0) {  // need to modify line_length & PCLK
        bNeedSetNormalMode = KAL_TRUE;

        if (shutter >= 3448275) {  //>32s
            shutter = 3448275;
        }

        CintR = ( (unsigned long long)shutter) / 128;
        Time_Farme = CintR + 0x0002;  // 1st framelength
      //  LOG_INF("CintR =%d \n", CintR);

        s5krst_write_cmos_sensor(0x0340, Time_Farme & 0xFFFF);  // Framelength
        s5krst_write_cmos_sensor(0x0202, CintR & 0xFFFF);  //shutter
        s5krst_write_cmos_sensor(0x0702, 0x0700);
        s5krst_write_cmos_sensor(0x0704, 0x0700);
    } else {
        if (bNeedSetNormalMode) {
            LOG_INF("exit long shutter\n");
            s5krst_write_cmos_sensor(0x0702, 0x0000);
            s5krst_write_cmos_sensor(0x0704, 0x0000);
            bNeedSetNormalMode = KAL_FALSE;
        }

        s5krst_write_cmos_sensor(0x0340, imgsensor.frame_length);
        s5krst_write_cmos_sensor(0x0202, imgsensor.shutter);
    }
    LOG_INF("shutter =%d, framelength =%d \n", shutter,imgsensor.frame_length);
}    /*    write_shutter  */

/*************************************************************************
 * FUNCTION
 *    set_shutter
 *
 * DESCRIPTION
 *    This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *    iShutter : exposured lines
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint32 shutter)
{
    unsigned long flags;

    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    write_shutter(shutter);
} /* set_shutter */


/*************************************************************************
 * FUNCTION
 *    set_shutter_frame_length
 *
 * DESCRIPTION
 *    for frame & 3A sync
 *
 *************************************************************************/
static void set_shutter_frame_length(kal_uint32 shutter,
                     kal_uint32 frame_length,
                     kal_bool auto_extend_en)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;
    kal_int32 dummy_line = 0;
    kal_uint64 CintR = 0;
    kal_uint64 Time_Farme = 0;

    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    spin_lock(&imgsensor_drv_lock);
    /* Change frame time */
    if (frame_length > 1)
        dummy_line = frame_length - imgsensor.frame_length;

    imgsensor.frame_length = imgsensor.frame_length + dummy_line;

    if (shutter > imgsensor.frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);
    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;

    //set_dummy();
    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if (realtime_fps >= 297 && realtime_fps <= 305) {
            set_max_framerate(296, 0);
        } else if (realtime_fps >= 147 && realtime_fps <= 150) {
            set_max_framerate(146, 0);
        } else {
            /* Extend frame length */
            s5krst_write_cmos_sensor(0x0340, imgsensor.frame_length);
        }
    } else {
        /* Extend frame length */
        s5krst_write_cmos_sensor(0x0340, imgsensor.frame_length);
    }
    LOG_INF("shutter =%d \n", shutter);

    if (shutter >= 0xFFF0) {
        bNeedSetNormalMode = KAL_TRUE;

        if (shutter >= 1538000) {
            shutter = 1538000;
        }
        CintR = (5013 * (unsigned long long)shutter) / 321536;
        Time_Farme = CintR + 0x0002;
     //   LOG_INF("CintR = %d, Time_Farme = %d\n", CintR,Time_Farme);

        s5krst_write_cmos_sensor(0x0340, Time_Farme & 0xFFFF);
        s5krst_write_cmos_sensor(0x0202, CintR & 0xFFFF);
        s5krst_write_cmos_sensor(0x0702, 0x0600);
        s5krst_write_cmos_sensor(0x0704, 0x0600);
    } else {
        if (bNeedSetNormalMode) {
            LOG_INF("exit long shutter\n");
            s5krst_write_cmos_sensor(0x0702, 0x0000);
            s5krst_write_cmos_sensor(0x0704, 0x0000);
            bNeedSetNormalMode = KAL_FALSE;
        }

        s5krst_write_cmos_sensor(0x0340, imgsensor.frame_length);
        s5krst_write_cmos_sensor(0x0202, imgsensor.shutter);
    }

    LOG_INF("Exit! shutter =%d, framelength =%d/%d, dummy_line=%d, auto_extend=%d\n",
        shutter, imgsensor.frame_length, frame_length, dummy_line, read_cmos_sensor(0x0350));
}    /* set_shutter_frame_length */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
     kal_uint16 reg_gain = 0x0;

    reg_gain = gain/2;
    return (kal_uint16)reg_gain;
}

/*************************************************************************
 * FUNCTION
 *    set_gain
 *
 * DESCRIPTION
 *    This function is to set global gain to sensor.
 *
 * PARAMETERS
 *    iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *    the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_gain;

    if (gain < BASEGAIN || gain > 64 * BASEGAIN) {
        LOG_INF("Error gain setting");
        if (gain < BASEGAIN) {
            gain = BASEGAIN;
        } else if (gain > 64 * BASEGAIN) {
            gain = 64 * BASEGAIN;
        }
    }

    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

    write_cmos_sensor_8(0x0204, (reg_gain >> 8));
    write_cmos_sensor_8(0x0205, (reg_gain & 0xff));

    return gain;
}    /*    set_gain  */



static void w2s5kjn1rearst03_set_lsc_reg_setting(
        kal_uint8 index, kal_uint16 *regDa, MUINT32 regNum)
{
    int i;
    int startAddr[4] = {0x9D88, 0x9CB0, 0x9BD8, 0x9B00};
    /*0:B,1:Gb,2:Gr,3:R*/

    LOG_INF("E! index:%d, regNum:%d\n", index, regNum);

    write_cmos_sensor_8(0x0B00, 0x01); /*lsc enable*/
    write_cmos_sensor_8(0x9014, 0x01);
    write_cmos_sensor_8(0x4439, 0x01);
    mdelay(1);
    LOG_INF("Addr 0xB870, 0x380D Value:0x%x %x\n",
        read_cmos_sensor_8(0xB870), read_cmos_sensor_8(0x380D));
    /*define Knot point, 2'b01:u3.7*/
    write_cmos_sensor_8(0x9750, 0x01);
    write_cmos_sensor_8(0x9751, 0x01);
    write_cmos_sensor_8(0x9752, 0x01);
    write_cmos_sensor_8(0x9753, 0x01);

    for (i = 0; i < regNum; i++)
        s5krst_write_cmos_sensor(startAddr[index] + 2*i, regDa[i]);

    write_cmos_sensor_8(0x0B00, 0x00); /*lsc disable*/
}
/*************************************************************************
 * FUNCTION
 *    night_mode
 *
 * DESCRIPTION
 *    This function night mode of sensor.
 *
 * PARAMETERS
 *    bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 streaming_control(kal_bool enable)
{
    int timeout = (10000 / imgsensor.current_fps) + 1;
    int i = 0;
    int framecnt = 0;

    LOG_INF("streaming_enable(0= Sw Standby,1= streaming): %d\n", enable);
    if (enable) {
        write_cmos_sensor_8(0x0100, 0x01);
        mDELAY(10);
    } else {
        write_cmos_sensor_8(0x0100, 0x00);
        for (i = 0; i < timeout; i++) {
            mDELAY(5);
            framecnt = read_cmos_sensor_8(0x0005);
            if (framecnt == 0xFF) {
                LOG_INF(" Stream Off OK at i=%d.\n", i);
                return ERROR_NONE;
            }
        }
        LOG_INF("Stream Off Fail! framecnt= %d.\n", framecnt);
    }
    return ERROR_NONE;
}

kal_uint16 s5kst_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
    char puSendCmd[I2C_BUFFER_LEN];
    kal_uint32 tosend, IDX;
    kal_uint16 addr = 0, addr_last = 0, data;

    tosend = 0;
    IDX = 0;

    while (len > IDX) {
        addr = para[IDX];
        {
            puSendCmd[tosend++] = (char)(addr >> 8);
            puSendCmd[tosend++] = (char)(addr & 0xFF);
            data = para[IDX + 1];
            puSendCmd[tosend++] = (char)(data >> 8);
            puSendCmd[tosend++] = (char)(data & 0xFF);
            IDX += 2;
            addr_last = addr;

        }
        if ((I2C_BUFFER_LEN - tosend) < 4
            || IDX == len || addr != addr_last) {
            iBurstWriteReg_multi(puSendCmd,
                        tosend,
                        imgsensor.i2c_write_id,
                        4,
                        imgsensor_info.i2c_speed);
            tosend = 0;
        }
    }
    return 0;
}


static void sensor_init(void)
{
    LOG_INF("sensor_init start\n");
    s5krst_write_cmos_sensor(0x6028, 0x4000);
    s5krst_write_cmos_sensor(0x0000, 0x0003);
    s5krst_write_cmos_sensor(0x0000, 0x38E1);
    s5krst_write_cmos_sensor(0x001E, 0x0007);
    s5krst_write_cmos_sensor(0x6028, 0x4000);
    s5krst_write_cmos_sensor(0x6010, 0x0001);
    mdelay(5);  //delay 5ms
    s5krst_write_cmos_sensor(0x6226, 0x0001);
    mdelay(10);  //delay 10ms
    s5kst_table_write_cmos_sensor(w2s5kjn1rearst03_init_setting,
        sizeof(w2s5kjn1rearst03_init_setting) / sizeof(kal_uint16));
    set_mirror_flip(imgsensor.mirror);
    LOG_INF("sensor_init End\n");
}    /*      sensor_init  */

static void preview_setting(void)
{
    LOG_INF("preview_setting Start\n");
    s5kst_table_write_cmos_sensor(w2s5kjn1rearst03_preview_setting,
        sizeof(w2s5kjn1rearst03_preview_setting)/sizeof(kal_uint16));
    LOG_INF("preview_setting End\n");
} /* preview_setting */


/*full size 30fps*/
static void capture_setting(kal_uint16 currefps)
{
    LOG_INF("%s 30 fps E! currefps:%d\n", __func__, currefps);
    //Deng.Cao@Cam.Drv, change the capture setting to preview setting, 20191220 start.
    s5kst_table_write_cmos_sensor(w2s5kjn1rearst03_capture_setting,
        sizeof(w2s5kjn1rearst03_capture_setting)/sizeof(kal_uint16));
    //Deng.Cao@Cam.Drv, change the capture setting to preview setting, 20191220 end.
    LOG_INF("%s 30 fpsX\n", __func__);
}

static void normal_video_setting(kal_uint16 currefps)
{
    LOG_INF("%s E! currefps:%d\n", __func__, currefps);
    s5kst_table_write_cmos_sensor(w2s5kjn1rearst03_normal_video_setting,
        sizeof(w2s5kjn1rearst03_normal_video_setting)/sizeof(kal_uint16));
    LOG_INF("X\n");
}

static void hs_video_setting(void)
{
    LOG_INF("%s E! currefps 120\n", __func__);
    s5kst_table_write_cmos_sensor(w2s5kjn1rearst03_hs_video_setting,
        sizeof(w2s5kjn1rearst03_hs_video_setting)/sizeof(kal_uint16));
    LOG_INF("X\n");
}

static void slim_video_setting(void)
{
    LOG_INF("%s E! 4608*2592@30fps\n", __func__);
    s5kst_table_write_cmos_sensor(w2s5kjn1rearst03_slim_video_setting,
        sizeof(w2s5kjn1rearst03_slim_video_setting)/sizeof(kal_uint16));
    LOG_INF("X\n");
}

/*full size 10fps*/

static void custom1_setting(void)
{
    LOG_INF("%s CUS1_12.5M_30_FPS E! currefps\n", __func__);
    /*************MIPI output setting************/
    s5kst_table_write_cmos_sensor(w2s5kjn1rearst03_custom1_setting,
        sizeof(w2s5kjn1rearst03_custom1_setting)/sizeof(kal_uint16));
    LOG_INF("X");
}

static void custom2_setting(void)
{
    LOG_INF("%s CUS2_50M_10_FPS E! currefps\n", __func__);
    /*************MIPI output setting************/
    s5kst_table_write_cmos_sensor(w2s5kjn1rearst03_custom2_setting,
        sizeof(w2s5kjn1rearst03_custom2_setting)/sizeof(kal_uint16));
    LOG_INF("X");
}

static void custom3_setting(void)
{
    LOG_INF("%s CUS3_12.5M_30_FPS E! currefps\n", __func__);
    /*************MIPI output setting************/
    s5kst_table_write_cmos_sensor(w2s5kjn1rearst03_custom3_setting,
        sizeof(w2s5kjn1rearst03_custom3_setting)/sizeof(kal_uint16));
    LOG_INF("X");
}
/*************************************************************************
 * FUNCTION
 *    get_imgsensor_id
 *
 * DESCRIPTION
 *    This function get the sensor ID
 *
 * PARAMETERS
 *    *sensorID : return the sensor ID
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
   // kal_int32 size = 0;
    /*sensor have two i2c address 0x34 & 0x20,
     *we should detect the module used i2c address
     */
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            *sensor_id = ((read_cmos_sensor_8(0x0000) << 8)
                    | read_cmos_sensor_8(0x0001)) + 1;
            LOG_INF(
                "read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x *sensor_id:0x%x\n",
                read_cmos_sensor_8(0x0000),
                read_cmos_sensor_8(0x0001),
                read_cmos_sensor(0x0000), *sensor_id);
            if (*sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",imgsensor.i2c_write_id, *sensor_id);
				int ret;
				ret = s5kjn1_init_sensor_cali_info();
				if (ret == ERROR_SENSOR_CONNECT_FAIL)
					return ERROR_SENSOR_CONNECT_FAIL;
				else
                    return ERROR_NONE;
            }

            LOG_INF("Read sensor id fail, id: 0x%x *sensor_id:0x%x\n", imgsensor.i2c_write_id, *sensor_id);
            retry--;
        } while (retry > 0);
        i++;
        retry = 2;
    }
    if (*sensor_id != imgsensor_info.sensor_id) {
        /*if Sensor ID is not correct,
         *Must set *sensor_id to 0xFFFFFFFF
         */
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 *    open
 *
 * DESCRIPTION
 *    This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint16 sensor_id = 0;

    LOG_INF("open start\n");
    /*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
     *we should detect the module used i2c address
     */
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = ((read_cmos_sensor_8(0x0000) << 8) | read_cmos_sensor_8(0x0001)) + 1;
            LOG_INF("Read sensor id [0x0000~1]: 0x%x, sensor_id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
            if (sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
                    imgsensor.i2c_write_id, sensor_id);
                break;
            }
            LOG_INF("Read sensor id fail, write id: 0x%x, sensor_id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
            retry--;
        } while (retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }
    if (imgsensor_info.sensor_id != sensor_id)
        return ERROR_SENSOR_CONNECT_FAIL;

    /* initail sequence write in  */
    sensor_init();
    #if W2_TXD_S5KJN1_OTP
    write_sensor_HW_GGC();
    #endif
    spin_lock(&imgsensor_drv_lock);

    imgsensor.autoflicker_en = KAL_FALSE;
    imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
    imgsensor.shutter = 0x3D0;
    imgsensor.gain = 0x100;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.dummy_pixel = 0;
    imgsensor.dummy_line = 0;
    imgsensor.ihdr_mode = 0;
    imgsensor.test_pattern = KAL_FALSE;
    imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("open End\n");
    return ERROR_NONE;
} /* open */

/*************************************************************************
 * FUNCTION
 *    close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
    LOG_INF("E\n");
    /* No Need to implement this function */
    streaming_control(KAL_FALSE);
    return ERROR_NONE;
} /* close */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *    This function start the sensor preview.
 *
 * PARAMETERS
 *    *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("%s E\n", __func__);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);

    preview_setting();
    set_mirror_flip(imgsensor.mirror);
    return ERROR_NONE;
} /* preview */

/*************************************************************************
 * FUNCTION
 *    capture
 *
 * DESCRIPTION
 *    This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

    if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
        LOG_INF(
            "Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
            imgsensor.current_fps,
            imgsensor_info.cap.max_framerate / 10);
    imgsensor.pclk = imgsensor_info.cap.pclk;
    imgsensor.line_length = imgsensor_info.cap.linelength;
    imgsensor.frame_length = imgsensor_info.cap.framelength;
    imgsensor.min_frame_length = imgsensor_info.cap.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;

    spin_unlock(&imgsensor_drv_lock);
    capture_setting(imgsensor.current_fps);
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}    /* capture() */
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
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    normal_video_setting(imgsensor.current_fps);
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}    /*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    /*imgsensor.video_mode = KAL_TRUE;*/
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    /*imgsensor.current_fps = 300;*/
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting();
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("%s. 720P@240FPS\n", __func__);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    /*imgsensor.video_mode = KAL_TRUE;*/
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    /*imgsensor.current_fps = 300;*/
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting();
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}    /* slim_video */


static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("%s.\n", __func__);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
    imgsensor.pclk = imgsensor_info.custom1.pclk;
    imgsensor.line_length = imgsensor_info.custom1.linelength;
    imgsensor.frame_length = imgsensor_info.custom1.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom1_setting();

    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}    /* custom1 */

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("%s.\n", __func__);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
    imgsensor.pclk = imgsensor_info.custom2.pclk;
    imgsensor.line_length = imgsensor_info.custom2.linelength;
    imgsensor.frame_length = imgsensor_info.custom2.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom2_setting();

    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}    /* custom2 */

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("%s.\n", __func__);

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
    imgsensor.pclk = imgsensor_info.custom3.pclk;
    imgsensor.line_length = imgsensor_info.custom3.linelength;
    imgsensor.frame_length = imgsensor_info.custom3.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    custom3_setting();

    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}    /* custom3 */

static kal_uint32
get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    LOG_INF("E\n");
    sensor_resolution->SensorFullWidth =
        imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight =
        imgsensor_info.cap.grabwindow_height;

    sensor_resolution->SensorPreviewWidth =
        imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight =
        imgsensor_info.pre.grabwindow_height;

    sensor_resolution->SensorVideoWidth =
        imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight =
        imgsensor_info.normal_video.grabwindow_height;

    sensor_resolution->SensorHighSpeedVideoWidth =
        imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight =
        imgsensor_info.hs_video.grabwindow_height;

    sensor_resolution->SensorSlimVideoWidth =
        imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight =
        imgsensor_info.slim_video.grabwindow_height;

    sensor_resolution->SensorCustom1Width =
        imgsensor_info.custom1.grabwindow_width;
    sensor_resolution->SensorCustom1Height =
        imgsensor_info.custom1.grabwindow_height;

    sensor_resolution->SensorCustom2Width =
        imgsensor_info.custom2.grabwindow_width;
    sensor_resolution->SensorCustom2Height =
        imgsensor_info.custom2.grabwindow_height;

    sensor_resolution->SensorCustom3Width =
        imgsensor_info.custom3.grabwindow_width;
    sensor_resolution->SensorCustom3Height =
        imgsensor_info.custom3.grabwindow_height;
        return ERROR_NONE;
} /* get_resolution */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
               MSDK_SENSOR_INFO_STRUCT *sensor_info,
               MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);

    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */

    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
    sensor_info->SensorOutputDataFormat =
        imgsensor_info.sensor_output_dataformat;

    sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
    sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
    sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
    sensor_info->HighSpeedVideoDelayFrame =
        imgsensor_info.hs_video_delay_frame;
    sensor_info->SlimVideoDelayFrame =
        imgsensor_info.slim_video_delay_frame;
    sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
    sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
    sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
    sensor_info->AESensorGainDelayFrame =
        imgsensor_info.ae_sensor_gain_delay_frame;
    sensor_info->AEISPGainDelayFrame =
        imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
    sensor_info->PDAF_Support = 2;
    sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
    sensor_info->SensorClockFreq = imgsensor_info.mclk;
    sensor_info->SensorClockDividCount = 3; /* not use */
    sensor_info->SensorClockRisingCount = 0;
    sensor_info->SensorClockFallingCount = 2; /* not use */
    sensor_info->SensorPixelClockCount = 3; /* not use */
    sensor_info->SensorDataLatchCount = 2; /* not use */

    sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->SensorWidthSampling = 0; /* 0 is default 1x */
    sensor_info->SensorHightSampling = 0; /* 0 is default 1x */
    sensor_info->SensorPacketECCOrder = 1;

    sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;

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

        sensor_info->SensorGrabStartX =
            imgsensor_info.normal_video.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.normal_video.starty;

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
        sensor_info->SensorGrabStartX =
            imgsensor_info.slim_video.startx;
        sensor_info->SensorGrabStartY =
            imgsensor_info.slim_video.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

        break;

    case MSDK_SCENARIO_ID_CUSTOM1:
        sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
        break;

    case MSDK_SCENARIO_ID_CUSTOM2:
        sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
        break;

    case MSDK_SCENARIO_ID_CUSTOM3:
        sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
        break;

        default:
        sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
        break;
    }

    return ERROR_NONE;
}    /*    get_info  */


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
    case MSDK_SCENARIO_ID_CUSTOM1:
        custom1(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_CUSTOM2:
        custom2(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_CUSTOM3:
        custom3(image_window, sensor_config_data);
        break;
    default:
        LOG_INF("Error ScenarioId setting");
        preview(image_window, sensor_config_data);
        return ERROR_INVALID_SCENARIO_ID;
    }

    return ERROR_NONE;
}    /* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
    LOG_INF("framerate = %d\n ", framerate);
    /* SetVideoMode Function should fix framerate */
    if (framerate == 0)
        /* Dynamic frame rate */
        return ERROR_NONE;
    spin_lock(&imgsensor_drv_lock);
    if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 296;
    else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 146;
    else
        imgsensor.current_fps = framerate;
    spin_unlock(&imgsensor_drv_lock);
    set_max_framerate(imgsensor.current_fps, 1);

    return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
    LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
    spin_lock(&imgsensor_drv_lock);
    if (enable) /*enable auto flicker*/
        imgsensor.autoflicker_en = KAL_TRUE;
    else /*Cancel Auto flick*/
        imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
        enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    kal_uint32 frame_length;

    LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        frame_length = imgsensor_info.pre.pclk / framerate * 10
                / imgsensor_info.pre.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.pre.framelength)
        ? (frame_length - imgsensor_info.pre.framelength) : 0;
        imgsensor.frame_length =
            imgsensor_info.pre.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        if (framerate == 0)
            return ERROR_NONE;
        frame_length = imgsensor_info.normal_video.pclk /
                framerate * 10 /
                imgsensor_info.normal_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.normal_video.framelength)
        ? (frame_length - imgsensor_info.normal_video.framelength)
        : 0;
        imgsensor.frame_length =
            imgsensor_info.normal_video.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
            LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n"
                    , framerate, imgsensor_info.cap.max_framerate/10);
        frame_length = imgsensor_info.cap.pclk / framerate * 10
                / imgsensor_info.cap.linelength;
        spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line =
            (frame_length > imgsensor_info.cap.framelength)
              ? (frame_length - imgsensor_info.cap.framelength) : 0;
            imgsensor.frame_length =
                imgsensor_info.cap.framelength
                + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        frame_length = imgsensor_info.hs_video.pclk / framerate * 10
                / imgsensor_info.hs_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.hs_video.framelength)
              ? (frame_length - imgsensor_info.hs_video.framelength)
              : 0;
        imgsensor.frame_length =
            imgsensor_info.hs_video.framelength
                + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        frame_length = imgsensor_info.slim_video.pclk / framerate * 10
            / imgsensor_info.slim_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.slim_video.framelength)
            ? (frame_length - imgsensor_info.slim_video.framelength)
            : 0;
        imgsensor.frame_length =
            imgsensor_info.slim_video.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_CUSTOM1:
        frame_length = imgsensor_info.custom1.pclk / framerate * 10
                / imgsensor_info.custom1.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.custom1.framelength)
            ? (frame_length - imgsensor_info.custom1.framelength)
            : 0;
        imgsensor.frame_length =
            imgsensor_info.custom1.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_CUSTOM2:
        frame_length = imgsensor_info.custom2.pclk / framerate * 10
                / imgsensor_info.custom2.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.custom2.framelength)
            ? (frame_length - imgsensor_info.custom2.framelength)
            : 0;
        imgsensor.frame_length =
            imgsensor_info.custom2.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    case MSDK_SCENARIO_ID_CUSTOM3:
        frame_length = imgsensor_info.custom3.pclk / framerate * 10
                / imgsensor_info.custom3.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.custom3.framelength)
            ? (frame_length - imgsensor_info.custom3.framelength)
            : 0;
        imgsensor.frame_length =
            imgsensor_info.custom3.framelength
            + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        break;
    default:  /*coding with  preview scenario by default*/
        frame_length = imgsensor_info.pre.pclk / framerate * 10
            / imgsensor_info.pre.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line =
            (frame_length > imgsensor_info.pre.framelength)
            ? (frame_length - imgsensor_info.pre.framelength) : 0;
        imgsensor.frame_length =
            imgsensor_info.pre.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (imgsensor.frame_length > imgsensor.shutter)
            set_dummy();
        LOG_INF("error scenario_id = %d, we use preview scenario\n",
            scenario_id);
        break;
    }
    return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
        enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
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
    case MSDK_SCENARIO_ID_CUSTOM1:
        *framerate = imgsensor_info.custom1.max_framerate;
        break;
    case MSDK_SCENARIO_ID_CUSTOM2:
        *framerate = imgsensor_info.custom2.max_framerate;
        break;
    case MSDK_SCENARIO_ID_CUSTOM3:
        *framerate = imgsensor_info.custom3.max_framerate;
        break;
    default:
        break;
    }

    return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    LOG_INF("enable: %d\n", enable);
    if (enable) {
        s5krst_write_cmos_sensor(0x0600, 0x0001);
        s5krst_write_cmos_sensor(0x0602, 0x0000);
        s5krst_write_cmos_sensor(0x0604, 0x0000);
        s5krst_write_cmos_sensor(0x0606, 0x0000);
        s5krst_write_cmos_sensor(0x0608, 0x0000);
    } else {
        s5krst_write_cmos_sensor(0x0600, 0x0000);
    }

    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
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
    struct SET_PD_BLOCK_INFO_T *PDAFinfo;
    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    struct SENSOR_VC_INFO_STRUCT *pvcinfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
        = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

    /*LOG_INF("feature_id = %d\n", feature_id);*/
    switch (feature_id) {
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
        break;
    #if 0
    case SENSOR_FEATURE_GET_EEPROM_DATA:
        LOG_INF("SENSOR_FEATURE_GET_EEPROM_DATA:%d\n", *feature_para_len);
        memcpy(&feature_para[0], CAM_DUAL_DATA, DUALCAM_CALI_DATA_LENGTH_8ALIGN);
        break;
    case SENSOR_FEATURE_GET_MODULE_INFO:
        LOG_INF("GET_MODULE_CamInfo:%d %d\n", *feature_para_len, *feature_data_32);
        *(feature_data_32 + 1) = (CAM_INFO[1] << 24)
                    | (CAM_INFO[0] << 16)
                    | (CAM_INFO[3] << 8)
                    | (CAM_INFO[2] & 0xFF);
        *(feature_data_32 + 2) = (CAM_INFO[5] << 24)
                    | (CAM_INFO[4] << 16)
                    | (CAM_INFO[7] << 8)
                    | (CAM_INFO[6] & 0xFF);
        break;
    case SENSOR_FEATURE_GET_MODULE_SN:
        LOG_INF("GET_MODULE_SN:%d %d\n", *feature_para_len, *feature_data_32);
        if (*feature_data_32 < CAMERA_MODULE_SN_LENGTH/4)
            *(feature_data_32 + 1) = (CAM_SN[4*(*feature_data_32) + 3] << 24)
                        | (CAM_SN[4*(*feature_data_32) + 2] << 16)
                        | (CAM_SN[4*(*feature_data_32) + 1] << 8)
                        | (CAM_SN[4*(*feature_data_32)] & 0xFF);
        break;
    case SENSOR_FEATURE_SET_SENSOR_OTP:
    {
        kal_int32 ret = IMGSENSOR_RETURN_SUCCESS;
        LOG_INF("SENSOR_FEATURE_SET_SENSOR_OTP length :%d\n", (UINT32)*feature_para_len);
        ret = write_Module_data((ACDK_SENSOR_ENGMODE_STEREO_STRUCT *)(feature_para));
        if (ret == ERROR_NONE)
            return ERROR_NONE;
        else
            return ERROR_MSDK_IS_ACTIVED;
    }
        break;
    #endif
    //+bug767771, liangyiyi.wt, ADD, 2022/10/7, modify for fixed sensor_fusion of its test fail
    case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
        *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 2892000;
        break;
    //-bug767771, liangyiyi.wt, ADD, 2022/10/7, modify for fixed sensor_fusion of its test fail
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
        case MSDK_SCENARIO_ID_CUSTOM1:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.custom1.pclk;
                break;
        case MSDK_SCENARIO_ID_CUSTOM2:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.custom2.pclk;
                break;
        case MSDK_SCENARIO_ID_CUSTOM3:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = imgsensor_info.custom3.pclk;
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
        case MSDK_SCENARIO_ID_CUSTOM1:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.custom1.framelength << 16)
                                 + imgsensor_info.custom1.linelength;
                break;
        case MSDK_SCENARIO_ID_CUSTOM2:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.custom2.framelength << 16)
                                 + imgsensor_info.custom2.linelength;
                break;
        case MSDK_SCENARIO_ID_CUSTOM3:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                        = (imgsensor_info.custom3.framelength << 16)
                                 + imgsensor_info.custom3.linelength;
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
    case SENSOR_FEATURE_SET_GAIN:
        set_gain((UINT16) *feature_data);
        break;
    case SENSOR_FEATURE_SET_REGISTER:
        write_cmos_sensor_8(sensor_reg_data->RegAddr,
                    sensor_reg_data->RegData);
        break;
    case SENSOR_FEATURE_GET_REGISTER:
        sensor_reg_data->RegData =
            read_cmos_sensor_8(sensor_reg_data->RegAddr);
        break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
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
    case SENSOR_FEATURE_GET_PDAF_DATA:
        LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
        break;
    case SENSOR_FEATURE_GET_4CELL_DATA:/*get 4 cell data from eeprom*/
        {
            int type = (kal_uint16)(*feature_data);
            char *data = (char *)(uintptr_t)(*(feature_data+1));
            int size = (kal_uint16)(*(feature_data+2));
            LOG_INF("read Cross type:%d,datasize:%d",type,size);
            if (type == FOUR_CELL_CAL_TYPE_XTALK_CAL) {
                s5krst_read_4cell_from_eeprom(data, size);
            }
            break;
        }
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
        LOG_INF("current fps :%d\n", *feature_data_32);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.current_fps = (UINT16)*feature_data_32;
        spin_unlock(&imgsensor_drv_lock);
        break;
    case SENSOR_FEATURE_SET_HDR:
        LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_32);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.ihdr_mode = (BOOL)*feature_data_32;
        spin_unlock(&imgsensor_drv_lock);
        break;
    //-bug727089, liangyiyi.wt,MODIFY, 2022.10.27, fix KASAN opened to Monitor memory, camera  memory access error.
    case SENSOR_FEATURE_GET_CROP_INFO:
        LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
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
        case MSDK_SCENARIO_ID_CUSTOM1:
            memcpy((void *)wininfo,
            (void *)&imgsensor_winsize_info[5],
            sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            memcpy((void *)wininfo,
            (void *)&imgsensor_winsize_info[6],
            sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            memcpy((void *)wininfo,
            (void *)&imgsensor_winsize_info[7],
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
    case SENSOR_FEATURE_GET_PDAF_INFO:
        LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n",
            (UINT16) *feature_data);
        PDAFinfo =
          (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            //memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
        case MSDK_SCENARIO_ID_CUSTOM3:
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info, sizeof(struct SET_PD_BLOCK_INFO_T));
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info_4080_2296, sizeof(struct SET_PD_BLOCK_INFO_T));
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            break;
        default:
            break;
        }
        break;
    case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
        LOG_INF(
        "SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
            (UINT16) *feature_data);
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
        case MSDK_SCENARIO_ID_CUSTOM3:
            *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
        break;
    case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
        break;
    case SENSOR_FEATURE_SET_PDAF:
        LOG_INF("PDAF mode :%d\n", *feature_data_16);
        imgsensor.pdaf_mode = *feature_data_16;
        break;
    case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
        LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
            (UINT16)*feature_data,
            (UINT16)*(feature_data+1),
            (UINT16)*(feature_data+2));
        break;
    case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
        set_shutter_frame_length((UINT32) (*feature_data),
                    (UINT32) (*(feature_data + 1)),
                    (UINT16) (*(feature_data + 2)));
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
        LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
            (UINT16)*feature_data, (UINT16)*(feature_data+1));
        break;
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
        streaming_control(KAL_FALSE);
        break;
    case SENSOR_FEATURE_SET_STREAMING_RESUME:
        LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
            *feature_data);
        if (*feature_data != 0)
            set_shutter(*feature_data);
        streaming_control(KAL_TRUE);
        break;

    case SENSOR_FEATURE_GET_BINNING_TYPE:
        switch (*(feature_data + 1)) {
        case MSDK_SCENARIO_ID_CUSTOM2:
            *feature_return_para_32 = 1000; /*BINNING_NONE*/
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *feature_return_para_32 = 4000;
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *feature_return_para_32 = 1000; /*BINNING_AVERAGED*/
            break;
        }
        LOG_ERR("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
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
        case MSDK_SCENARIO_ID_CUSTOM1:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom1.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom2.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.custom3.mipi_pixel_rate;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.pre.mipi_pixel_rate;
            break;
        }
    }
        break;
    case SENSOR_FEATURE_GET_VC_INFO:
        LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n",
            (UINT16)*feature_data);
        pvcinfo =
            (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
        switch (*feature_data_32) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
                sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
                sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
        case MSDK_SCENARIO_ID_CUSTOM3:
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
                sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        default:
            break;
        }
        break;

    case SENSOR_FEATURE_SET_LSC_TBL:
    {
        kal_uint8 index =
            *(((kal_uint8 *)feature_para) + (*feature_para_len));

        w2s5kjn1rearst03_set_lsc_reg_setting(index, feature_data_16,
                      (*feature_para_len)/sizeof(UINT16));
    }
        break;

    case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
        *feature_return_para_32 = imgsensor.current_ae_effective_frame;
        break;

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


UINT32 W2S5KJN1REARST_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc != NULL) {
        *pfFunc = &sensor_func;
    }
    return ERROR_NONE;
} /* S5KJN103_MIPI_RAW_SensorInit */
