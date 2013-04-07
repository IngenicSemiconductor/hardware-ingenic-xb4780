/*
 * linux/drivers/misc/jz_cim.h -- Ingenic CIM driver
 *
 * Copyright (C) 2005-2010, Ingenic Semiconductor Inc. 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __JZ_CIM_H__
#define __JZ_CIM_H__

#define CIMIO_SHUTDOWN               0x01 // stop preview
#define CIMIO_START_PREVIEW          0x02
#define CIMIO_START_CAPTURE          0x03
#define CIMIO_GET_FRAME              0x04
#define CIMIO_GET_SENSORINFO         0x05
#define CIMIO_GET_VAR                0x06
#define CIMIO_GET_CAPTURE_PARAM      0x07
#define CIMIO_GET_PREVIEW_PARAM      0x08 // get preview size and format
#define CIMIO_GET_SUPPORT_PSIZE      0x09
#define CIMIO_SET_PARAM              0x0a
#define CIMIO_SET_PREVIEW_MEM        0x0b // alloc mem for buffers by app
#define CIMIO_SET_CAPTURE_MEM        0x0c // alloc mem for buffers by app
#define CIMIO_SELECT_SENSOR          0x0d
#define CIMIO_DO_FOCUS               0x0e
#define CIMIO_AF_INIT                0x0f
#define CIMIO_SET_PREVIEW_SIZE       0x10
#define CIMIO_SET_CAPTURE_SIZE       0x11
#define CIMIO_GET_SUPPORT_CSIZE      0x12
#define CIMIO_SET_VIDEO_MODE         0x13
#define CIMIO_STOP_PREVIEW           0x14
#define CIMIO_SET_TLB_BASE           0x15
#define CIMIO_GET_SENSOR_COUNT       0x16
#define CIMIO_SET_PREVIEW_FMT        0x17
#define CIMIO_SET_CAPTURE_FMT        0x18

//camera param cmd
#define CPCMD_SET_RESOLUTION         0x01
#define CPCMD_SET_OUTPUT_FORMAT      0x03
//camera param cmd
#define CPCMD_SET_BALANCE            (0x1 << (16 + 3))
#define CPCMD_SET_EFFECT             (0x1 << (16 + 4))
#define CPCMD_SET_ANTIBANDING        (0x1 << (16 + 5))
#define CPCMD_SET_FLASH_MODE         (0x1 << (16 + 6))
#define CPCMD_SET_SCENE_MODE         (0x1 << (16 + 7))
#define CPCMD_SET_FOCUS_MODE         (0x1 << (16 + 9))
#define CPCMD_SET_FPS                (0x1 << (16 + 10))
#define CPCMD_SET_NIGHTSHOT_MODE     (0x1 << (16 + 11))
#define CPCMD_SET_LUMA_ADAPTATION    (0x1 << (16 + 12))
#define CPCMD_SET_BRIGHTNESS         (0x1 << (16 + 13))  //add for VT app 
#define CPCMD_SET_CONTRAST           (0x1 << (16 + 14)) //add for VT app 
//-------------------------------------------------

struct frm_size {
unsigned int w;
unsigned int h;
};

struct resolution_info {
struct frm_size ptable[16];
struct frm_size ctable[16];
};

struct mode_bit_map {
unsigned short balance;
unsigned short effect;
unsigned short antibanding;
unsigned short flash_mode;
unsigned short scene_mode;
unsigned short focus_mode;
unsigned short fps;
};

struct camera_param {
unsigned int cmd;
struct resolution_info param;
};

struct sensor_info {
unsigned int sensor_id;
char name[32];
int facing;
int orientation;

unsigned int prev_resolution_nr;
unsigned int cap_resolution_nr;

struct mode_bit_map modes;
};

#endif // __JZ_CIM_H__
