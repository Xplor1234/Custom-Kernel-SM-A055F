/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _KD_IMGSENSOR_H
#define _KD_IMGSENSOR_H

#include <linux/ioctl.h>
#include <linux/i3c/device.h>

#ifndef ASSERT
#define ASSERT(expr)        WARN_ON(!(expr))
#endif

#define IMGSENSORMAGIC 'i'
/* IOCTRL(inode * ,file * ,cmd ,arg ) */
/* S means "set through a ptr" */
/* T means "tell by a arg value" */
/* G means "get by a ptr" */
/* Q means "get by return a value" */
/* X means "switch G and S atomically" */
/* H means "switch T and Q atomically" */

/******************************************************************************
 *
 ******************************************************************************/

/* sensorOpen */
#define KDIMGSENSORIOC_T_OPEN \
	_IO(IMGSENSORMAGIC, 0)
/* sensorGetInfo */
#define KDIMGSENSORIOC_X_GET_CONFIG_INFO \
	_IOWR(IMGSENSORMAGIC, 5, struct IMGSENSOR_GET_CONFIG_INFO_STRUCT)

#define KDIMGSENSORIOC_X_GETINFO \
	_IOWR(IMGSENSORMAGIC, 5, struct ACDK_SENSOR_GETINFO_STRUCT)
/* sensorGetResolution */
#define KDIMGSENSORIOC_X_GETRESOLUTION \
	_IOWR(IMGSENSORMAGIC, 10, struct ACDK_SENSOR_RESOLUTION_INFO_STRUCT)
/* For kernel 64-bit */
#define KDIMGSENSORIOC_X_GETRESOLUTION2 \
	_IOWR(IMGSENSORMAGIC, 10, struct ACDK_SENSOR_PRESOLUTION_STRUCT)
/* sensorFeatureControl */
#define KDIMGSENSORIOC_X_FEATURECONCTROL \
	_IOWR(IMGSENSORMAGIC, 15, struct ACDK_SENSOR_FEATURECONTROL_STRUCT)
/* sensorControl */
#define KDIMGSENSORIOC_X_CONTROL \
	_IOWR(IMGSENSORMAGIC, 20, struct ACDK_SENSOR_CONTROL_STRUCT)
/* sensorClose */
#define KDIMGSENSORIOC_T_CLOSE \
	_IO(IMGSENSORMAGIC, 25)
/* sensorSearch */
#define KDIMGSENSORIOC_T_CHECK_IS_ALIVE \
	_IO(IMGSENSORMAGIC, 30)
/* set sensor driver */
#define KDIMGSENSORIOC_X_SET_DRIVER \
	_IOWR(IMGSENSORMAGIC, 35, struct SENSOR_DRIVER_INDEX_STRUCT)
/* get socket postion */
#define KDIMGSENSORIOC_X_GET_SOCKET_POS \
	_IOWR(IMGSENSORMAGIC, 40, u32)
/* set I2C bus */
#define KDIMGSENSORIOC_X_SET_I2CBUS \
	_IOWR(IMGSENSORMAGIC, 45, u32)
/* set I2C bus */
#define KDIMGSENSORIOC_X_RELEASE_I2C_TRIGGER_LOCK \
	_IO(IMGSENSORMAGIC, 50)
/* Set Shutter Gain Wait Done */
#define KDIMGSENSORIOC_X_SET_SHUTTER_GAIN_WAIT_DONE \
	_IOWR(IMGSENSORMAGIC, 55, u32)
/* set mclk */
#define KDIMGSENSORIOC_X_SET_MCLK_PLL \
	_IOWR(IMGSENSORMAGIC, 60, struct ACDK_SENSOR_MCLK_STRUCT)
#define KDIMGSENSORIOC_X_GETINFO2 \
	_IOWR(IMGSENSORMAGIC, 65, struct IMAGESENSOR_GETINFO_STRUCT)
/* set open/close sensor index */
#define KDIMGSENSORIOC_X_SET_CURRENT_SENSOR \
	_IOWR(IMGSENSORMAGIC, 70, u32)
/* set GPIO */
#define KDIMGSENSORIOC_X_SET_GPIO \
	_IOWR(IMGSENSORMAGIC, 75, struct IMGSENSOR_GPIO_STRUCT)
/* Get ISP CLK */
#define KDIMGSENSORIOC_X_GET_ISP_CLK \
	_IOWR(IMGSENSORMAGIC, 80, u32)
/* Get CSI CLK */
#define KDIMGSENSORIOC_X_GET_CSI_CLK \
	_IOWR(IMGSENSORMAGIC, 85, u32)

/* Get ISP CLK via MMDVFS*/
#define KDIMGSENSORIOC_DFS_UPDATE \
	_IOWR(IMGSENSORMAGIC, 90, unsigned int)
#define KDIMGSENSORIOC_GET_SUPPORTED_ISP_CLOCKS \
	_IOWR(IMGSENSORMAGIC, 95, struct IMAGESENSOR_GET_SUPPORTED_ISP_CLK)
#define KDIMGSENSORIOC_GET_CUR_ISP_CLOCK \
	_IOWR(IMGSENSORMAGIC, 100, unsigned int)

#ifdef CONFIG_COMPAT
#define COMPAT_KDIMGSENSORIOC_X_GET_CONFIG_INFO \
	_IOWR(IMGSENSORMAGIC, 5, struct COMPAT_IMGSENSOR_GET_CONFIG_INFO_STRUCT)

#define COMPAT_KDIMGSENSORIOC_X_GETINFO \
	_IOWR(IMGSENSORMAGIC, 5, struct COMPAT_ACDK_SENSOR_GETINFO_STRUCT)
#define COMPAT_KDIMGSENSORIOC_X_FEATURECONCTROL \
	_IOWR(IMGSENSORMAGIC, 15, \
		struct COMPAT_ACDK_SENSOR_FEATURECONTROL_STRUCT)
#define COMPAT_KDIMGSENSORIOC_X_CONTROL \
	_IOWR(IMGSENSORMAGIC, 20, struct COMPAT_ACDK_SENSOR_CONTROL_STRUCT)
#define COMPAT_KDIMGSENSORIOC_X_GETINFO2 \
	_IOWR(IMGSENSORMAGIC, 65, struct COMPAT_IMAGESENSOR_GETINFO_STRUCT)
#define COMPAT_KDIMGSENSORIOC_X_GETRESOLUTION2 \
	_IOWR(IMGSENSORMAGIC, 10, struct COMPAT_ACDK_SENSOR_PRESOLUTION_STRUCT)
#endif

/*****************************************
 * Transfer SENSOR PID to I3C DEVICE INFO
 * I3C_DEVICE:
 *	Para1: manuf_id = PID[47:33]
 *	Para2: part_id = PID[31:16]
 *	Para3: data = NULL
 *****************************************/
#define PID_TO_I3C_DEV(_pid)  I3C_DEVICE ((_pid>>33), ((_pid>>16)&0xFFFF), (void *)NULL)

/************************************************************************
 *
 ************************************************************************/
#define HI1339_SENSOR_ID                          0x1339
#define SENSOR_DRVNAME_HI1339_MIPI_RAW            "hi1339_mipi_raw"
#define SENSOR_DRVNAME_HI1339SUBTXD_MIPI_RAW      "hi1339subtxd_mipi_raw"
#define SENSOR_DRVNAME_HI1339SUBOFILM_MIPI_RAW    "hi1339subofilm_mipi_raw"
#define HI1339OFILM_SENSOR_ID                     0x133A
#define SENSOR_DRVNAME_HI1339OFILM_MIPI_RAW       "hi1339ofilm_mipi_raw"
/* Onyx */
#define OV13B10LN_SENSOR_ID                       0x0D42
#define SENSOR_DRVNAME_OV13B10LN_MIPI_RAW         "ov13b10ln_mipi_raw"
#define S5K4H7LN_SENSOR_ID                        0x487B
#define SENSOR_DRVNAME_S5K4H7LN_MIPI_RAW          "s5k4h7ln_mipi_raw"

#define GC08A3REAR_SENSOR_ID                          0x08a3
#define SENSOR_DRVNAME_GC08A3REAR_MIPI_RAW            "gc08a3rear_mipi_raw"

#define S5K4H7SUB_SENSOR_ID                     0x487C
#define SENSOR_DRVNAME_S5K4H7SUB_MIPI_RAW       "s5k4h7sub_mipi_raw"
/* SENSOR CHIP VERSION */
/*IMX*/
#define IMX06A_SENSOR_ID                        0xa18a
#define IMX858_SENSOR_ID                        0x0858
#define IMX858DUAL_SENSOR_ID                    0x0859
#define IMX499_SENSOR_ID                        0x0499
#define IMX481_SENSOR_ID                        0x0481
#define IMX486_SENSOR_ID                        0x0486
#define IMX586_SENSOR_ID                        0x0586
#define IMX758_SENSOR_ID                        0x0758
#define IMX758LITE_SENSOR_ID                    0x0759
#define IMX598_SENSOR_ID                        0x0598
#define IMX598LITE_SENSOR_ID                    0x0599
#define IMX766_SENSOR_ID                        0x0766
#define IMX766DUAL_SENSOR_ID                    0x0767
#define IMX766O_SENSOR_ID                       0x0768
#define IMX766DUALO_SENSOR_ID                   0x0769
#define IMX800_SENSOR_ID                        0x0800
#define IMX709_SENSOR_ID                        0x0709
#define IMX866_SENSOR_ID                        0x0866
#define IMX866RGB_SENSOR_ID                     0x0866
#define IMX989_SENSOR_ID                        0x0989
#define IMX989LITE_SENSOR_ID                    0x098A
#define IMX966_SENSOR_ID                        0x9186
#define IMX709O_SENSOR_ID                       0x0709
#define IMX519_SENSOR_ID                        0x0519
#define IMX576_SENSOR_ID                        0x0576
#define IMX350_SENSOR_ID                        0x0350
#define IMX398_SENSOR_ID                        0x0398
#define IMX268_SENSOR_ID                        0x0268
#define IMX386_SENSOR_ID                        0x0386
#define IMX300_SENSOR_ID                        0x0300
#define IMX386_MONO_SENSOR_ID                   0x0286
#define IMX362_SENSOR_ID                        0x0362
#define IMX338_SENSOR_ID                        0x0338
#define IMX334_SENSOR_ID                        0x0334
#define IMX334SUB_SENSOR_ID                     0x0335
#define IMX376_SENSOR_ID                        0x0376
#define IMX318_SENSOR_ID                        0x0318
#define IMX319_SENSOR_ID                        0x0319
#define IMX377_SENSOR_ID                        0x0377
#define IMX278_SENSOR_ID                        0x0278
#define IMX258_SENSOR_ID                        0x0258
#define IMX258_MONO_SENSOR_ID                   0x0259
#define IMX230_SENSOR_ID                        0x0230
#define IMX220_SENSOR_ID                        0x0220
#define IMX219_SENSOR_ID                        0x0219
#define IMX214_SENSOR_ID                        0x0214
#define IMX214_MONO_SENSOR_ID                   0x0215
#define IMX179_SENSOR_ID                        0x0179
#define IMX178_SENSOR_ID                        0x0178
#define IMX135_SENSOR_ID                        0x0135
#define IMX132MIPI_SENSOR_ID                    0x0132
#define IMX119_SENSOR_ID                        0x0119
#define IMX105_SENSOR_ID                        0x0105
#define IMX091_SENSOR_ID                        0x0091
#define IMX073_SENSOR_ID                        0x0046
#define IMX058_SENSOR_ID                        0x0058
#define IMX582_SENSOR_ID                        0x0582
#define IMX596_SENSOR_ID                        0x0596
#define IMX6632X_SENSOR_ID                      0x0663
/*OV*/
#define OV23850_SENSOR_ID                       0x023850
#define OV16880_SENSOR_ID                       0x016880
#define OV16825MIPI_SENSOR_ID                   0x016820
#define OV13855_SENSOR_ID                       0xD855
#define OV13850_SENSOR_ID                       0xD850
#define OV12A10_SENSOR_ID                       0x1241
#define OV13870_SENSOR_ID                       0x013870
#define OV13850_SENSOR_ID                       0xD850
#define OV13855_SENSOR_ID                       0xD855
#define OV16885_SENSOR_ID                       0x16885
#define OV13855MAIN2_SENSOR_ID                  0xD856
#define OV12830_SENSOR_ID                       0xC830
#define OV9760MIPI_SENSOR_ID                    0x9760
#define OV9740MIPI_SENSOR_ID                    0x9740
#define OV9726_SENSOR_ID                        0x9726
#define OV9726MIPI_SENSOR_ID                    0x9726
#define OV8865_SENSOR_ID                        0x8865
#define OV8858_SENSOR_ID                        0x8858
#define OV8858S_SENSOR_ID                      (0x8858+1)
#define OV8856_SENSOR_ID                        0x885A
#define OV8830_SENSOR_ID                        0x8830
#define OV8825_SENSOR_ID                        0x8825
#define OV7675_SENSOR_ID                        0x7673
#define OV5693_SENSOR_ID                        0x5690
#define OV5670MIPI_SENSOR_ID                    0x5670
#define OV5670MIPI_SENSOR_ID_2                  (0x5670+010000)
#define OV2281MIPI_SENSOR_ID                    0x5670
#define OV5675MIPI_SENSOR_ID                    0x5675
#define OV5671MIPI_SENSOR_ID                    0x5671
#define OV5650_SENSOR_ID                        0x5651
#define OV5650MIPI_SENSOR_ID                    0x5651
#define OV5648MIPI_SENSOR_ID                    0x5648
#define OV5647_SENSOR_ID                        0x5647
#define OV5647MIPI_SENSOR_ID                    0x5647
#define OV5645MIPI_SENSOR_ID                    0x5645
#define OV5642_SENSOR_ID                        0x5642
#define OV4688MIPI_SENSOR_ID                    0x4688
#define OV3640_SENSOR_ID                        0x364C
#define OV2724MIPI_SENSOR_ID                    0x2724
#define OV2722MIPI_SENSOR_ID                    0x2722
#define OV2680MIPI_SENSOR_ID                    0x2680
#define OV2680_SENSOR_ID                        0x2680
#define OV2659_SENSOR_ID                        0x2656
#define OV2655_SENSOR_ID                        0x2656
#define OV2650_SENSOR_ID                        0x2652
#define OV2650_SENSOR_ID_1                      0x2651
#define OV2650_SENSOR_ID_2                      0x2652
#define OV2650_SENSOR_ID_3                      0x2655
#define OV20880MIPI_SENSOR_ID                   0x20880
#define OV05A20_SENSOR_ID                       0x5305
#define OX08B40_SENSOR_ID                       0x580841

/*S5K*/
#define S5KHP3SP_SENSOR_ID                      0x1b73
#define S5KJD1_SENSOR_ID                        0x3841
#define S5KJN1_SENSOR_ID                        0x38E1
#define S5K2LQSX_SENSOR_ID                      0x2c1a
#define S5K4H7_SENSOR_ID                        0x487B
#define S5K3P8SP_SENSOR_ID                      0x3108
#define S5K2T7SP_SENSOR_ID                      0x2147
#define S5K3P8SX_SENSOR_ID                      0x3108
#define S5K2L7_SENSOR_ID                        0x20C7
#define S5K3L6_SENSOR_ID                        0x30C6
#define S5K3L8_SENSOR_ID                        0x30C8
#define S5K3M3_SENSOR_ID                        0x30D3
#define S5K3M5SX_SENSOR_ID                      0x30D5
#define S5K3M5SXO_SENSOR_ID                     0x30D5
#define S5K2X8_SENSOR_ID                        0x2188
#define S5K2P6_SENSOR_ID                        0x2106
#define S5K2P7_SENSOR_ID                        0x2107
#define S5K2P8_SENSOR_ID                        0x2108
#define S5K3P3_SENSOR_ID                        0x3103
#define S5K3P3SX_SENSOR_ID                      0x3103
#define S5K3P8_SENSOR_ID                        0x3108
#define S5K3P8STECH_SENSOR_ID                   0xf3108
#define S5K3M2_SENSOR_ID                        0x30D2
#define S5K4E6_SENSOR_ID                        0x4e60
#define S5K3AAEA_SENSOR_ID                      0x07AC
#define S5K3BAFB_SENSOR_ID                      0x7070
#define S5K3H7Y_SENSOR_ID                       0x3087
#define S5K3H2YX_SENSOR_ID                      0x382b
#define S5KA3DFX_SENSOR_ID                      0x00AB
#define S5K3E2FX_SENSOR_ID                      0x3E2F
#define S5K4B2FX_SENSOR_ID                      0x5080
#define S5K4E1GA_SENSOR_ID                      0x4E10
#define S5K4ECGX_SENSOR_ID                      0x4EC0
#define S5K53BEX_SENSOR_ID                      0x45A8
#define S5K53BEB_SENSOR_ID                      0x87A8
#define S5K5BAFX_SENSOR_ID                      0x05BA
#define S5K5E2YA_SENSOR_ID                      0x5e20
#define S5K4HAYX_SENSOR_ID                      0x48AB
#define S5K4H5YX_2LANE_SENSOR_ID                0x485B
#define S5K4H5YC_SENSOR_ID                      0x485B
#define S5K83AFX_SENSOR_ID                      0x01C4
#define S5K5CAGX_SENSOR_ID                      0x05ca
#define S5K8AAYX_MIPI_SENSOR_ID                 0x08aa
#define S5K8AAYX_SENSOR_ID                      0x08aa
#define S5K5E8YX_SENSOR_ID                      0x5e80
#define S5K5E8YXREAR2_SENSOR_ID                 0x5e81
#define S5K5E9_SENSOR_ID                        0x559b
#define S5KHM2SP_SENSOR_ID                      0x1AD2
#define S5K4H7ALPHA_SENSOR_ID                   0x487B
#define S5KGD2_SENSOR_ID                        0x0842
#define S5KGM2_SENSOR_ID                        0x08D2
#define S5K5E9YX_SENSOR_ID                      0x559B
#define S5KGW3_SENSOR_ID                        0x7309

/*HI*/
#define HI1337_SENSOR_ID                        0x1337
#define HI1337F_SENSOR_ID                       0x1338
#define HI1337FU_SENSOR_ID                      0x1339
#define HI841_SENSOR_ID                         0x0841
#define HI707_SENSOR_ID                         0x00b8
#define HI704_SENSOR_ID                         0x0096
#define HI556_SENSOR_ID                         0x0556
#define HI551_SENSOR_ID                         0x0551
#define HI553_SENSOR_ID                         0x0553
#define HI545MIPI_SENSOR_ID                     0x0545
#define HI544MIPI_SENSOR_ID                     0x0544
#define HI542_SENSOR_ID                         0x00B1
#define HI542MIPI_SENSOR_ID                     0x00B1
#define HI253_SENSOR_ID                         0x0092
#define HI251_SENSOR_ID                         0x0084
#define HI191MIPI_SENSOR_ID                     0x0191
#define HIVICF_SENSOR_ID                        0x0081
#define HI1339SUBTXD_SENSOR_ID                  0x133A
#define HI1339SUBOFILM_SENSOR_ID                0x133B
#define HI847_SENSOR_ID                         0x0847
#define HI2021Q_SENSOR_ID                       0x2021
/*MT*/
#define MT9D011_SENSOR_ID                       0x1511
#define MT9D111_SENSOR_ID                       0x1511
#define MT9D112_SENSOR_ID                       0x1580
#define MT9M011_SENSOR_ID                       0x1433
#define MT9M111_SENSOR_ID                       0x143A
#define MT9M112_SENSOR_ID                       0x148C
#define MT9M113_SENSOR_ID                       0x2480
#define MT9P012_SENSOR_ID                       0x2800
#define MT9P012_SENSOR_ID_REV7                  0x2801
#define MT9T012_SENSOR_ID                       0x1600
#define MT9T013_SENSOR_ID                       0x2600
#define MT9T113_SENSOR_ID                       0x4680
#define MT9V112_SENSOR_ID                       0x1229
#define MT9DX11_SENSOR_ID                       0x1519
#define MT9D113_SENSOR_ID                       0x2580
#define MT9D115_SENSOR_ID                       0x2580
#define MT9D115MIPI_SENSOR_ID                   0x2580
#define MT9V113_SENSOR_ID                       0x2280
#define MT9V114_SENSOR_ID                       0x2283
#define MT9V115_SENSOR_ID                       0x2284
#define MT9P015_SENSOR_ID                       0x2803
#define MT9P017_SENSOR_ID                       0x4800
#define MT9P017MIPI_SENSOR_ID                   0x4800
#define MT9T113MIPI_SENSOR_ID                   0x4680
/*GC*/
#define GC5035_SENSOR_ID                        0x5035
#define GC2375_SENSOR_ID                        0x2375
#define GC2375H_SENSOR_ID                       0x2375
#define GC2375SUB_SENSOR_ID                     0x2376
#define GC2365_SENSOR_ID                        0x2365
#define GC2366_SENSOR_ID                        0x2366
#define GC2355_SENSOR_ID                        0x2355
#define GC2235_SENSOR_ID                        0x2235
#define GC2035_SENSOR_ID                        0x2035
#define GC2145_SENSOR_ID                        0x2145
#define GC0330_SENSOR_ID                        0xC1
#define GC0329_SENSOR_ID                        0xC0
#define GC0310_SENSOR_ID                        0xa310
#define GC0313MIPI_YUV_SENSOR_ID                0xD0
#define GC0312_SENSOR_ID                        0xb310
#define GC5035_MACRO_SENSOR_ID                  0x5036
#define GC08A3_SENSOR_ID						0x08a3
#define GC8034_SENSOR_ID                        0x8044
#define GC8C34_SENSOR_ID                        0x80C4
#define GC02M1_SENSOR_ID                        0x02e0
#define GC02M1_SENSOR_ID1                       0x02d0
#define GC02M1B_SENSOR_ID                       0x02e0
#define GC8054_SENSOR_ID                        0x8054
/*SP*/
#define SP0A19_YUV_SENSOR_ID                    0xA6
#define SP2518_YUV_SENSOR_ID                    0x53
#define SP2509_SENSOR_ID                        0x2509
#define SP250A_SENSOR_ID                        0x250A
/*A*/
#define A5141MIPI_SENSOR_ID                     0x4800
#define A5142MIPI_SENSOR_ID                     0x4800
/*HM*/
#define HM3451_SENSOR_ID                        0x345
/*AR*/
#define AR0833_SENSOR_ID                        0x4B03
/*SIV*/
#define SID020A_SENSOR_ID                       0x12B4
#define SIV100B_SENSOR_ID                       0x0C11
#define SIV100A_SENSOR_ID                       0x0C10
#define SIV120A_SENSOR_ID                       0x1210
#define SIV120B_SENSOR_ID                       0x0012
#define SIV121D_SENSOR_ID                       0xDE
#define SIM101B_SENSOR_ID                       0x09A0
#define SIM120C_SENSOR_ID                       0x0012
#define SID130B_SENSOR_ID                       0x001b
#define SIC110A_SENSOR_ID                       0x000D
#define SIV120B_SENSOR_ID                       0x0012
/*PAS (PixArt Image)*/
#define PAS105_SENSOR_ID                        0x0065
#define PAS302_SENSOR_ID                        0x0064
#define PAS5101_SENSOR_ID                       0x0067
#define PAS6180_SENSOR_ID                       0x6179
/*Panasoic*/
#define MN34152_SENSOR_ID                       0x01
/*Toshiba*/
#define T4KA7_SENSOR_ID                         0x2c30
/*Others*/
#define SR846_SENSOR_ID                         0x0846
#define SR846D_SENSOR_ID                        0x0846
#define SHARP3D_SENSOR_ID                       0x003d
#define T8EV5_SENSOR_ID                         0x1011

#define S5KGD1SP_SENSOR_ID                      0x0841
#define HI846_SENSOR_ID                         0x0846
#define OV02A10_MONO_SENSOR_ID                  0x2509
#define IMX686_SENSOR_ID                        0X0686
#define IMX616_SENSOR_ID                        0x0616
#define OV48C_SENSOR_ID                         0x564843
#define IMX355_SENSOR_ID                        0x0355
#define OV13B10LZ_SENSOR_ID                       0x0d42
#define OV13B10_SENSOR_ID                       0x560d42
#define OV02B10_SENSOR_ID                       0x002b
#define SR846D_SENSOR_ID                        0x0846

#define OV48B_SENSOR_ID                         0x564842
#define OV64B_SENSOR_ID                         0x566442
#define S5K3P9SP_SENSOR_ID                      0x3109
#define GC8054_SENSOR_ID                        0x8054
#define GC02M0_SENSOR_ID                        0x02d0
#define GC02M0_SENSOR_ID1                       0x02d1
#define GC02M0_SENSOR_ID2                       0x02d2
#define GC02K0_SENSOR_ID                        0x2385
#define OV16A10_SENSOR_ID                       0x561641
#define GC02M1B_SENSOR_ID                       0x02e0
#define GC08A3_SENSOR_ID                        0x08a3
#define MAX96712MIPI_SENSOR_ID  0x52
#define MAX96712ISXMIPI_SENSOR_ID  0x54
#define MAX96712AMIPI_SENSOR_ID   0xA4
#define MAX96712A0MIPI_SENSOR_ID  0xA0
#define MAX96712A1MIPI_SENSOR_ID  0xA1
#define MAX96712A2MIPI_SENSOR_ID  0xA2
#define MAX96712A3MIPI_SENSOR_ID  0xA3
#define LT7911_SENSOR_ID  0x1605
/* CAMERA DRIVER NAME */
#define CAMERA_HW_DEVNAME                       "kd_camera_hw"
/* SENSOR DEVICE DRIVER NAME */
/*IMX*/
#define SENSOR_DRVNAME_IMX06A_MIPI_RAW          "imx06a_mipi_raw"
#define SENSOR_DRVNAME_IMX858_MIPI_RAW          "imx858_mipi_raw"
#define SENSOR_DRVNAME_IMX858DUAL_MIPI_RAW      "imx858dual_mipi_raw"
#define SENSOR_DRVNAME_IMX499_MIPI_RAW          "imx499_mipi_raw"
#define SENSOR_DRVNAME_IMX499_MIPI_RAW_13M      "imx499_mipi_raw_13m"
#define SENSOR_DRVNAME_IMX481_MIPI_RAW          "imx481_mipi_raw"
#define SENSOR_DRVNAME_IMX486_MIPI_RAW          "imx486_mipi_raw"
#define SENSOR_DRVNAME_IMX586_MIPI_RAW          "imx586_mipi_raw"
#define SENSOR_DRVNAME_IMX598_MIPI_RAW          "imx598_mipi_raw"
#define SENSOR_DRVNAME_IMX598LITE_MIPI_RAW      "imx598lite_mipi_raw"
#define SENSOR_DRVNAME_IMX758_MIPI_RAW          "imx758_mipi_raw"
#define SENSOR_DRVNAME_IMX758LITE_MIPI_RAW      "imx758lite_mipi_raw"
#define SENSOR_DRVNAME_IMX766_MIPI_RAW          "imx766_mipi_raw"
#define SENSOR_DRVNAME_IMX766O_MIPI_RAW         "imx766o_mipi_raw"
#define SENSOR_DRVNAME_IMX709_MIPI_RAW          "imx709_mipi_raw"
#define SENSOR_DRVNAME_IMX709O_MIPI_RAW         "imx709o_mipi_raw"
#define SENSOR_DRVNAME_IMX766DUAL_MIPI_RAW      "imx766dual_mipi_raw"
#define SENSOR_DRVNAME_IMX766DUALO_MIPI_RAW     "imx766dualo_mipi_raw"
#define SENSOR_DRVNAME_IMX989_MIPI_RAW          "imx989_mipi_raw"
#define SENSOR_DRVNAME_IMX989LITE_MIPI_RAW      "imx989lite_mipi_raw"
#define SENSOR_DRVNAME_IMX966_MIPI_RAW          "imx966_mipi_raw"
#define SENSOR_DRVNAME_IMX866RGB_MIPI_RAW       "imx866rgb_mipi_raw"
#define SENSOR_DRVNAME_IMX800_MIPI_RAW          "imx800_mipi_raw"
#define SENSOR_DRVNAME_IMX6632X_MIPI_RAW        "imx6632x_mipi_raw"
#define SENSOR_DRVNAME_IMX519_MIPI_RAW          "imx519_mipi_raw"
#define SENSOR_DRVNAME_IMX519DUAL_MIPI_RAW      "imx519dual_mipi_raw"
#define SENSOR_DRVNAME_IMX576_MIPI_RAW          "imx576_mipi_raw"
#define SENSOR_DRVNAME_IMX350_MIPI_RAW          "imx350_mipi_raw"
#define SENSOR_DRVNAME_IMX398_MIPI_RAW          "imx398_mipi_raw"
#define SENSOR_DRVNAME_IMX268_MIPI_RAW          "imx268_mipi_raw"
#define SENSOR_DRVNAME_IMX386_MIPI_RAW          "imx386_mipi_raw"
#define SENSOR_DRVNAME_IMX300_MIPI_RAW          "imx300_mipi_raw"
#define SENSOR_DRVNAME_IMX386_MIPI_MONO         "imx386_mipi_mono"
#define SENSOR_DRVNAME_IMX362_MIPI_RAW          "imx362_mipi_raw"
#define SENSOR_DRVNAME_IMX338_MIPI_RAW          "imx338_mipi_raw"
#define SENSOR_DRVNAME_IMX334_MIPI_RAW          "imx334_mipi_raw"
#define SENSOR_DRVNAME_IMX334SUB_MIPI_RAW       "imx334sub_mipi_raw"
#define SENSOR_DRVNAME_IMX376_MIPI_RAW          "imx376_mipi_raw"
#define SENSOR_DRVNAME_IMX318_MIPI_RAW          "imx318_mipi_raw"
#define SENSOR_DRVNAME_IMX319_MIPI_RAW          "imx319_mipi_raw"
#define SENSOR_DRVNAME_IMX377_MIPI_RAW          "imx377_mipi_raw"
#define SENSOR_DRVNAME_IMX278_MIPI_RAW          "imx278_mipi_raw"
#define SENSOR_DRVNAME_IMX258_MIPI_RAW          "imx258_mipi_raw"
#define SENSOR_DRVNAME_IMX258_MIPI_MONO         "imx258_mipi_mono"
#define SENSOR_DRVNAME_IMX230_MIPI_RAW          "imx230_mipi_raw"
#define SENSOR_DRVNAME_IMX220_MIPI_RAW          "imx220_mipi_raw"
#define SENSOR_DRVNAME_IMX219_MIPI_RAW          "imx219_mipi_raw"
#define SENSOR_DRVNAME_IMX214_MIPI_MONO         "imx214_mipi_mono"
#define SENSOR_DRVNAME_IMX214_MIPI_RAW          "imx214_mipi_raw"
#define SENSOR_DRVNAME_IMX179_MIPI_RAW          "imx179_mipi_raw"
#define SENSOR_DRVNAME_IMX178_MIPI_RAW          "imx178_mipi_raw"
#define SENSOR_DRVNAME_IMX135_MIPI_RAW          "imx135_mipi_raw"
#define SENSOR_DRVNAME_IMX132_MIPI_RAW          "imx132_mipi_raw"
#define SENSOR_DRVNAME_IMX119_MIPI_RAW          "imx119_mipi_raw"
#define SENSOR_DRVNAME_IMX105_MIPI_RAW          "imx105_mipi_raw"
#define SENSOR_DRVNAME_IMX091_MIPI_RAW          "imx091_mipi_raw"
#define SENSOR_DRVNAME_IMX073_MIPI_RAW          "imx073_mipi_raw"
#define SENSOR_DRVNAME_IMX582_MIPI_RAW          "imx582_mipi_raw"
#define SENSOR_DRVNAME_IMX596_MIPI_RAW          "imx596_mipi_raw"
/*OV*/
#define SENSOR_DRVNAME_OV23850_MIPI_RAW         "ov23850_mipi_raw"
#define SENSOR_DRVNAME_OV16880_MIPI_RAW         "ov16880_mipi_raw"
#define SENSOR_DRVNAME_OV16885_MIPI_RAW         "ov16885_mipi_raw"
#define SENSOR_DRVNAME_OV16825_MIPI_RAW         "ov16825_mipi_raw"
#define SENSOR_DRVNAME_OV13855_MIPI_RAW         "ov13855_mipi_raw"
#define SENSOR_DRVNAME_OV13870_MIPI_RAW         "ov13870_mipi_raw"
#define SENSOR_DRVNAME_OV13855_MIPI_RAW         "ov13855_mipi_raw"
#define SENSOR_DRVNAME_OV13855MAIN2_MIPI_RAW    "ov13855main2_mipi_raw"
#define SENSOR_DRVNAME_OV13850_MIPI_RAW         "ov13850_mipi_raw"
#define SENSOR_DRVNAME_OV12A10_MIPI_RAW         "ov12a10_mipi_raw"
#define SENSOR_DRVNAME_OV12830_MIPI_RAW         "ov12830_mipi_raw"
#define SENSOR_DRVNAME_OV9760_MIPI_RAW          "ov9760_mipi_raw"
#define SENSOR_DRVNAME_OV9740_MIPI_YUV          "ov9740_mipi_yuv"
#define SENSOR_DRVNAME_0V9726_RAW               "ov9726_raw"
#define SENSOR_DRVNAME_OV9726_MIPI_RAW          "ov9726_mipi_raw"
#define SENSOR_DRVNAME_OV8865_MIPI_RAW          "ov8865_mipi_raw"
#define SENSOR_DRVNAME_OV8858_MIPI_RAW          "ov8858_mipi_raw"
#define SENSOR_DRVNAME_OV8858S_MIPI_RAW         "ov8858s_mipi_raw"
#define SENSOR_DRVNAME_OV8856_MIPI_RAW          "ov8856_mipi_raw"
#define SENSOR_DRVNAME_OV8830_RAW               "ov8830_raw"
#define SENSOR_DRVNAME_OV8825_MIPI_RAW          "ov8825_mipi_raw"
#define SENSOR_DRVNAME_OV7675_YUV               "ov7675_yuv"
#define SENSOR_DRVNAME_OV5693_MIPI_RAW          "ov5693_mipi_raw"
#define SENSOR_DRVNAME_OV5670_MIPI_RAW          "ov5670_mipi_raw"
#define SENSOR_DRVNAME_OV5670_MIPI_RAW_2        "ov5670_mipi_raw_2"
#define SENSOR_DRVNAME_OV2281_MIPI_RAW          "ov2281_mipi_raw"
#define SENSOR_DRVNAME_OV5675_MIPI_RAW          "ov5675mipiraw"
#define SENSOR_DRVNAME_OV5671_MIPI_RAW          "ov5671_mipi_raw"
#define SENSOR_DRVNAME_OV5647MIPI_RAW           "ov5647_mipi_raw"
#define SENSOR_DRVNAME_OV5645_MIPI_RAW          "ov5645_mipi_raw"
#define SENSOR_DRVNAME_OV5645_MIPI_YUV          "ov5645_mipi_yuv"
#define SENSOR_DRVNAME_OV5650MIPI_RAW           "ov5650_mipi_raw"
#define SENSOR_DRVNAME_OV5650_RAW               "ov5650_raw"
#define SENSOR_DRVNAME_OV5648_MIPI_RAW          "ov5648_mipi_raw"
#define SENSOR_DRVNAME_OV5647_RAW               "ov5647_raw"
#define SENSOR_DRVNAME_OV5642_RAW               "ov5642_raw"
#define SENSOR_DRVNAME_OV5642_MIPI_YUV          "ov5642_mipi_yuv"
#define SENSOR_DRVNAME_OV5642_MIPI_RGB          "ov5642_mipi_rgb"
#define SENSOR_DRVNAME_OV5642_MIPI_JPG          "ov5642_mipi_jpg"
#define SENSOR_DRVNAME_OV5642_YUV               "ov5642_yuv"
#define SENSOR_DRVNAME_OV5642_YUV_SWI2C         "ov5642_yuv_swi2c"
#define SENSOR_DRVNAME_OV4688_MIPI_RAW          "ov4688_mipi_raw"
#define SENSOR_DRVNAME_OV3640_RAW               "ov3640_raw"
#define SENSOR_DRVNAME_OV3640_YUV               "ov3640_yuv"
#define SENSOR_DRVNAME_OV2724_MIPI_RAW          "ov2724_mipi_raw"
#define SENSOR_DRVNAME_OV2722_MIPI_RAW          "ov2722_mipi_raw"
#define SENSOR_DRVNAME_OV2680_MIPI_RAW          "ov2680_mipi_raw"
#define SENSOR_DRVNAME_OV2659_YUV               "ov2659_yuv"
#define SENSOR_DRVNAME_OV2655_YUV               "ov2655_yuv"
#define SENSOR_DRVNAME_OV2650_RAW               "ov265x_raw"
#define SENSOR_DRVNAME_OV20880_MIPI_RAW         "ov20880_mipi_raw"
#define SENSOR_DRVNAME_OV05A20_MIPI_RAW         "ov05a20_mipi_raw"
#define SENSOR_DRVNAME_OX08B40_MIPI_RAW         "ox08b40_mipi_raw"

/*S5K*/
#define SENSOR_DRVNAME_S5KHP3SP_MIPI_RAW        "s5khp3sp_mipi_raw"
#define SENSOR_DRVNAME_S5KJD1_MIPI_RAW        "s5kjd1_mipi_raw"
#define SENSOR_DRVNAME_S5K2LQSX_MIPI_RAW        "s5k2lqsx_mipi_raw"
#define SENSOR_DRVNAME_S5K4H7_MIPI_RAW          "s5k4h7_mipi_raw"
#define SENSOR_DRVNAME_S5K3P8SP_MIPI_RAW        "s5k3p8sp_mipi_raw"
#define SENSOR_DRVNAME_S5K2T7SP_MIPI_RAW        "s5k2t7sp_mipi_raw"
#define SENSOR_DRVNAME_S5K2T7SP_MIPI_RAW_5M     "s5k2t7sp_mipi_raw_5m"
#define SENSOR_DRVNAME_S5K3P8SX_MIPI_RAW        "s5k3p8sx_mipi_raw"
#define SENSOR_DRVNAME_S5K2L7_MIPI_RAW          "s5k2l7_mipi_raw"
#define SENSOR_DRVNAME_S5K3L6_MIPI_RAW          "s5k3l6_mipi_raw"
#define SENSOR_DRVNAME_S5K3L8_MIPI_RAW          "s5k3l8_mipi_raw"
#define SENSOR_DRVNAME_S5K3M3_MIPI_RAW          "s5k3m3_mipi_raw"
#define SENSOR_DRVNAME_S5K3M5SX_MIPI_RAW        "s5k3m5sx_mipi_raw"
#define SENSOR_DRVNAME_S5K3M5SXO_MIPI_RAW       "s5k3m5sxo_mipi_raw"
#define SENSOR_DRVNAME_S5K2X8_MIPI_RAW          "s5k2x8_mipi_raw"
#define SENSOR_DRVNAME_S5K2P6_MIPI_RAW          "s5k2p6_mipi_raw"
#define SENSOR_DRVNAME_S5K2P7_MIPI_RAW          "s5k2p7_mipi_raw"
#define SENSOR_DRVNAME_S5K2P8_MIPI_RAW          "s5k2p8_mipi_raw"
#define SENSOR_DRVNAME_S5K3P3SX_MIPI_RAW        "s5k3p3sx_mipi_raw"
#define SENSOR_DRVNAME_S5K3P3_MIPI_RAW          "s5k3p3_mipi_raw"
#define SENSOR_DRVNAME_S5K3P8_MIPI_RAW          "s5k3p8_mipi_raw"
#define SENSOR_DRVNAME_S5K3M2_MIPI_RAW          "s5k3m2_mipi_raw"
#define SENSOR_DRVNAME_S5K4E6_MIPI_RAW          "s5k4e6_mipi_raw"
#define SENSOR_DRVNAME_S5K3H2YX_MIPI_RAW        "s5k3h2yx_mipi_raw"
#define SENSOR_DRVNAME_S5K3H7Y_MIPI_RAW         "s5k3h7y_mipi_raw"
#define SENSOR_DRVNAME_S5K4H5YC_MIPI_RAW        "s5k4h5yc_mipi_raw"
#define SENSOR_DRVNAME_S5K4E1GA_MIPI_RAW        "s5k4e1ga_mipi_raw"
#define SENSOR_DRVNAME_S5K4ECGX_MIPI_YUV        "s5k4ecgx_mipi_yuv"
#define SENSOR_DRVNAME_S5K5CAGX_YUV             "s5k5cagx_yuv"
#define SENSOR_DRVNAME_S5K4HAYX_MIPI_RAW        "s5k4hayx_mipi_raw"
#define SENSOR_DRVNAME_S5K4H5YX_2LANE_MIPI_RAW  "s5k4h5yx_2lane_mipi_raw"
#define SENSOR_DRVNAME_S5K5E2YA_MIPI_RAW        "s5k5e2ya_mipi_raw"
#define SENSOR_DRVNAME_S5K8AAYX_MIPI_YUV        "s5k8aayx_mipi_yuv"
#define SENSOR_DRVNAME_S5K8AAYX_YUV             "s5k8aayx_yuv"
#define SENSOR_DRVNAME_S5K5E8YX_MIPI_RAW        "s5k5e8yx_mipi_raw"
#define SENSOR_DRVNAME_S5K5E8YXREAR2_MIPI_RAW   "s5k5e8yxrear2_mipi_raw"
#define SENSOR_DRVNAME_S5K5E9_MIPI_RAW          "s5k5e9_mipi_raw"
#define SENSOR_DRVNAME_S5KHM2SP_MIPI_RAW        "s5khm2sp_mipi_raw"
#define SENSOR_DRVNAME_S5KJN1_MIPI_RAW			"s5kjn1_mipi_raw"
#define SENSOR_DRVNAME_S5K4H7ALPHA_MIPI_RAW     "s5k4h7alpha_mipi_raw"
#define SENSOR_DRVNAME_S5KGD2_MIPI_RAW         "s5kgd2_mipi_raw"
#define SENSOR_DRVNAME_S5KGM2_MIPI_RAW          "s5kgm2_mipi_raw"
#define SENSOR_DRVNAME_S5K5E9YX_MIPI_RAW        "s5k5e9yx_mipi_raw"
#define SENSOR_DRVNAME_S5KGW3_MIPI_RAW         "s5kgw3_mipi_raw"
/*HI*/
#define SENSOR_DRVNAME_HI841_MIPI_RAW           "hi841_mipi_raw"
#define SENSOR_DRVNAME_HI1337_MIPI_RAW          "hi1337_mipi_raw"
#define SENSOR_DRVNAME_HI1337F_MIPI_RAW         "hi1337f_mipi_raw"
#define SENSOR_DRVNAME_HI1337FU_MIPI_RAW        "hi1337fu_mipi_raw"
#define SENSOR_DRVNAME_HI707_YUV                "hi707_yuv"
#define SENSOR_DRVNAME_HI704_YUV                "hi704_yuv"
#define SENSOR_DRVNAME_HI556_MIPI_RAW           "hi556_mipi_raw"
#define SENSOR_DRVNAME_HI551_MIPI_RAW           "hi551_mipi_raw"
#define SENSOR_DRVNAME_HI553_MIPI_RAW           "hi553_mipi_raw"
#define SENSOR_DRVNAME_HI545_MIPI_RAW           "hi545_mipi_raw"
#define SENSOR_DRVNAME_HI542_RAW                "hi542_raw"
#define SENSOR_DRVNAME_HI542MIPI_RAW            "hi542_mipi_raw"
#define SENSOR_DRVNAME_HI544_MIPI_RAW           "hi544_mipi_raw"
#define SENSOR_DRVNAME_HI253_YUV                "hi253_yuv"
#define SENSOR_DRVNAME_HI191_MIPI_RAW           "hi191_mipi_raw"
#define SENSOR_DRVNAME_HI847_MIPI_RAW           "hi847_mipi_raw"
#define SENSOR_DRVNAME_HI2021Q_MIPI_RAW         "hi2021q_mipi_raw"
/*MT*/
#define SENSOR_DRVNAME_MT9P012_RAW              "mt9p012_raw"
#define SENSOR_DRVNAME_MT9P015_RAW              "mt9p015_raw"
#define SENSOR_DRVNAME_MT9P017_RAW              "mt9p017_raw"
#define SENSOR_DRVNAME_MT9P017_MIPI_RAW         "mt9p017_mipi_raw"
#define SENSOR_DRVNAME_MT9D115_MIPI_RAW         "mt9d115_mipi_raw"
#define SENSOR_DRVNAME_MT9V114_YUV              "mt9v114_yuv"
#define SENSOR_DRVNAME_MT9V115_YUV              "mt9v115_yuv"
#define SENSOR_DRVNAME_MT9T113_YUV              "mt9t113_yuv"
#define SENSOR_DRVNAME_MT9V113_YUV              "mt9v113_yuv"
#define SENSOR_DRVNAME_MT9T113_MIPI_YUV         "mt9t113_mipi_yuv"
/*GC*/
#define SENSOR_DRVNAME_GC02M0_MIPI_RAW          "gc02m0_mipi_raw"
#define SENSOR_DRVNAME_GC5035MIPI_RAW           "gc5035_mipi_raw"
#define SENSOR_DRVNAME_GC5035_MIPI_RAW          "gc5035_mipi_raw"
#define SENSOR_DRVNAME_GC5035_MACRO_MIPI_RAW    "gc5035_macro_mipi_raw"
#define SENSOR_DRVNAME_GC2375_MIPI_RAW          "gc2375_mipi_raw"
#define SENSOR_DRVNAME_GC2375H_MIPI_RAW         "gc2375h_mipi_raw"
#define SENSOR_DRVNAME_GC2375SUB_MIPI_RAW       "gc2375sub_mipi_raw"
#define SENSOR_DRVNAME_GC2365_MIPI_RAW          "gc2365_mipi_raw"
#define SENSOR_DRVNAME_GC2366_MIPI_RAW          "gc2366_mipi_raw"
#define SENSOR_DRVNAME_GC08A3_MIPI_RAW          "gc08a3_mipi_raw"
#define SENSOR_DRVNAME_GC2035_YUV               "gc2035_yuv"
#define SENSOR_DRVNAME_GC2235_RAW               "gc2235_raw"
#define SENSOR_DRVNAME_GC2355_MIPI_RAW          "gc2355_mipi_raw"
#define SENSOR_DRVNAME_GC2355_RAW               "gc2355_raw"
#define SENSOR_DRVNAME_GC0330_YUV               "gc0330_yuv"
#define SENSOR_DRVNAME_GC0329_YUV               "gc0329_yuv"
#define SENSOR_DRVNAME_GC2145_MIPI_YUV          "gc2145_mipi_yuv"
#define SENSOR_DRVNAME_GC0310_MIPI_YUV          "gc0310_mipi_yuv"
#define SENSOR_DRVNAME_GC0310_YUV               "gc0310_yuv"
#define SENSOR_DRVNAME_GC0312_YUV               "gc0312_yuv"
#define SENSOR_DRVNAME_GC0313MIPI_YUV           "gc0313_mipi_yuv"
#define SENSOR_DRVNAME_GC8C34_MIPI_RAW          "gc8c34_mipi_raw"
#define SENSOR_DRVNAME_GC8034_MIPI_RAW          "gc8034_mipi_raw"
#define SENSOR_DRVNAME_GC02M1_MIPI_RAW          "gc02m1_mipi_raw"
#define SENSOR_DRVNAME_GC02M1B_MIPI_MONO        "gc02m1b_mipi_mono"
#define SENSOR_DRVNAME_GC8054_MIPI_RAW           "gc8054_mipi_raw"
/*SP*/
#define SENSOR_DRVNAME_SP0A19_YUV               "sp0a19_yuv"
#define SENSOR_DRVNAME_SP2518_YUV               "sp2518_yuv"
#define SENSOR_DRVNAME_SP2509_MIPI_RAW          "sp2509_mipi_raw"
#define SENSOR_DRVNAME_SP250A_MIPI_RAW          "sp250a_mipi_raw"
/*A*/
#define SENSOR_DRVNAME_A5141_MIPI_RAW           "a5141_mipi_raw"
#define SENSOR_DRVNAME_A5142_MIPI_RAW           "a5142_mipi_raw"
/*HM*/
#define SENSOR_DRVNAME_HM3451_RAW               "hm3451_raw"
/*AR*/
#define SENSOR_DRVNAME_AR0833_MIPI_RAW          "ar0833_mipi_raw"
/*SIV*/
#define SENSOR_DRVNAME_SIV121D_YUV              "siv121d_yuv"
#define SENSOR_DRVNAME_SIV120B_YUV              "siv120b_yuv"
/*PAS (PixArt Image)*/
#define SENSOR_DRVNAME_PAS6180_SERIAL_YUV       "pas6180_serial_yuv"
/*Panasoic*/
#define SENSOR_DRVNAME_MN34152_MIPI_RAW         "mn34152_mipi_raw"
/*Toshiba*/
#define SENSOR_DRVNAME_T4KA7_MIPI_RAW           "t4ka7_mipi_raw"
/*Others*/
#define SENSOR_DRVNAME_SHARP3D_MIPI_YUV         "sharp3d_mipi_yuv"
#define SENSOR_DRVNAME_T8EV5_YUV                "t8ev5_yuv"
#define SENSOR_DRVNAME_SR846_MIPI_RAW           "sr846_mipi_raw"
#define SENSOR_DRVNAME_SR846D_MIPI_RAW          "sr846d_mipi_raw"
/*Test*/
#define SENSOR_DRVNAME_IMX135_MIPI_RAW_5MP      "imx135_mipi_raw_5mp"
#define SENSOR_DRVNAME_IMX135_MIPI_RAW_8MP      "imx135_mipi_raw_8mp"
#define SENSOR_DRVNAME_OV13870_MIPI_RAW_5MP     "ov13870_mipi_raw_5mp"
#define SENSOR_DRVNAME_OV8856_MIPI_RAW_5MP      "ov8856_mipi_raw_5mp"
#define SENSOR_DRVNAME_S5KGD1SP_MIPI_RAW        "s5kgd1sp_mipi_raw"
#define SENSOR_DRVNAME_HI846_MIPI_RAW           "hi846_mipi_raw"
#define SENSOR_DRVNAME_GC02M0_MIPI_RAW          "gc02m0_mipi_raw"
#define SENSOR_DRVNAME_GC08A3_MIPI_RAW          "gc08a3_mipi_raw"
#define SENSOR_DRVNAME_OV02A10_MIPI_MONO        "ov02a10_mipi_mono"
#define SENSOR_DRVNAME_IMX686_MIPI_RAW          "imx686_mipi_raw"
#define SENSOR_DRVNAME_IMX616_MIPI_RAW          "imx616_mipi_raw"
#define SENSOR_DRVNAME_OV48B_MIPI_RAW           "ov48b_mipi_raw"
#define SENSOR_DRVNAME_OV64B_MIPI_RAW           "ov64b_mipi_raw"
#define SENSOR_DRVNAME_S5K3P9SP_MIPI_RAW        "s5k3p9sp_mipi_raw"
#define SENSOR_DRVNAME_GC8054_MIPI_RAW          "gc8054_mipi_raw"
#define SENSOR_DRVNAME_GC02M0B_MIPI_MONO        "gc02m0b_mipi_mono"
#define SENSOR_DRVNAME_GC02M0B_MIPI_MONO1       "gc02m0b_mipi_mono1"
#define SENSOR_DRVNAME_GC02M0B_MIPI_MONO2       "gc02m0b_mipi_mono2"
#define SENSOR_DRVNAME_GC02K0B_MIPI_MONO        "gc02k0b_mipi_mono"
#define SENSOR_DRVNAME_OV16A10_MIPI_RAW         "ov16a10_mipi_raw"
#define SENSOR_DRVNAME_GC02M1B_MIPI_MONO        "gc02m1b_mipi_mono"
#define SENSOR_DRVNAME_OV48C_MIPI_RAW           "ov48c_mipi_raw"
#define SENSOR_DRVNAME_IMX355_MIPI_RAW          "imx355_mipi_raw"
#define SENSOR_DRVNAME_OV13B10LZ_MIPI_RAW       "ov13b10lz_mipi_raw"
#define SENSOR_DRVNAME_OV13B10_MIPI_RAW         "ov13b10_mipi_raw"
#define SENSOR_DRVNAME_OV02B10_MIPI_RAW         "ov02b10_mipi_raw"
#define SENSOR_DRVNAME_MAX96712_MIPI_YUV        "max96712_mipi_yuv"
#define SENSOR_DRVNAME_MAX96712ISX_MIPI_YUV        "max96712isx_mipi_yuv"
#define SENSOR_DRVNAME_MAX96712A_MIPI_YUV        "max96712a_mipi_yuv"
#define SENSOR_DRVNAME_MAX96712A0_MIPI_YUV        "max96712a0_mipi_yuv"
#define SENSOR_DRVNAME_MAX96712A1_MIPI_YUV        "max96712a1_mipi_yuv"
#define SENSOR_DRVNAME_MAX96712A2_MIPI_YUV        "max96712a2_mipi_yuv"
#define SENSOR_DRVNAME_MAX96712A3_MIPI_YUV        "max96712a3_mipi_yuv"
#define SENSOR_DRVNAME_LT7911_MIPI_YUV        "lt7911_mipi_yuv"

/*W2V*/
#define W2S5KJN1REARTRULY_SENSOR_ID  0x38E1
#define SENSOR_DRVNAME_W2S5KJN1REARTRULY_MIPI_RAW "w2s5kjn1reartruly_mipi_raw"
#define W2S5KJN1REARST_SENSOR_ID  0x38E2
#define SENSOR_DRVNAME_W2S5KJN1REARST_MIPI_RAW "w2s5kjn1rearst_mipi_raw"
#define W2HI5022QREARTXD_SENSOR_ID  0x5022
#define SENSOR_DRVNAME_W2HI5022QREARTXD_MIPI_RAW "w2hi5022qreartxd_mipi_raw"
//+bugS96901AA5-742,daizheng.wt,ADD,2024/08/15,add hi1336/sc1300mcs bringup code.
#define W2HI1336FRONTTRULY_SENSOR_ID              0x1336
#define SENSOR_DRVNAME_W2HI1336FRONTTRULY_MIPI_RAW          "w2hi1336fronttruly_mipi_raw"
#define W2SC1300MCSFRONTTXD_SENSOR_ID              0x9628
#define SENSOR_DRVNAME_W2SC1300MCSFRONTTXD_MIPI_RAW          "w2sc1300mcsfronttxd_mipi_raw"
#define W2SC1300MCSFRONTST_SENSOR_ID              0x9629
#define SENSOR_DRVNAME_W2SC1300MCSFRONTST_MIPI_RAW          "w2sc1300mcsfrontst_mipi_raw"
//-bugS96901AA5-742,daizheng.wt,ADD,2024/08/15,add hi1336/sc1300mcs bringup code.
//+s96901aa5-742,huangmiao.wt,ADD,2024/08/26, camera depth/micro sensor bringup
#define W2SC202CSDEPLH_SENSOR_ID  0xeb52
#define SENSOR_DRVNAME_W2SC202CSDEPLH_MIPI_MONO "w2sc202csdeplh_mipi_mono"
#define W2SC202CSDEPLH2_SENSOR_ID  0xeb54
#define SENSOR_DRVNAME_W2SC202CSDEPLH2_MIPI_MONO "w2sc202csdeplh2_mipi_mono"
#define W2C2519DEPCXT_SENSOR_ID  0x020f
#define SENSOR_DRVNAME_W2C2519DEPCXT_MIPI_MONO "w2c2519depcxt_mipi_mono"
#define W2C2519DEPCXT2_SENSOR_ID  0x0210
#define SENSOR_DRVNAME_W2C2519DEPCXT2_MIPI_MONO "w2c2519depcxt2_mipi_mono"
#define W2GC02M1DEPSJ_SENSOR_ID  0x02e0
#define SENSOR_DRVNAME_W2GC02M1DEPSJ_MIPI_RAW "w2gc02m1depsj_mipi_raw"
#define W2GC02M1MICROCXT_SENSOR_ID  0x02e1
#define SENSOR_DRVNAME_W2GC02M1MICROCXT_MIPI_RAW "w2gc02m1microcxt_mipi_raw"
#define W2BF2253LMICROSJ_SENSOR_ID  0x2253
#define SENSOR_DRVNAME_W2BF2253LMICROSJ_MIPI_RAW "w2bf2253lmicrosj_mipi_raw"
#define W2SC202CSMICROLH_SENSOR_ID  0xeb53
#define SENSOR_DRVNAME_W2SC202CSMICROLH_MIPI_RAW "w2sc202csmicrolh_mipi_raw"
//-s96901aa5-742,huangmiao.wt,ADD,2024/08/26, camera depth/micro sensor bringup

//+bug767780,changtianshi.wt,ADD,2022/08/21,add for TMO param diff
#define W2TMOS5KJN1REARTRULY_SENSOR_ID  0x38E3
#define SENSOR_DRVNAME_W2TMOS5KJN1REARTRULY_MIPI_RAW "w2tmos5kjn1reartruly_mipi_raw"
#define W2TMOS5KJN1REARST_SENSOR_ID  0x38E4
#define SENSOR_DRVNAME_W2TMOS5KJN1REARST_MIPI_RAW "w2tmos5kjn1rearst_mipi_raw"
#define W2TMOHI5022QREARTXD_SENSOR_ID  0x5023
#define SENSOR_DRVNAME_W2TMOHI5022QREARTXD_MIPI_RAW "w2tmohi5022qreartxd_mipi_raw"
#define W2TMOHI1336FRONTTRULY_SENSOR_ID              0x1336+1
#define SENSOR_DRVNAME_W2TMOHI1336FRONTTRULY_MIPI_RAW          "w2tmohi1336fronttruly_mipi_raw"
#define W2TMOSC1300MCSFRONTTXD_SENSOR_ID              0x9630
#define SENSOR_DRVNAME_W2TMOSC1300MCSFRONTTXD_MIPI_RAW          "w2tmosc1300mcsfronttxd_mipi_raw"
#define W2TMOSC1300MCSFRONTST_SENSOR_ID              0x9631
#define SENSOR_DRVNAME_W2TMOSC1300MCSFRONTST_MIPI_RAW          "w2tmosc1300mcsfrontst_mipi_raw"
#define W2TMOSC202CSDEPLH_SENSOR_ID  0xeb55
#define SENSOR_DRVNAME_W2TMOSC202CSDEPLH_MIPI_MONO "w2tmosc202csdeplh_mipi_mono"
#define W2TMOSC202CSDEPLH2_SENSOR_ID  0xeb57
#define SENSOR_DRVNAME_W2TMOSC202CSDEPLH2_MIPI_MONO "w2tmosc202csdeplh2_mipi_mono"
#define W2TMOC2519DEPCXT_SENSOR_ID  0x020e
#define SENSOR_DRVNAME_W2TMOC2519DEPCXT_MIPI_MONO "w2tmoc2519depcxt_mipi_mono"
#define W2TMOC2519DEPCXT2_SENSOR_ID  0x0211
#define SENSOR_DRVNAME_W2TMOC2519DEPCXT2_MIPI_MONO "w2tmoc2519depcxt2_mipi_mono"
#define W2TMOGC02M1DEPSJ_SENSOR_ID  0x02e3
#define SENSOR_DRVNAME_W2TMOGC02M1DEPSJ_MIPI_RAW "w2tmogc02m1depsj_mipi_raw"
#define W2TMOGC02M1MICROCXT_SENSOR_ID  0x02e4
#define SENSOR_DRVNAME_W2TMOGC02M1MICROCXT_MIPI_RAW "w2tmogc02m1microcxt_mipi_raw"
#define W2TMOBF2253LMICROSJ_SENSOR_ID  0x2254
#define SENSOR_DRVNAME_W2TMOBF2253LMICROSJ_MIPI_RAW "w2tmobf2253lmicrosj_mipi_raw"
#define W2TMOSC202CSMICROLH_SENSOR_ID  0xeb56
#define SENSOR_DRVNAME_W2TMOSC202CSMICROLH_MIPI_RAW "w2tmosc202csmicrolh_mipi_raw"
//-bug767780,changtianshi.wt,ADD,2022/08/21,add for TMO param diff

/*N28V*/
//+S96818AA3-866,chenjiaoyan.wt,ADD,2024/08/22, main sensor bringup
#define N28HI5021QREARTRULY_SENSOR_ID  0x5021+4
#define SENSOR_DRVNAME_N28HI5021QREARTRULY_MIPI_RAW "n28hi5021qreartruly_mipi_raw"
#define N28HI5021QREARDC_SENSOR_ID  0x5021+5
#define SENSOR_DRVNAME_N28HI5021QREARDC_MIPI_RAW "n28hi5021qreardc_mipi_raw"
#define N28GC50E0REARTXD_SENSOR_ID  0x50e0
#define SENSOR_DRVNAME_N28GC50E0REARTXD_MIPI_RAW "n28gc50e0reartxd_mipi_raw"
#define N28S5KJN1REARTRULY_SENSOR_ID  0x38E1
#define SENSOR_DRVNAME_N28S5KJN1REARTRULY_MIPI_RAW "n28s5kjn1reartruly_mipi_raw"
#define N28S5KJN1REARDC_SENSOR_ID  0x38E1+1
#define SENSOR_DRVNAME_N28S5KJN1REARDC_MIPI_RAW "n28s5kjn1reardc_mipi_raw"
//-S96818AA3-866,chenjiaoyan.wt,ADD,2024/08/22, main sensor bringup
#define N28HI846FRONTTRULY_SENSOR_ID              0x0846
#define SENSOR_DRVNAME_N28HI846FRONTTRULY_MIPI_RAW "n28hi846fronttruly_mipi_raw"
#define N28GC08A3FRONTCXT_SENSOR_ID               0x08a3
#define SENSOR_DRVNAME_N28GC08A3FRONTCXT_MIPI_RAW "n28gc08a3frontcxt_mipi_raw"
#define N28SC800CSFRONTDC_SENSOR_ID               0xd126
#define SENSOR_DRVNAME_N28SC800CSFRONTDC_MIPI_RAW "n28sc800csfrontdc_mipi_raw"
#define N28C8496FRONTDC_SENSOR_ID                 0x0803
#define SENSOR_DRVNAME_N28C8496FRONTDC_MIPI_RAW "n28c8496frontdc_mipi_raw"
#define N28SC800CSAFRONTDC_SENSOR_ID              0xd154
#define SENSOR_DRVNAME_N28SC800CSAFRONTDC_MIPI_RAW "n28sc800csafrontdc_mipi_raw"
//+S96818AA3-866,limeng5.wt,ADD,2024/08/19,C2519,GC2375H, depth sensor bringup
#define N28C2519DEPCXT_SENSOR_ID  0x020f
#define SENSOR_DRVNAME_N28C2519DEPCXT_MIPI_MONO "n28c2519depcxt_mipi_mono"
#define N28C2519DEPCXT2_SENSOR_ID  0x020f+1
#define SENSOR_DRVNAME_N28C2519DEPCXT2_MIPI_MONO "n28c2519depcxt2_mipi_mono"
#define N28C2519DEPCXT3_SENSOR_ID  0x020f+2
#define SENSOR_DRVNAME_N28C2519DEPCXT3_MIPI_MONO "n28c2519depcxt3_mipi_mono"
#define N28C2519DEPCXT4_SENSOR_ID  0x020f+3
#define SENSOR_DRVNAME_N28C2519DEPCXT4_MIPI_MONO "n28c2519depcxt4_mipi_mono"
#define N28C2519DEPCXT5_SENSOR_ID  0x020f+4
#define SENSOR_DRVNAME_N28C2519DEPCXT5_MIPI_MONO "n28c2519depcxt5_mipi_mono"
#define N28GC2375HDEPLH_SENSOR_ID  0x2375
#define SENSOR_DRVNAME_N28GC2375HDEPLH_MIPI_RAW "n28gc2375hdeplh_mipi_raw"
#define N28GC2375HDEPLH2_SENSOR_ID  0x2375+1
#define SENSOR_DRVNAME_N28GC2375HDEPLH2_MIPI_RAW "n28gc2375hdeplh2_mipi_raw"
#define N28GC2375HDEPLH3_SENSOR_ID  0x2375+2
#define SENSOR_DRVNAME_N28GC2375HDEPLH3_MIPI_RAW "n28gc2375hdeplh3_mipi_raw"
#define N28GC2375HDEPLH4_SENSOR_ID  0x2375+3
#define SENSOR_DRVNAME_N28GC2375HDEPLH4_MIPI_RAW "n28gc2375hdeplh4_mipi_raw"
#define N28GC2375HDEPLH5_SENSOR_ID  0x2375+4
#define SENSOR_DRVNAME_N28GC2375HDEPLH5_MIPI_RAW "n28gc2375hdeplh5_mipi_raw"
//-S96818AA3-866,limeng5.wt,ADD,2024/08/19,C2519,GC2375H, depth sensor bringup
/************************************
 * I3C SENSOR PID (TOTAL 48-BITS)
 ************************************/
/* SONY */
#define IMX866RGB_I3C_PID							0x036008660000


/************************************
 * ADD I3C SENSOR To TABLE
 ************************************/
static const struct i3c_device_id mtk_i3c_id_table[] = {
	PID_TO_I3C_DEV(IMX866RGB_I3C_PID),
	{ /* Sentinel, Don't remove this. */ },
};


/******************************************************************************
 *
 ******************************************************************************/
void KD_IMGSENSOR_PROFILE_INIT(void);
void KD_IMGSENSOR_PROFILE(char *tag);
void KD_IMGSENSOR_PROFILE_INIT_I2C(void);
void KD_IMGSENSOR_PROFILE_I2C(char *tag, int trans_num);

#define mDELAY(ms)     mdelay(ms)
#define uDELAY(us)       udelay(us)
#endif              /* _KD_IMGSENSOR_H */
