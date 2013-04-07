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

#ifndef __CAMERA_DEVICE_COMMON_H_
#define __CAMERA_DEVICE_COMMON_H_

#include "CameraCore.h"

namespace android {
  
    enum CommonMode {
        WHITE_BALANCE,
        EFFECT_MODE,
        FLASH_MODE,
        FOCUS_MODE,
        SCENE_MODE,
        ANTIBAND_MODE,
        FLIP_HORIZONTALLY,
        FLIP_VERTICALLY,
        BRIGHTNESS_UP,
        BRIGHTNESS_DOWN
    };

    enum Command{
        FOCUS_INIT,
        START_FOCUS,
        GET_FOCUS_STATUS,
        SET_PREVIEW_SIZE,
        SET_PICTURE_SIZE,
        START_PREVIEW,
        STOP_PREVIEW,
        START_ZOOM,
        STOP_ZOOM,
        START_FACE_DETECT,
        STOP_FACE_DETECT,
        PAUSE_FACE_DETECT,
        INIT_TAKE_PICTURE,
        TAKE_PICTURE,
        STOP_PICTURE,
    };

    enum BufferType{
        PREVIEW_BUFFER,
    };

    struct camera_buffer {
        struct camera_memory* common;
        struct dmmu_mem_info dmmu_info;
        CameraYUVMeta yuvMeta[5];
        int index;
        int offset;
        int size;
        int nr;
        int fd;
        int paddr;
    };

    struct global_info
    {
        unsigned int sensor_count;
        unsigned int preview_buf_nr;
        unsigned int capture_buf_nr;
    };

    class CameraDeviceCommon : public virtual RefBase{

    public:
        virtual ~CameraDeviceCommon(void){ }
        CameraDeviceCommon(void) { }

    public:
        virtual int allocateStream(BufferType type,camera_request_memory get_memory,
                                   uint32_t width,
                                   uint32_t height,
                                   int format)= 0;
        virtual void freeStream(BufferType type)= 0;
        virtual int getNextFrame(void)= 0;
        virtual void* getCurrentFrame(void)= 0;
        virtual int getPreviewFrameSize(void)= 0;
        virtual int getCaptureFrameSize(void)= 0;
        virtual void getPreviewSize(int* w, int* h) = 0;
        virtual int getFrameOffset(void)= 0;
        virtual camera_memory_t* getPreviewBufferHandle(void) = 0;
        virtual camera_memory_t* getCaptureBufferHandle(void) = 0;
        virtual unsigned int getPreviewFrameIndex(void) = 0;
        virtual int setCommonMode(CommonMode mode_type, unsigned short mode_value)= 0;
        virtual void setCameraFormat(int format) = 0;
        virtual int setCameraParam(struct camera_param& param,int fps)= 0;
        virtual int getResolution(struct resolution_info* info)= 0;
        virtual int getPreviewFormat()= 0;
        virtual int getCaptureFormat()= 0;
        virtual void getSensorInfo(struct sensor_info* s_info,struct resolution_info* r_info )= 0;
        virtual bool getSupportPreviewDataCapture(void) = 0;
        virtual bool getSupportCaptureIncrease(void) = 0;
        virtual int getCurrentCameraId(void)= 0;
        virtual int connectDevice(int id)= 0;
        virtual void disConnectDevice(void)= 0;
        virtual int startDevice(void)= 0;
        virtual int stopDevice(void)= 0;
        virtual int getCameraModuleInfo(int camera_id, struct camera_info* info)= 0;
        virtual int getCameraNum(void)= 0;
        virtual int sendCommand(uint32_t cmd_type, uint32_t arg1=0, uint32_t arg2=0, uint32_t result=0)= 0;
        virtual void initTakePicture(int width,int height,camera_request_memory get_memory) = 0;
        virtual void deInitTakePicture(void) = 0;
        virtual bool getZoomState(void) = 0;
        virtual void update_device_name(const char* deviceName, int len) = 0;
        virtual unsigned long getTlbBase(void) = 0;
        virtual void flushCache(void*,int)  = 0;
        virtual bool usePmem(void) = 0;

    public:
        enum {
            DEVICE_CONNECTED = 1<<0, //1
            DEVICE_STARTED = 1<<1,   //2
            DEVICE_INITIALIZED = 1<<2, //4
            DEVICE_UNINIT=1<<3, //8
            DEVICE_STOPED=1<<4, //16
        };

    };
};

#endif
