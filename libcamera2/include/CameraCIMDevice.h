/*
 * Camera HAL for Ingenic android 4.1
 *
 * Copyright 2011 Ingenic Semiconductor LTD.
 *
 * author: 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CAMERA_CIM_DEVICE_H_
#define __CAMERA_CIM_DEVICE_H_

#include "CameraDeviceCommon.h"

namespace android {

    class CameraCIMDevice : public CameraDeviceCommon{

    public:
        static CameraCIMDevice* getInstance();

    protected:

        CameraCIMDevice();

        virtual ~CameraCIMDevice();

    private:
        void initGlobalInfo(void);
        int  getFormat(int format);
        int initPmem(int format);
        void deInitPmem(void);

    public:

        int allocateStream(BufferType type,camera_request_memory get_memory,
                           uint32_t width,
                           uint32_t height,
                           int format);
        void freeStream(BufferType bype);
        void* getCurrentFrame(void);
        int getPreviewFrameSize(void);
        int getCaptureFrameSize(void);
        void getPreviewSize(int* w, int* h);
        int getNextFrame(void);
        bool usePmem(void);
        int getFrameOffset(void);
        unsigned int getPreviewFrameIndex(void);
        camera_memory_t* getPreviewBufferHandle(void);
        camera_memory_t* getCaptureBufferHandle(void);
        int getPreviewFormat(void);
        int getCaptureFormat(void);
        int setCommonMode(CommonMode mode_type, unsigned short mode_value);
        void setCameraFormat(int format);
        int setCameraParam(struct camera_param &param,int fps);
        void getSensorInfo(struct sensor_info* s_info,struct resolution_info* r_info );
        bool getSupportPreviewDataCapture(void);
        bool getSupportCaptureIncrease(void);
        int getResolution(struct resolution_info* info); 
        int getCurrentCameraId(void);
        int connectDevice(int id);
        void disConnectDevice(void);
        int startDevice(void);
        int stopDevice(void);
        int getCameraModuleInfo(int camera_id, struct camera_info* info);
        int getCameraNum(void);
        int sendCommand(uint32_t cmd_type, uint32_t arg1=0, uint32_t arg2=0, uint32_t result=0);
        void initTakePicture(int width,int height,camera_request_memory get_memory);
        void deInitTakePicture(void);
        bool getZoomState(void);
        void update_device_name(const char* deviceName, int len) {
            return;
        }

        unsigned long getTlbBase(void) {
            return (unsigned long)mtlb_base;
        }

        void flushCache(void* buffer,int buffer_size);

    private:
          
        mutable Mutex mlock;
        int device_fd;
        int pmem_device_fd;
        int mPmemTotalSize;
        int cimDeviceState;
        struct global_info mglobal_info;
        struct sensor_info   ms_info;
        struct camera_buffer preview_buffer;
        struct camera_buffer capture_buffer;
        int currentId;
        bool mChangedBuffer;
        unsigned int mPreviewFrameSize;
        unsigned int mtlb_base;
        int mpreviewFormat;
        int mPreviewWidth;
        int mPreviewHeight;
        bool preview_use_pmem;
        bool capture_use_pmem;
        bool mEnablestartZoom;
    public:

        static const char path[];
        static Mutex sLock;
        static CameraCIMDevice* sInstance;

    };
};
#endif
