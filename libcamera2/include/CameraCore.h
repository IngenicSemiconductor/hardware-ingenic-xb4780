/*
 * Camera HAL for Ingenic android 4.1
 *
 * Copyright 2012 Ingenic Semiconductor LTD.
 *
 * author: 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CAMERA_CORE_H_
#define __CAMERA_CORE_H_

#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <linux/android_pmem.h>
#include <utils/Timers.h>
#include <utils/Log.h>
#include <utils/Errors.h>
#include <utils/String8.h>
#include <utils/threads.h>
#include <utils/WorkQueue.h>
#include <utils/List.h>
#include <binder/IMemory.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <cutils/properties.h>
#include <cmath>
#include <ui/PixelFormat.h>
#include <ui/Rect.h>
#include <ui/GraphicBufferMapper.h>
#include <media/stagefright/foundation/ADebug.h>

#include <hardware/camera.h>
#ifdef CAMERA_VERSION2
#include <hardware/camera2.h>
#endif
#undef PAGE_SIZE
#include "dmmu.h"
#include "android_jz_ipu.h"
#include "hal_public.h"
#include "hwcomposer.h"

#define PMEMDEVICE "/dev/pmem_camera"

#define START_ADDR_ALIGN 0x1000 /* 4096 byte */
#define STRIDE_ALIGN 0x800 /* 2048 byte */

#define PREVIEW_BUFFER_CONUT 5
#define PREVIEW_BUFFER_SIZE 0x400000 //4M
#define CAPTURE_BUFFER_COUNT 1

#define MIN_WIDTH 640
#define MIN_HEIGHT 480
#define SIZE_640X480      (MIN_WIDTH*MIN_HEIGHT)
#define SIZE_1600X1200    (1600*1200)
#define SIZE_1024X768     (1024*768)

#define RECORDING_BUFFER_NUM 2

#define LOG_FUNCTION_NAME ALOGV("%d: %s() ENTER", __LINE__, __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT ALOGV("%d: %s() EXIT", __LINE__, __FUNCTION__);

#define WHITE_BALANCEVALUES_NUM 8
enum WhiteBalanceValues {
    WHITE_BALANCE_AUTO = 0x1<<0,
    WHITE_BALANCE_INCANDESCENT = 0x1<<1,
    WHITE_BALANCE_FLUORESCENT = 0x1<<2,
    WHITE_BALANCE_WARM_FLUORESCENT = 0x1<<3,
    WHITE_BALANCE_DAYLIGHT = 0x1<<4,
    WHITE_BALANCE_CLOUDY_DAYLIGHT = 0x1<<5,
    WHITE_BALANCE_TWILIGHT = 0x1<<6,
    WHITE_BALANCE_SHADE = 0x1<<7
};

#define EFFECTVALUES_NUM 9
enum EffectValues {
    EFFECT_NONE = 0x1<<0,
    EFFECT_MONO = 0x1<<1,
    EFFECT_NEGATIVE = 0x1<<2,
    EFFECT_SOLARIZE = 0x1<<3,
    EFFECT_SEPIA = 0x1<<4,
    EFFECT_POSTERIZE = 0x1<<5,
    EFFECT_WHITEBOARD = 0x1<<6,
    EFFECT_BLACKBOARD = 0x1<<7,
    EFFECT_AQUA = 0x1<<8
};

#define ANTIBANVALUES_NUM 4
enum AntibandValues {
    ANTIBANDING_AUTO = 0x1<<0,
    ANTIBANDING_50HZ = 0x1<<1,
    ANTIBANDING_60HZ = 0x1<<2,
    ANTIBANDING_OFF = 0x1<<3
};

#define SCENEVALUES_NUM 16
enum SceneValues {
    SCENE_MODE_AUTO = 0x1<<0,
    SCENE_MODE_ACTION = 0x1<<1,
    SCENE_MODE_PORTRAIT = 0x1<<2,
    SCENE_MODE_LANDSCAPE = 0x1<<3,
    SCENE_MODE_NIGHT = 0x1<<4,
    SCENE_MODE_NIGHT_PORTRAIT = 0x1<<5,
    SCENE_MODE_THEATRE = 0x1<<6,
    SCENE_MODE_BEACH = 0x1<<7,
    SCENE_MODE_SNOW = 0x1<<8,
    SCENE_MODE_SUNSET = 0x1<<9,
    SCENE_MODE_STEADYPHOTO = 0x1<<10,
    SCENE_MODE_FIREWORKS = 0x1<<11,
    SCENE_MODE_SPORTS = 0x1<<12,
    SCENE_MODE_PARTY = 0x1<<13,
    SCENE_MODE_CANDLELIGHT = 0x1<<14,
    SCENE_MODE_BARCODE = 0x1<<15
};

#define FLASHMODE_NUM 6
enum FalshMode {
    FLASH_MODE_OFF = 0x1<<0,
    FLASH_MODE_AUTO = 0x1<<1,
    FLASH_MODE_ON = 0x1<<2,
    FLASH_MODE_RED_EYE = 0x1<<3,
    FLASH_MODE_TORCH = 0x1<<4,
    FLASH_MODE_ALWAYS = 0x1<<5
};

#define FOCUSMODE_NUM 7
enum FocusMode {
    FOCUS_MODE_FIXED = 0x1<<0,
    FOCUS_MODE_AUTO = 0x1<<1,
    FOCUS_MODE_INFINITY = 0x1<<2,
    FOCUS_MODE_MACRO = 0x1<<3,
    FOCUS_MODE_CONTINUOUS_VIDEO = 0x1<<4,
    FOCUS_MODE_CONTINUOUS_PICTURE = 0x1<<5,
    FOCUS_MODE_EDOF = 0x1<<6
};

#define PREVIEWFORMAT_NUM 8
enum PreviewFormat {
    PIXEL_FORMAT_YUV422SP = 0x1<<0,
    PIXEL_FORMAT_YUV420SP = 0x1<<1,
    PIXEL_FORMAT_YUV422I = 0x1<<2,
    PIXEL_FORMAT_RGB565 = 0x1<<3,
    PIXEL_FORMAT_JPEG = 0x1<<4,
    PIXEL_FORMAT_YUV420P = 0x1<<5,
    PIXEL_FORMAT_JZ_YUV420T = 0x1<<6,
    PIXEL_FORMAT_JZ_YUV420P = 0x1<<7
};


/**
 * The metadata of the video frame data.
 */

typedef struct CameraYUVMeta {
 
    /**
     *  The index in the frame buffers.
     */
 
	int32_t index;
    /**
     *  video  frame width and height.
     */
 
	int32_t width;
	int32_t height;

    /**
     *  if use pmem, this is y phys addr of video frame of yuv data .
     */
 
	int32_t yPhy;

    /**
     *  if use pmem, this is u phys addr of video frame of yuv data .
     */
 
	int32_t uPhy;


    /**
     *  if use pmem, this is v phys addr of video frame of yuv data .
     */
 
	int32_t vPhy;


    /**
     *  this is y virtual addr of video frame of yuv data .
     */
 
	int32_t yAddr;


    /**
     *  this is u virtual addr of video frame of yuv data .
     */

	int32_t uAddr;


    /**
     *  this is v virtual addr of video frame of yuv data .
     */

	int32_t vAddr;


    /**
     *  this is y stride in byte of video frame of yuv data .
     */

	int32_t yStride;


    /**
     *  this is uv stride in byte of video frame of yuv data .
     */

	int32_t uStride;
	int32_t vStride;


    /**
     *  this is buffer number of video frames.
     */

	int32_t count;


    /**
     *  this is format of video frames, it is define in
     *  system/core/include/system/graphics.h
     *  pixel format definitions
     */

	int32_t format;
}CameraYUVMeta;

#ifdef __cplusplus
extern "C" {
#endif

#include "jz_cim_core.h"

#ifdef __cplusplus
}
#endif
#endif
