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

#define LOG_TAG "CameraCIMDevice"
//#define LOG_NDEBUG 0

#include "CameraCIMDevice.h"
#include "JZCameraParameters.h"

namespace android {

    const char CameraCIMDevice::path[] = "/dev/cim";
    Mutex CameraCIMDevice::sLock;
    CameraCIMDevice* CameraCIMDevice::sInstance = NULL;

    CameraCIMDevice* CameraCIMDevice::getInstance() {
        ALOGV("%s", __FUNCTION__);
        Mutex::Autolock _l(sLock);
        CameraCIMDevice* instance = sInstance;
        if (instance == NULL) {
            instance = new CameraCIMDevice();
            sInstance = instance;
        }
        return instance;
    }

    CameraCIMDevice::CameraCIMDevice()
        :CameraDeviceCommon(),
         mlock("CameraCIMDevice::lock"),
         device_fd(-1),
         pmem_device_fd(-1),
         mPmemTotalSize(0),
         cimDeviceState(DEVICE_UNINIT),
         currentId(-1),
         mChangedBuffer(false),
         mPreviewFrameSize(0),
         mtlb_base(0),
         mpreviewFormat(-1),
         mPreviewWidth(0),
         mPreviewHeight(0),
         preview_use_pmem(false),
         capture_use_pmem(false),
         mEnablestartZoom(false) {
        memset(&mglobal_info, 0, sizeof(struct global_info));
        memset(&preview_buffer, 0, sizeof(struct camera_buffer));
        memset(&capture_buffer, 0, sizeof(struct camera_buffer));
        preview_buffer.fd = -1;
        capture_buffer.fd = -1;
        initGlobalInfo();
    }

    CameraCIMDevice::~CameraCIMDevice() {
    }


    int CameraCIMDevice::allocateStream(BufferType type,camera_request_memory get_memory,
                                        uint32_t width,
                                        uint32_t height,
                                        int format) {
        status_t res = NO_ERROR;
        int tmp_size = 0;

        if (type != PREVIEW_BUFFER) {
            ALOGE("%s: don't support %d type buffer allocate",__FUNCTION__, type);
            return BAD_VALUE;
        }

        tmp_size = (width * height) << 1;
        if ((preview_buffer.common != NULL) && (preview_buffer.common->data != NULL)) {
            if (preview_buffer.size != tmp_size || preview_buffer.yuvMeta[0].format != format) {
                mChangedBuffer = true;
                freeStream(PREVIEW_BUFFER);
                mChangedBuffer = false;
            } else {
                mChangedBuffer = false;
                goto setdriver;
            }
        }

        preview_buffer.nr = mglobal_info.preview_buf_nr;
        preview_buffer.size = tmp_size;
        mPreviewFrameSize = (unsigned int)(preview_buffer.size);
        tmp_size = mPreviewFrameSize * mglobal_info.preview_buf_nr;


        if (format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
            if (initPmem(format) != NO_ERROR) {
                return BAD_VALUE;
            }
        }

        if ((mPmemTotalSize > 0) && (tmp_size >= mPmemTotalSize)) {
            ALOGE("%s: alloc buffer more than pmem buffer size, pmemTotalSize: %fMid, alloc: %fMib",
                  __FUNCTION__,mPmemTotalSize/(1024.0*1024.0), tmp_size/(1024.0*1024.0));
            return BAD_VALUE;
        }

        ALOGV("nr: %d size: %d, size: %dx%d, format: 0x%x",preview_buffer.nr ,preview_buffer.size,width, height, format);
        preview_buffer.fd = -1;
        if ((format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) && (pmem_device_fd > 0)) {
            preview_use_pmem = true;
            preview_buffer.fd = pmem_device_fd;
        } else {
            preview_use_pmem = false;
        }

        if (get_memory == NULL) {
            ALOGE("%s: no get memory tool",__FUNCTION__);
            return BAD_VALUE;
        }

        preview_buffer.common = get_memory(preview_buffer.fd,preview_buffer.size, preview_buffer.nr,NULL);
        if (preview_buffer.common != NULL && preview_buffer.common->data != NULL) {
            if (preview_use_pmem && (pmem_device_fd > 0)) {
                struct pmem_region region;
                ::ioctl(preview_buffer.fd, PMEM_GET_PHYS, &region);
                preview_buffer.paddr = region.offset;
            } else {
                ALOGV("%s: use tlb",__FUNCTION__);
                preview_buffer.paddr = 0x00;
            }

            preview_buffer.dmmu_info.vaddr = preview_buffer.common->data;
            preview_buffer.dmmu_info.size = preview_buffer.common->size;

            for (int i = 0; i < (int)(preview_buffer.common->size); i += 0x1000) {
                ((uint8_t*)(preview_buffer.common->data))[i] = 0xff;
            }
            ((uint8_t*)(preview_buffer.common->data))[preview_buffer.common->size - 1] = 0xff;
            dmmu_map_user_memory(&(preview_buffer.dmmu_info));

            for (int i= 0; i < preview_buffer.nr; ++i) {
                preview_buffer.yuvMeta[i].index = i;
                preview_buffer.yuvMeta[i].width = width;
                preview_buffer.yuvMeta[i].height = height;
                preview_buffer.yuvMeta[i].format = format;
                preview_buffer.yuvMeta[i].count = preview_buffer.nr;
                preview_buffer.yuvMeta[i].yAddr = (int32_t)preview_buffer.common->data + (preview_buffer.size) * i;
                preview_buffer.yuvMeta[i].yPhy = preview_buffer.paddr + i * (preview_buffer.size);
                if ((preview_buffer.yuvMeta[i].format == HAL_PIXEL_FORMAT_JZ_YUV_420_P)
                    || (preview_buffer.yuvMeta[i].format == HAL_PIXEL_FORMAT_YV12)) {
                    tmp_size = width*height;
                    preview_buffer.yuvMeta[i].uAddr = (preview_buffer.yuvMeta[i].yAddr + tmp_size + 127) & (-128);
                    preview_buffer.yuvMeta[i].vAddr = (preview_buffer.yuvMeta[i].uAddr + (tmp_size>>2) + 127) & (-128);
                    preview_buffer.yuvMeta[i].uPhy = (preview_buffer.yuvMeta[i].yPhy + tmp_size + 127) & (-128);
                    preview_buffer.yuvMeta[i].vPhy = (preview_buffer.yuvMeta[i].uPhy + (tmp_size>>2) + 127) & (-128);
                    preview_buffer.yuvMeta[i].yStride = (preview_buffer.yuvMeta[i].width + 15) & (-16);
                    preview_buffer.yuvMeta[i].uStride = ((preview_buffer.yuvMeta[i].yStride >> 1) + 15) & (-16);
                    preview_buffer.yuvMeta[i].vStride = preview_buffer.yuvMeta[i].uStride;
                    ALOGV("alloc yuv 420p, i = %d, yaddr = 0x%x",
                             i, preview_buffer.yuvMeta[i].yAddr);
                } else if (preview_buffer.yuvMeta[i].format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
                    tmp_size = width*height*12/8;
                    preview_buffer.yuvMeta[i].uAddr = preview_buffer.yuvMeta[i].yAddr + tmp_size;
                    preview_buffer.yuvMeta[i].vAddr = preview_buffer.yuvMeta[i].uAddr;
                    preview_buffer.yuvMeta[i].uPhy = preview_buffer.yuvMeta[i].yPhy + tmp_size;
                    preview_buffer.yuvMeta[i].vPhy = preview_buffer.yuvMeta[i].uPhy;
                    preview_buffer.yuvMeta[i].yStride = preview_buffer.yuvMeta[i].width<<4;
                    preview_buffer.yuvMeta[i].uStride = (preview_buffer.yuvMeta[i].yStride>>1);
                    preview_buffer.yuvMeta[i].vStride = preview_buffer.yuvMeta[i].uStride;
                    ALOGV("alloc yuv420b i = %d, yaddr = 0x%x", i, preview_buffer.yuvMeta[i].yAddr);
                } else if (preview_buffer.yuvMeta[i].format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
                    preview_buffer.yuvMeta[i].uAddr = preview_buffer.yuvMeta[i].yAddr;
                    preview_buffer.yuvMeta[i].vAddr = preview_buffer.yuvMeta[i].uAddr;
                    preview_buffer.yuvMeta[i].uPhy = preview_buffer.yuvMeta[i].yPhy;
                    preview_buffer.yuvMeta[i].vPhy = preview_buffer.yuvMeta[i].uPhy;
                    preview_buffer.yuvMeta[i].yStride = preview_buffer.yuvMeta[i].width<<1;
                    preview_buffer.yuvMeta[i].uStride = preview_buffer.yuvMeta[i].yStride;
                    preview_buffer.yuvMeta[i].vStride = preview_buffer.yuvMeta[i].yStride;
                    ALOGV("alloc yuv 422i i = %d, yaddr = 0x%x", i, preview_buffer.yuvMeta[i].yAddr);
                }
            }

        setdriver:
            ALOGV("%s: preview buffer size: %fMib [%dx%d]",
                  __FUNCTION__,preview_buffer.size*preview_buffer.nr/(1024.0*1024.0),
                  width,height);
            if (preview_use_pmem) {
                ::ioctl(device_fd, CIMIO_SET_TLB_BASE,0);
            } else if (mtlb_base > 0) {
                ::ioctl(device_fd, CIMIO_SET_TLB_BASE,mtlb_base);
            }
            ::ioctl(device_fd, CIMIO_SET_PREVIEW_MEM, (unsigned long)(preview_buffer.yuvMeta));
            return NO_ERROR;
        }
        return BAD_VALUE;
    }

    void CameraCIMDevice::freeStream(BufferType type) {

        if (type != PREVIEW_BUFFER) {
                ALOGE("%s: don't support buffer type for free",__FUNCTION__);
                return;
        }

        if (preview_buffer.common == NULL || preview_buffer.common->data == NULL) {
            ALOGE("%s: preview buffer already free",__FUNCTION__);
            return;
        }

        if ((device_fd < 0) || mChangedBuffer) {
            dmmu_unmap_user_memory(&(preview_buffer.dmmu_info));
            memset(preview_buffer.yuvMeta, 0, 5 * sizeof (CameraYUVMeta));
            preview_buffer.size = 0;
            preview_buffer.nr = 0;
            preview_buffer.paddr = 0;
            preview_buffer.fd = -1;
            if((preview_buffer.common != NULL) && (preview_buffer.common->data != NULL)) {
                if (preview_use_pmem) {
                    munmap(preview_buffer.common->data,preview_buffer.common->size);
                }
                preview_buffer.common->release(preview_buffer.common);
                preview_buffer.common->data = NULL;
                preview_buffer.common = NULL;
            }
        }
        return ;
    }

    void CameraCIMDevice::flushCache(void* buffer,int buffer_size) {

        uint32_t addr = 0;
        int size = 0;
        int flag = 1;

        if ((buffer == NULL) && (preview_buffer.yuvMeta[0].yAddr > 0)) {
            addr = preview_buffer.yuvMeta[0].yAddr;
            size = preview_buffer.size * preview_buffer.nr;
        } else {
            addr = (uint32_t)buffer;
            size = buffer_size;
        }
        cacheflush((long int)addr, (long int)((unsigned int)addr+size), flag);
    }

    void* CameraCIMDevice::getCurrentFrame(void) {

        unsigned int addr = 0;

        if (device_fd < 0) {
            ALOGE("%s: cim device don't open", __FUNCTION__);
            return NULL;
        }

        if (cimDeviceState & DEVICE_STARTED) {
            addr = ::ioctl(device_fd, CIMIO_GET_FRAME);
        }

        if (preview_use_pmem) {
            preview_buffer.offset = addr - preview_buffer.yuvMeta[0].yPhy;
            for (int i = 0; i < preview_buffer.nr; ++i) {
                if ((int32_t)addr == preview_buffer.yuvMeta[i].yPhy) {
                    preview_buffer.index = preview_buffer.yuvMeta[i].index;
                    return (void*)&(preview_buffer.yuvMeta[i]);
                }
            }
        } else {
            if ((preview_buffer.common != NULL) && (preview_buffer.common->data != NULL)) {
                preview_buffer.offset = addr - (unsigned int)(preview_buffer.common->data);
            }

            for (int i = 0; i < preview_buffer.nr; ++i) {
                if ((int32_t)addr == preview_buffer.yuvMeta[i].yAddr) {
                    preview_buffer.index = preview_buffer.yuvMeta[i].index;
                    return (void*)&(preview_buffer.yuvMeta[i]);
                }
            }
        }

        return NULL;
    }

    void CameraCIMDevice::initTakePicture(int width,int height,
         camera_request_memory get_memory) {

        if (device_fd < 0) {
            ALOGE("%s: cim device not open",__FUNCTION__);
            return;
        }
        ALOGV("%s: init take picture: %dx%d",__FUNCTION__,width, height);
        // set resolution
        struct frm_size pictureSize = {width,height};
        ::ioctl(device_fd, CIMIO_SET_CAPTURE_SIZE,&pictureSize);
        int format = HAL_PIXEL_FORMAT_YCbCr_422_I;

        // set format
#ifdef COMPRESS_JPEG_USE_HW
        format = HAL_PIXEL_FORMAT_JZ_YUV_420_B;
        if (initPmem(format) != NO_ERROR) {
            return BAD_VALUE;
        }
        capture_buffer.fd = pmem_device_fd;
        if (preview_use_pmem) {
            mChangedBuffer = true;
            freeStream(PREVIEW_BUFFER);
            mChangedBuffer = false;
        }
        capture_use_pmem = true;
        ::ioctl(device_fd, CIMIO_SET_CAPTURE_FMT, HAL_PIXEL_FORMAT_JZ_YUV_420_B);
#else
        capture_buffer.fd = -1;
        capture_use_pmem = false;
        ::ioctl(device_fd, CIMIO_SET_CAPTURE_FMT, HAL_PIXEL_FORMAT_YCbCr_422_I);
#endif
        // set mem
        capture_buffer.nr = mglobal_info.capture_buf_nr;
        capture_buffer.size = (width*height)<<1;
        ALOGV("%s: alloc capture buffer: %f Mib; nr: %d",
         __FUNCTION__,(float)capture_buffer.size / (1024*1024),capture_buffer.nr);

        if (get_memory == NULL) {
            ALOGE("%s: no get memory tool",__FUNCTION__);
            return ;
        }

        capture_buffer.common = get_memory(capture_buffer.fd,capture_buffer.size,capture_buffer.nr,NULL);
        if (capture_buffer.common != 0
            && capture_buffer.common->data != 0) {
            capture_buffer.dmmu_info.vaddr = capture_buffer.common->data;
            capture_buffer.dmmu_info.size = capture_buffer.common->size;

            for (int i = 0; i < (int)(capture_buffer.common->size); i += 0x1000) {
                ((uint8_t*)(capture_buffer.common->data))[i] = 0xff;
            }

            ((uint8_t*)(capture_buffer.common->data))[capture_buffer.common->size - 1] = 0xff;
            dmmu_map_user_memory(&(capture_buffer.dmmu_info));

            if (capture_use_pmem && (pmem_device_fd > 0)) {
                struct pmem_region region;
                ::ioctl(capture_buffer.fd, PMEM_GET_PHYS, &region);
                capture_buffer.paddr = region.offset;
            } else {
                capture_buffer.paddr = 0x00;
            }

            memset(capture_buffer.yuvMeta, 0, 5 * sizeof(struct CameraYUVMeta));
            int tmp_size = 0;
            for (int i= 0; i < capture_buffer.nr; ++i) {
                capture_buffer.yuvMeta[i].index = i;
                capture_buffer.yuvMeta[i].width = width;
                capture_buffer.yuvMeta[i].height = height;
                capture_buffer.yuvMeta[i].format = format;
                capture_buffer.yuvMeta[i].count = capture_buffer.nr;
                capture_buffer.yuvMeta[i].yAddr = (int32_t)capture_buffer.common->data + (capture_buffer.size) * i;
                capture_buffer.yuvMeta[i].yPhy = capture_buffer.paddr + i * (capture_buffer.size);
                if (capture_buffer.yuvMeta[i].format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
                    tmp_size = width*height*12/8;
                    capture_buffer.yuvMeta[i].uAddr = capture_buffer.yuvMeta[i].yAddr + tmp_size;
                    capture_buffer.yuvMeta[i].vAddr = capture_buffer.yuvMeta[i].uAddr;
                    capture_buffer.yuvMeta[i].uPhy = capture_buffer.yuvMeta[i].yPhy + tmp_size;
                    capture_buffer.yuvMeta[i].vPhy = capture_buffer.yuvMeta[i].uPhy;
                    capture_buffer.yuvMeta[i].yStride = capture_buffer.yuvMeta[i].width<<4;
                    capture_buffer.yuvMeta[i].uStride = (capture_buffer.yuvMeta[i].yStride>>1);
                    capture_buffer.yuvMeta[i].vStride = capture_buffer.yuvMeta[i].uStride;
                    ALOGV("alloc yuv420b i = %d, yaddr = 0x%x", i, capture_buffer.yuvMeta[i].yAddr);
                } else if (capture_buffer.yuvMeta[i].format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
                    capture_buffer.yuvMeta[i].uAddr = capture_buffer.yuvMeta[i].yAddr;
                    capture_buffer.yuvMeta[i].vAddr = capture_buffer.yuvMeta[i].uAddr;
                    capture_buffer.yuvMeta[i].uPhy = capture_buffer.yuvMeta[i].yPhy;
                    capture_buffer.yuvMeta[i].vPhy = capture_buffer.yuvMeta[i].uPhy;
                    capture_buffer.yuvMeta[i].yStride = capture_buffer.yuvMeta[i].width<<1;
                    capture_buffer.yuvMeta[i].uStride = capture_buffer.yuvMeta[i].yStride;
                    capture_buffer.yuvMeta[i].vStride = capture_buffer.yuvMeta[i].yStride;
                    ALOGV("alloc yuv 422i i = %d, yaddr = 0x%x", i, capture_buffer.yuvMeta[i].yAddr);
                }
            }

            if (capture_use_pmem) {
                ALOGV("%s: use tlb",__FUNCTION__);
                ::ioctl(device_fd, CIMIO_SET_TLB_BASE,0);
            } else if (mtlb_base > 0) {
                ::ioctl(device_fd, CIMIO_SET_TLB_BASE,mtlb_base);
            }
            ::ioctl(device_fd, CIMIO_SET_CAPTURE_MEM, (unsigned long)(capture_buffer.yuvMeta));
        }
        return;
    }

    void CameraCIMDevice::deInitTakePicture(void) {
        capture_buffer.size = 0;
        capture_buffer.nr = 0;
        capture_buffer.paddr = 0;
        capture_buffer.fd = -1;
        if (capture_buffer.common != NULL
            && capture_buffer.common->data != NULL) {
            dmmu_unmap_user_memory(&(capture_buffer.dmmu_info));
            if (capture_use_pmem) {
                munmap(capture_buffer.common->data,capture_buffer.common->size);
            }
            capture_buffer.common->release(capture_buffer.common);
            capture_buffer.common->data = NULL;
            capture_buffer.common = NULL;
        }
    }

    int CameraCIMDevice::getPreviewFrameSize(void) {
        return preview_buffer.size;
    }

    int CameraCIMDevice::getCaptureFrameSize(void) {
        return preview_buffer.size;
    }

    int CameraCIMDevice::getNextFrame(void) {
        usleep(21028);
        return NO_ERROR;
    }

    bool CameraCIMDevice::usePmem(void) {
        return preview_use_pmem;
    }

    camera_memory_t* CameraCIMDevice::getPreviewBufferHandle(void) {
        return preview_buffer.common;
    }

    camera_memory_t* CameraCIMDevice::getCaptureBufferHandle(void) {
        return preview_buffer.common;
    }

    unsigned int CameraCIMDevice::getPreviewFrameIndex(void) {
        return preview_buffer.index;
    }

    void CameraCIMDevice::getPreviewSize(int* w, int* h) {
        *w = mPreviewWidth;
        *h = mPreviewHeight;
    }

    int CameraCIMDevice::getFrameOffset(void) {
        return preview_buffer.offset;
    }

    int CameraCIMDevice::setCommonMode(CommonMode mode_type, unsigned short mode_value) {
        status_t res = NO_ERROR;

        if (device_fd < 0)
            return INVALID_OPERATION;

        switch(mode_type)
            {
            case WHITE_BALANCE:
                ALOGV("set white_balance mode");
                res = ::ioctl(device_fd,CIMIO_SET_PARAM, mode_value | CPCMD_SET_BALANCE);
                break;

            case EFFECT_MODE:
                ALOGV("set effect mode");
                res = ::ioctl(device_fd,CIMIO_SET_PARAM, mode_value | CPCMD_SET_EFFECT);
                break;

            case FLASH_MODE:
                ALOGV("set flash mode");
                res = ::ioctl(device_fd,CIMIO_SET_PARAM, mode_value | CPCMD_SET_FLASH_MODE);
                break;

            case FOCUS_MODE:
                ALOGV("set focus mode");
                res = ::ioctl(device_fd,CIMIO_SET_PARAM, mode_value | CPCMD_SET_FOCUS_MODE);
                break;

            case SCENE_MODE:
                ALOGV("set scene mode");
                res = ::ioctl(device_fd,CIMIO_SET_PARAM, mode_value | CPCMD_SET_SCENE_MODE);
                break;

            case ANTIBAND_MODE:
                ALOGV("set antiband mode");
                res = ::ioctl(device_fd,CIMIO_SET_PARAM, mode_value | CPCMD_SET_ANTIBANDING);
                break;
            case FLIP_HORIZONTALLY:
                ALOGV("set flip horizontally");
                break;
            case FLIP_VERTICALLY:
                ALOGV("set flip vertically");
                break;
            case BRIGHTNESS_UP:
                ALOGV("set brightness up");
                break;
            case BRIGHTNESS_DOWN:
                ALOGV("set brightness down");
                break;
            }
        return res;
    }

    int CameraCIMDevice::getFormat(int format)
    {
        int tmp_format;

        switch(format)
            {

            case PIXEL_FORMAT_YUV422SP:
                tmp_format = HAL_PIXEL_FORMAT_YCbCr_422_SP;
                break;

            case PIXEL_FORMAT_YUV420SP:
                tmp_format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
                break;

            case PIXEL_FORMAT_YUV422I:
                tmp_format = HAL_PIXEL_FORMAT_YCbCr_422_I;
                break;

            case PIXEL_FORMAT_YUV420P:
                tmp_format = HAL_PIXEL_FORMAT_YV12;
                break;

            case PIXEL_FORMAT_RGB565:
                tmp_format = HAL_PIXEL_FORMAT_RGB_565;
                break;

            case PIXEL_FORMAT_JZ_YUV420P:
                tmp_format = HAL_PIXEL_FORMAT_JZ_YUV_420_P;
                break;

            case PIXEL_FORMAT_JZ_YUV420T:
                tmp_format = HAL_PIXEL_FORMAT_JZ_YUV_420_B;
                break;

            default:
                tmp_format = HAL_PIXEL_FORMAT_YCbCr_422_I;
                break;

            }
        return tmp_format;
    }

    int CameraCIMDevice::getCaptureFormat() {
        return  getFormat(mpreviewFormat);
    }

    int CameraCIMDevice::getPreviewFormat() {
        return  getFormat(mpreviewFormat);
    }

    void CameraCIMDevice::setCameraFormat(int format) {
        if (mpreviewFormat != format) {
            mpreviewFormat = format;
            ::ioctl(device_fd, CIMIO_SET_PREVIEW_FMT, getFormat(mpreviewFormat));
        }
    }

    int CameraCIMDevice::setCameraParam(struct camera_param& param,int fps) {
        status_t res = NO_ERROR;
        if (device_fd > 0) {
            switch(param.cmd)
                {
                case CPCMD_SET_RESOLUTION:
                    res = ::ioctl(device_fd, CIMIO_SET_PREVIEW_SIZE, &(param.param.ptable[0]));
                    mPreviewWidth = param.param.ptable[0].w;
                    mPreviewHeight = param.param.ptable[0].h;
                    break;
                default:
                    ALOGE("%s: don't support cmd type",__FUNCTION__);
                    return BAD_VALUE;
                }
        }
        return res;
    }

    void CameraCIMDevice::getSensorInfo(struct sensor_info* s_info,struct resolution_info* r_info ) {

        if (device_fd > 0) {
            memset(&ms_info, 0, sizeof(struct sensor_info));
            ms_info.sensor_id = currentId >= 0 ? currentId : 0;
            ::ioctl(device_fd, CIMIO_GET_SENSORINFO, &ms_info);
            memcpy(s_info, &ms_info, sizeof(struct sensor_info));

            ALOGV("%s: sensor id = %d, name =%s, facing = %d,"
                     "orig = %d, preview table number = %d, capture table nr = %d",
                     __FUNCTION__,s_info->sensor_id, 
                     s_info->name, s_info->facing, 
                     s_info->orientation, s_info->prev_resolution_nr, 
                     s_info->cap_resolution_nr);

            ::ioctl(device_fd, CIMIO_GET_SUPPORT_PSIZE, &(r_info->ptable));
            ::ioctl(device_fd, CIMIO_GET_SUPPORT_CSIZE, &(r_info->ctable));
        }

        return;
    }

    static const char* supportTable[] = {
        "gc0308",
        "hi253",
        //"ov2650",
        //"gc2015",
        "ov2659-front",
        "ov2659-back",
        "ov7675",
        //"sp0838",
    };
    static const int supportTableSize = sizeof(supportTable) / sizeof(const char*);
    bool CameraCIMDevice::getSupportPreviewDataCapture(void) {
        struct sensor_info info;
        if (device_fd > 0) {
            info.sensor_id = currentId >= 0 ? currentId : 0;
            ::ioctl(device_fd, CIMIO_GET_SENSORINFO, &info);
            if (info.name == NULL
                || (strlen(info.name) == 0))
                return false;
            for (int i = 0; i < supportTableSize; ++i) {
                if (strcmp(supportTable[i],info.name) == 0) {
                    return true;
                }
            }
            ALOGV("%s not support preview data capture, name: %s",__FUNCTION__, info.name);
        }
        return false;
    }

    bool CameraCIMDevice::getSupportCaptureIncrease(void) {
        struct sensor_info info;
        if (device_fd > 0) {
            info.sensor_id = currentId >= 0 ? currentId : 0;
            ::ioctl(device_fd, CIMIO_GET_SENSORINFO, &info);
            if (info.name == NULL
                || (strlen(info.name) == 0))
                return false;
            if (strcmp("ov2650",info.name) == 0) {
                return true;
            }
            ALOGV("%s not support preview data capture, name: %s",__FUNCTION__, info.name);
        }
        return false;
    }

    int CameraCIMDevice::getCurrentCameraId(void) {
        return currentId;
    }

    int CameraCIMDevice::getResolution(struct resolution_info* r_info) {
        struct sensor_info s_info;
        getSensorInfo(&s_info, r_info);
        return NO_ERROR;
    }

    int CameraCIMDevice::initPmem(int format) {

        if (access(PMEMDEVICE, R_OK|W_OK) != 0) {
            ALOGE("%s: %s device don't access,err: %s",__FUNCTION__,
                  PMEMDEVICE, strerror(errno));
            return BAD_VALUE;
        }

        if (pmem_device_fd < 0) {
            pmem_device_fd = open(PMEMDEVICE, O_RDWR);
            if (pmem_device_fd < 0) {
                ALOGE("%s: open %s error, %s", __FUNCTION__, PMEMDEVICE, strerror(errno));
                pmem_device_fd = -1;
            } else {
                struct pmem_region region;
                ::ioctl(pmem_device_fd, PMEM_GET_TOTAL_SIZE, &region);
                mPmemTotalSize = region.len;
                ::ioctl(pmem_device_fd,PMEM_ALLOCATE,mPmemTotalSize);
            }
        }
        return NO_ERROR;
    }

    void CameraCIMDevice::deInitPmem(void) {
        if (pmem_device_fd > 0) {
            close(pmem_device_fd);
            pmem_device_fd = -1;
            mPmemTotalSize = 0;
        }
    }

    int CameraCIMDevice::connectDevice(int id) {
        status_t res = NO_ERROR;

        ALOGV("%s: cim device state = %d, connect camera = %d, currentId = %d",
                 __FUNCTION__,cimDeviceState, id, currentId);

        if (cimDeviceState & DEVICE_CONNECTED) {
            ALOGV("%s: already connect",__FUNCTION__);
            return res;
        }

        if (device_fd < 0) {
            device_fd = open(CameraCIMDevice::path,O_RDWR);
            if (device_fd < 0) {
                cimDeviceState = DEVICE_UNINIT;
                ALOGE("%s: can not connect cim device",__FUNCTION__);
                return NO_INIT;
            }
        }

        if (currentId != id) {
            currentId = id;
            ::ioctl(device_fd, CIMIO_SELECT_SENSOR, currentId);
        }

        if (mtlb_base == 0) {
            dmmu_init();
            dmmu_get_page_table_base_phys(&mtlb_base);
        }
        cimDeviceState |= DEVICE_CONNECTED;
        return res;
    }

    void CameraCIMDevice::disConnectDevice(void) {

        status_t res = NO_ERROR;

        ALOGV("%s:cim device state = %d",__FUNCTION__,cimDeviceState);
        if (cimDeviceState & DEVICE_CONNECTED) {
            if (device_fd > 0) {
                close(device_fd);
                device_fd = -1;
            }
            currentId = -1;
            cimDeviceState &= ~DEVICE_CONNECTED;
            mpreviewFormat = -1;
        }

        freeStream(PREVIEW_BUFFER);
        deInitPmem();

        if (mtlb_base > 0) {
            dmmu_deinit();
            mtlb_base = 0;
        }
        return;
    }

    int CameraCIMDevice::startDevice(void) {

        status_t res = UNKNOWN_ERROR;

        ALOGV("%s: cim device state = %d",__FUNCTION__, cimDeviceState);

        if (cimDeviceState & DEVICE_STARTED) {
            ALOGV("%s: already start",__FUNCTION__);
            return NO_ERROR;
        }
        ALOGV("%s: start preview",__FUNCTION__);
        res = ::ioctl(device_fd,CIMIO_START_PREVIEW);
        if (res == 0) {
            cimDeviceState &= ~DEVICE_STOPED;
            cimDeviceState |= DEVICE_STARTED;
        }
        return res;
    }

    int CameraCIMDevice::stopDevice(void) {
        status_t res = NO_ERROR;

        ALOGV("%s: cim device state = %d",__FUNCTION__, cimDeviceState);
        if(cimDeviceState & DEVICE_STOPED) {
            ALOGV("%s: already stop",__FUNCTION__);
            return res;
        }
        res = ::ioctl(device_fd,CIMIO_SHUTDOWN);
        if (res == 0) {
            cimDeviceState &= ~DEVICE_STARTED;
            cimDeviceState |= DEVICE_STOPED;
        }
        return res;
    }

    int CameraCIMDevice::getCameraModuleInfo(int camera_id, struct camera_info* info) {

        status_t res = NO_ERROR;

        ALOGV("%s: id: %d, current id: %d",__FUNCTION__,camera_id, currentId);

        if (device_fd < 0) {
            device_fd = open(CameraCIMDevice::path,O_RDWR);
            if (device_fd < 0) {
                ALOGE("%s: can not connect cim device",__FUNCTION__);
                return NO_INIT;
            }
        }

        if (device_fd > 0) {
            memset(&ms_info, 0, sizeof(struct sensor_info));
            ms_info.sensor_id = camera_id;
            res = ::ioctl(device_fd, CIMIO_GET_SENSORINFO, &ms_info);
            if (mglobal_info.sensor_count == 1) {
                info->facing = CAMERA_FACING_FRONT;
            } else {
                info->facing = ms_info.facing;
            }
            info->orientation = ms_info.orientation;

            ALOGV("%s: id: %d, name: %s, facing: %d, oriention: %d",
                     __FUNCTION__,ms_info.sensor_id, 
                     strlen(ms_info.name) > 0 ? ms_info.name : "NULL" , 
                     info->facing, info->orientation
                     );

            return NO_ERROR;
        }

        ALOGE("%s: get camera info error id: %d, device_fd: %d", __FUNCTION__, camera_id, device_fd);
        info->facing = CAMERA_FACING_FRONT;
        info->orientation = 0;
        return NO_ERROR;
    }

    void CameraCIMDevice::initGlobalInfo(void) {

        status_t res = BAD_VALUE;

        if (device_fd < 0) {

            device_fd = open(CameraCIMDevice::path,O_RDWR);
            if (device_fd < 0) {
                ALOGE("%s: can not connect cim device",__FUNCTION__);
                return ;
            }

            mglobal_info.sensor_count = ::ioctl(device_fd, CIMIO_GET_SENSOR_COUNT);
            mglobal_info.preview_buf_nr = PREVIEW_BUFFER_CONUT;
            mglobal_info.capture_buf_nr = CAPTURE_BUFFER_COUNT;
        }

        ALOGV("%s: device_fd: %d, num: %d", __FUNCTION__,device_fd,mglobal_info.sensor_count);
    }

    int CameraCIMDevice::getCameraNum(void) {

        status_t res = NO_ERROR;
          
        if (mglobal_info.sensor_count > 0)
            return mglobal_info.sensor_count;

        return 0; 
    }

    bool CameraCIMDevice::getZoomState(void) {
        return mEnablestartZoom;
    }

    int CameraCIMDevice::sendCommand(uint32_t cmd_type, uint32_t arg1, uint32_t arg2, uint32_t result) {

        status_t res = UNKNOWN_ERROR;

        ALOGV("%s: cim device state = %d, cmd = %d",
                 __FUNCTION__,cimDeviceState, cmd_type);

        if (device_fd > 0)
            {
                switch(cmd_type)
                    {
                    case PAUSE_FACE_DETECT:
                        break;
                    case FOCUS_INIT:
                        res = ::ioctl(device_fd, CIMIO_AF_INIT);
                        break;
                    case START_FOCUS:
                        res = ::ioctl(device_fd, CIMIO_DO_FOCUS);
                        break;
                    case GET_FOCUS_STATUS:
                        res = 0;//::ioctl(device_fd, CIMIO_GET_FOCUS_STATE);
                        break;
                    case START_PREVIEW:
                        res = ::ioctl(device_fd,CIMIO_START_PREVIEW);
                        break;
                    case STOP_PREVIEW:
                        res = ::ioctl(device_fd,CIMIO_SHUTDOWN);
                        break;
                    case START_ZOOM:
                        ALOGV("%s: start zoom",__FUNCTION__);
                        mEnablestartZoom = true;
                        res = NO_ERROR;
                        break;
                    case STOP_ZOOM:
                        ALOGV("%s: stop zoom",__FUNCTION__);
                        mEnablestartZoom = false;
                        res = NO_ERROR;
                        break;
                    case START_FACE_DETECT:
                        break;
                    case STOP_FACE_DETECT:
                        break;
                    case INIT_TAKE_PICTURE:
                        break;
                    case TAKE_PICTURE: {
                        int addr = ::ioctl(device_fd,CIMIO_START_CAPTURE);
                        if (addr != 0) {
                            if (capture_use_pmem) {
                                for (int i = 0 ; i < capture_buffer.nr; ++i) {
                                    if ((int32_t)addr == capture_buffer.yuvMeta[i].yPhy) {
                                        ALOGV("%s: take picture addr: 0x%x",__FUNCTION__,addr);
                                        return (int32_t)(&(capture_buffer.yuvMeta[i]));
                                    }
                                }
                            } else {
                                for (int i = 0; i < capture_buffer.nr; ++i) {
                                    if ((int32_t)addr == capture_buffer.yuvMeta[i].yAddr)
                                        return (int32_t)(&(capture_buffer.yuvMeta[i]));
                                }
                            }
                        }
                        res = 0;
                        break;
                    }
                    case STOP_PICTURE:
                        ALOGV("%s: stop picture",__FUNCTION__);
                        break;
                    }
            }
        return res;
    }
};
