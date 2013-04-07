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

#define LOG_TAG "CameraV4L2Device"
//#define LOG_NDEBUG 0
#include "CameraV4L2Device.h"
#include "JZCameraParameters.h"
#include <sys/stat.h>

//#define DEBUG_FLIP
#define LOST_FRAME_NUM 6

namespace android {

    Mutex CameraV4L2Device::sLock;
    CameraV4L2Device* CameraV4L2Device::sInstance = NULL;

    CameraV4L2Device* CameraV4L2Device::getInstance() {
        Mutex::Autolock _l(sLock);
        CameraV4L2Device* instance = sInstance;
        if (instance == 0) {
            instance = new CameraV4L2Device();
            sInstance = instance;
        }
        return instance;
    }

    CameraV4L2Device::CameraV4L2Device()
        :CameraDeviceCommon(),
         mlock("CameraV4L2Device::lock"),
         device_fd(-1),
         V4L2DeviceState(DEVICE_UNINIT),
         currentId(-1),
         mtlb_base(0),
         need_update(false),
         videoIn(NULL),
         mMmapPreviewBufferHandle (NULL),
         mCurrentFrameIndex(0),
         mPreviewFrameSize(0),
         mPreviewWidth(0),
         mPreviewHeight(0),
         mPreviewFps(15),
         mAllocWidth(0),
         mAllocHeight(0),
         preview_use_pmem(false),
         capture_use_pmem(false),
         mEnablestartZoom(false),
         pmem_device_fd(-1),
         mpreviewFormatHal(-1),
         mpreviewFormatV4l(-1),
         is_capture(false),
         io(IO_METHOD_USERPTR),
         isChangedSize(false),
         isSupportHighResuPre(false),
         mReqLostFrameNum(LOST_FRAME_NUM) {
        videoIn = (struct vdIn*)calloc(1, sizeof(struct vdIn));
        memset(&mglobal_info, 0, sizeof(struct global_info));
        memset(device_name, 0, 256);
        memset(&preview_buffer, 0, sizeof(struct camera_buffer));
        preview_buffer.fd = -1;
        memset(&capture_buffer, 0, sizeof(struct camera_buffer));
        capture_buffer.fd = -1;
        for (int i = 0; i < NB_BUFFER; ++i) {
            mPreviewBuffer[i] = NULL;
        }
        initGlobalInfo();
        s.control_list = NULL;
        s.num_controls = 0;
        s.width_req = 0;
        s.height_req = 0;
    }

    void CameraV4L2Device::setDeviceCount(int num)
    {
        mglobal_info.sensor_count = num;
        ALOGV("%s: set camera num: %d", __FUNCTION__, num);
    }

    void CameraV4L2Device::update_device_name(const char* deviceName, int len)
    {
        strncpy(device_name, deviceName, len);
        device_name[len+1] = '\0';
        need_update = true;
        ALOGV("%s: set device name: %s",__FUNCTION__, deviceName);
    }

    void CameraV4L2Device::initGlobalInfo(void)
    {
        mglobal_info.preview_buf_nr = NB_BUFFER;
        mglobal_info.capture_buf_nr = NB_BUFFER;
        ALOGV("%s: buffer num: %d",__FUNCTION__,NB_BUFFER);
        return;
    }

    CameraV4L2Device::~CameraV4L2Device()
    {
        if (videoIn != NULL) {
            free(videoIn);
            videoIn = NULL;
        }
    }

    int CameraV4L2Device::allocateStream(BufferType type,camera_request_memory get_memory,
                                         uint32_t width,
                                         uint32_t height,
                                         int format) {

        if (V4L2DeviceState & DEVICE_STARTED) {
            ALOGV("%s already start",device_name);
            return NO_ERROR;
        }
        mAllocWidth = width;
        mAllocHeight = height;
        mPreviewFrameSize = (int)((width*height)<<1);
        init_param(width,height,mPreviewFps);
        switch(io) {
        case IO_METHOD_READ:
            return init_read(videoIn->format.fmt.pix.sizeimage,get_memory);
            break;
        case IO_METHOD_MMAP:
            return init_mmap(get_memory);
            break;
        case IO_METHOD_USERPTR:
            return init_userp(width, height, get_memory, format);
            break;
        }
        return NO_ERROR;
    }

    int CameraV4L2Device::init_read (unsigned int buffer_size,
                                     camera_request_memory get_memory) {

        videoIn->read_write_buffers = get_memory(-1, buffer_size, 1, NULL);
        if (videoIn->read_write_buffers == NULL) {
            ALOGE("Out of memory");
            return NO_MEMORY;
        }
        struct dmmu_mem_info dmmu_info;
        dmmu_info.vaddr = videoIn->read_write_buffers->data;
        dmmu_info.size = videoIn->read_write_buffers->size;
        dmmu_map_buffer(&dmmu_info);
        return NO_ERROR;
    }

    int CameraV4L2Device::init_mmap(camera_request_memory get_memory) {

        int ret = NO_ERROR;

        ALOGV("Enter %s",__FUNCTION__);

        isChangedSize = true;
        freeStream(PREVIEW_BUFFER);
        isChangedSize = false;

        memset(&videoIn->rb, 0, sizeof(videoIn->rb));
        videoIn->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        videoIn->rb.memory = V4L2_MEMORY_MMAP;
        videoIn->rb.count = NB_BUFFER;

        if (-1 == ::ioctl (device_fd, VIDIOC_REQBUFS, &videoIn->rb)) {
            ALOGE("%s: reqbufs error:%s",__FUNCTION__, strerror(errno));
            return BAD_VALUE;
        }

        if (videoIn->rb.count < 2) {
            ALOGE("%s: buffer number < 2",__FUNCTION__);
            return BAD_VALUE;
        }

        if (get_memory == NULL) {
            return BAD_VALUE;
        }

        struct dmmu_mem_info dmmu_info;
        for (unsigned int i=0; i<videoIn->rb.count; ++i) {
            memset(&videoIn->buf, 0, sizeof(struct v4l2_buffer));
            videoIn->buf.index = i;
            videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            videoIn->buf.memory = V4L2_MEMORY_MMAP;
            if (-1 == ::ioctl (device_fd, VIDIOC_QUERYBUF, &videoIn->buf)) {
                ALOGE("Init: Unable to query buffer (%s)", strerror(errno));
                return BAD_VALUE;
            }

            if (mMmapPreviewBufferHandle == NULL) {
                mMmapPreviewBufferHandle = get_memory(-1,videoIn->buf.length, videoIn->rb.count, NULL);
                dmmu_info.vaddr = mMmapPreviewBufferHandle->data;
                dmmu_info.size = mMmapPreviewBufferHandle->size;
                dmmu_map_buffer(&dmmu_info);
            }
            videoIn->mem_length[i] = videoIn->buf.length;
            videoIn->mem_num++;
            videoIn->mem[i] = mmap(NULL,
                                   videoIn->buf.length,
                                   PROT_READ|PROT_WRITE,
                                   MAP_SHARED,
                                   device_fd,
                                   videoIn->buf.m.offset);
            memset(&dmmu_info, 0, sizeof(struct dmmu_mem_info));
            dmmu_info.vaddr = videoIn->mem[i];
            dmmu_info.size = videoIn->mem_length[i];
            dmmu_map_buffer(&dmmu_info);
            if (videoIn->mem[i] == MAP_FAILED) {
                ALOGE("Init: Unable to map buffer (%s)", strerror(errno));
                return -1;
            }
            ret = ::ioctl(device_fd, VIDIOC_QBUF, &videoIn->buf);
            if (ret < 0) {
                ALOGE("Init: VIDIOC_QBUF failed, err: %s",strerror(errno));
                return -1;
            }
        }
        return NO_ERROR;
    }

    int CameraV4L2Device::init_userp(uint32_t width, uint32_t height,
                     camera_request_memory get_memory,int format) {
        ALOGV("Enter %s, device: %s, size: %dx%d",
              __FUNCTION__,device_name, width, height);

        int ret = NO_ERROR;
        int size = (width*height)<<1;

        if ((preview_buffer.common != NULL)
            && (preview_buffer.common->data != NULL)) {
            if ((size != preview_buffer.size)
                || (preview_buffer.yuvMeta[0].format != format)) {
                isChangedSize = true;
                freeStream(PREVIEW_BUFFER);
                isChangedSize = false;
            } else {
                goto reqbuf;
            }
        }

        if (format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
            if (initPmem(format) != NO_ERROR) {
                return BAD_VALUE;
            }
       }

        preview_buffer.nr = mglobal_info.preview_buf_nr;
        preview_buffer.size = size;
        if ((format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) && (pmem_device_fd > 0)) {
            preview_buffer.fd = pmem_device_fd;
            preview_use_pmem = true;
        } else {
            preview_buffer.fd = -1;
            preview_use_pmem = false;
        }

        if (get_memory == NULL) {
            return BAD_VALUE;
        }

        preview_buffer.common = get_memory(preview_buffer.fd,
                              preview_buffer.size,preview_buffer.nr,NULL);
        if (preview_buffer.common != NULL) {
            for (int i=0; i<preview_buffer.nr; i++) {
                mPreviewBuffer[i] = (uint8_t*)preview_buffer.common->data
                 + (i * preview_buffer.size);
            }

            preview_buffer.dmmu_info.vaddr = preview_buffer.common->data;
            preview_buffer.dmmu_info.size = preview_buffer.common->size;
            dmmu_map_buffer(&preview_buffer.dmmu_info);
        } else {
            ALOGE("%s: alloc preview buffer error",__FUNCTION__);
            return BAD_VALUE;
        }

    reqbuf:
        mCurrentFrameIndex = 0;
        memset(&videoIn->rb, 0, sizeof(videoIn->rb));
        videoIn->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        videoIn->rb.memory = V4L2_MEMORY_USERPTR;
        videoIn->rb.count = NB_BUFFER;
        ret = ::ioctl(device_fd, VIDIOC_REQBUFS, &videoIn->rb);
        if (ret < 0) {
            ALOGE("Init: VIDIOC_REQBUFS failed: %s", strerror(errno));
            return ret;
        }

        if (videoIn->rb.count < 2) {
            ALOGE("Insufficient buffer memory on : %s", device_name);
            return UNKNOWN_ERROR;
        }

        if (!preview_use_pmem) {
#ifdef VIDIOC_SET_TLB_BASE
            if ((mtlb_base>0) && (-1 == ::ioctl(device_fd,VIDIOC_SET_TLB_BASE,&mtlb_base))) {
                ALOGE("%s: set tlb base error: %s",__FUNCTION__, strerror(errno));
                return BAD_VALUE;
            }
#endif
        }

        for (unsigned int i = 0; i < videoIn->rb.count; ++i) {
            memset(&videoIn->buf, 0, sizeof(struct v4l2_buffer));
            videoIn->buf.index = i;
            videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            videoIn->buf.memory = V4L2_MEMORY_USERPTR;
            videoIn->buf.m.userptr = (unsigned long)mPreviewBuffer[i];
            videoIn->buf.length = preview_buffer.size;
            ret = ::ioctl(device_fd, VIDIOC_QBUF, &videoIn->buf);
            if (ret < 0) {
                ALOGE("Init: VIDIOC_QBUF failed,err: %s",strerror(errno));
                return BAD_VALUE;
            }
        }
        return NO_ERROR;
    }

    void CameraV4L2Device::dmmu_map_buffer(struct dmmu_mem_info *dmmu_info) {
        for (int i = 0; i < (int)(dmmu_info->size); i += 0x1000) {
            ((uint8_t*)(dmmu_info->vaddr))[i] = 0xff;
        }
        ((uint8_t*)(dmmu_info->vaddr))[dmmu_info->size - 1] = 0xff;
        dmmu_map_user_memory(dmmu_info);
    }

    void CameraV4L2Device::freeStream(BufferType type) {

        if (device_fd < 0 || isChangedSize) {
            switch(io) {
            case IO_METHOD_READ:
                freeReadWritePreviewBuffer();
                break;
            case IO_METHOD_USERPTR:
                freePreviewBuffer(type);
                break;
            case IO_METHOD_MMAP:
                freeMMapPreviewBuffer();
                break;
            }
        }
    }

    void CameraV4L2Device::freeReadWritePreviewBuffer(void) {

        if (videoIn->read_write_buffers != NULL) {
            struct dmmu_mem_info dmmu_info;
            dmmu_info.vaddr = videoIn->read_write_buffers->data;
            dmmu_info.size = videoIn->read_write_buffers->size;
            dmmu_unmap_user_memory(&dmmu_info);
            videoIn->read_write_buffers->release(videoIn->read_write_buffers);
            videoIn->read_write_buffers = NULL;
        }
    }

    void CameraV4L2Device::freeMMapPreviewBuffer(void) {
        int ret = NO_ERROR;
        ALOGV("Enter %s",__FUNCTION__);

        struct dmmu_mem_info dmmu_info;
        if (mMmapPreviewBufferHandle != NULL) {
            dmmu_info.vaddr = mMmapPreviewBufferHandle->data;
            dmmu_info.size = mMmapPreviewBufferHandle->size;
            dmmu_unmap_user_memory(&dmmu_info);
            mMmapPreviewBufferHandle->release(mMmapPreviewBufferHandle);
            mMmapPreviewBufferHandle = NULL;
        }

        for (int i=0; i<videoIn->mem_num; ++i) {
            if (videoIn->mem[i] != NULL) {
                dmmu_info.vaddr = videoIn->mem[i];
                dmmu_info.size = videoIn->mem_length[i];
                dmmu_unmap_user_memory(&dmmu_info);
                ret = munmap(videoIn->mem[i], videoIn->mem_length[i]);
                videoIn->mem[i] = NULL;
                videoIn->mem_length[i] = 0;
            }
            if (-1 == ret) {
                ALOGE("%s: unmap %d buffer error: %s",__FUNCTION__, i, strerror(errno));
            }
        }
    }

    void CameraV4L2Device::freePreviewBuffer(BufferType type) {

        ALOGV("Enter %s",__FUNCTION__);
        if (type != PREVIEW_BUFFER) {
            ALOGE("%s: don't support %d type",__FUNCTION__, type);
            return;
        }

        if (preview_buffer.common == NULL) {
            ALOGE("%s: %d type buffer already free",__FUNCTION__, type);
            return;
        }

        dmmu_unmap_user_memory(&(preview_buffer.dmmu_info));
        if (preview_use_pmem) {
            munmap(preview_buffer.common->data,preview_buffer.common->size);
        }
        preview_buffer.common->release(preview_buffer.common);
        preview_buffer.common->data = NULL;
        preview_buffer.common = NULL;
        preview_buffer.size = 0;
        mCurrentFrameIndex = 0;
        memset(&preview_buffer.yuvMeta[0], 0, sizeof(CameraYUVMeta));
        ALOGD("%s: free preview buffer success",__FUNCTION__);

        for (int i = 0; i < preview_buffer.nr; ++i) {
            mPreviewBuffer[i] = NULL;
        }
    }

    void* CameraV4L2Device::getCurrentFrame(void)
    {
        unsigned int count = 100;

        while (count-- > 0) {
            for (;;) {
                fd_set fds;
                struct timeval tv;
                int r;
                FD_ZERO (&fds);
                FD_SET (device_fd, &fds);

                tv.tv_sec = 4;
                tv.tv_usec = 0;

                r = select(device_fd + 1, &fds, NULL, NULL, &tv);

                if (-1 == r) {
                    if (EINTR == errno) {
                        ALOGE("select error");
                        continue;
                    } else {
                        ALOGE("select error!!! exit");
                        return NULL;
                    }
                }

                if (0 == r) {
                    ALOGE("select timeout");
                    return NULL;
                }

                if (read_frame() != NULL) {
                    if (LOST_FRAME_NUM == mReqLostFrameNum) {
                        goto exit_loop;
                    } else {
                        mReqLostFrameNum++;
                        ALOGD("lost %d frame.",mReqLostFrameNum);
                        continue;
                    }
                }

                if (errno != EAGAIN) {
                    return NULL;
                }
            }
        }
    exit_loop:
        CameraYUVMeta* yuvMeta = &preview_buffer.yuvMeta[0];
        return (void*)yuvMeta;
    }

    void* CameraV4L2Device::read_frame(void)
    {
        switch (io) {
        case IO_METHOD_READ:
            return getReadWriteCurrentFrame();
            break;
        case IO_METHOD_USERPTR:
            return getUserPtrCurrentFrame();
            break;
        case IO_METHOD_MMAP:
            return getMmapCurrentFrame();
            break;
        }
        return NULL;
    }

    void* CameraV4L2Device::getReadWriteCurrentFrame(void) {

        if (mpreviewFormatHal != PIXEL_FORMAT_YUV422I) {
            ALOGE("%s: don't support 0x%x format",__FUNCTION__,
                  mpreviewFormatHal);
            return NULL;
        }

        if (-1 == read(device_fd, videoIn->read_write_buffers->data,
                       videoIn->read_write_buffers->size)) {
            ALOGE("%s: read buffer error: %s",__FUNCTION__, strerror(errno));
            return NULL;
        }

        mCurrentFrameIndex = 0;
        CameraYUVMeta* yuvMeta = &preview_buffer.yuvMeta[0];
        memset(yuvMeta, 0, sizeof(CameraYUVMeta));
        yuvMeta->index = 0;
        yuvMeta->count = 1;
        yuvMeta->width = mAllocWidth;
        yuvMeta->height = mAllocHeight;
        yuvMeta->yAddr = (uint32_t)videoIn->read_write_buffers->data;
        yuvMeta->yPhy = 0;
        yuvMeta->uPhy = 0;
        yuvMeta->vPhy = 0;
        yuvMeta->format = HAL_PIXEL_FORMAT_YCbCr_422_I;
        yuvMeta->yStride = mAllocWidth << 1;
        yuvMeta->uStride = yuvMeta->yStride;
        yuvMeta->vStride = yuvMeta->yStride;
        yuvMeta->uAddr = yuvMeta->yAddr;
        yuvMeta->vAddr = yuvMeta->yAddr;
        return (void*)yuvMeta;
    }

    void* CameraV4L2Device::getUserPtrCurrentFrame(void) {

        int ret = NO_ERROR;

        memset(&videoIn->buf, 0, sizeof(videoIn->buf));
        videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        videoIn->buf.memory = V4L2_MEMORY_USERPTR;
        ret = ::ioctl(device_fd , VIDIOC_DQBUF, &videoIn->buf);
        if (ret < 0) {
            ALOGE("%s: VIDIOC_DQBUF failed,err: %s",__FUNCTION__,strerror(errno));
            return NULL;
        }

        bool findPtr = false;
        for (int i=0; i< preview_buffer.nr; ++i) {
            if ((unsigned int)videoIn->buf.m.userptr == (unsigned int)mPreviewBuffer[i]) {
                findPtr = true;
                mCurrentFrameIndex = i;
                break;
            }
        }
        if (!findPtr) {
            ALOGE("buf length: %d, buf start 0x%lx, buf index: %d",videoIn->buf.length,
                  videoIn->buf.m.userptr, videoIn->buf.index);
            ALOGE("VIDIOC_DQBUF error, not find ptr");
            return NULL;
        }

        CameraYUVMeta* yuvMeta = &preview_buffer.yuvMeta[0];
        memset(yuvMeta, 0, sizeof(CameraYUVMeta));
        yuvMeta->index = mCurrentFrameIndex;
        yuvMeta->count = 1;
        yuvMeta->width = mAllocWidth;
        yuvMeta->height = mAllocHeight;
        yuvMeta->yAddr = (uint32_t)videoIn->buf.m.userptr;
        yuvMeta->yPhy = 0;
        yuvMeta->uPhy = 0;
        yuvMeta->vPhy = 0;
        if(mpreviewFormatHal == PIXEL_FORMAT_YUV422I) {
            yuvMeta->format = HAL_PIXEL_FORMAT_YCbCr_422_I;
            yuvMeta->yStride = mAllocWidth << 1;
            yuvMeta->uStride = yuvMeta->yStride;
            yuvMeta->vStride = yuvMeta->yStride;

            yuvMeta->uAddr = yuvMeta->yAddr;
            yuvMeta->vAddr = yuvMeta->yAddr;
        } else if(mpreviewFormatHal == PIXEL_FORMAT_JZ_YUV420T) {
            yuvMeta->format = HAL_PIXEL_FORMAT_JZ_YUV_420_B;
            yuvMeta->yStride = mAllocWidth << 4;
            yuvMeta->uStride = yuvMeta->yStride >> 1;
            yuvMeta->vStride = yuvMeta->uStride;

            yuvMeta->uAddr = yuvMeta->yAddr +
                (mAllocWidth*mAllocHeight) * 12 / 8;
            yuvMeta->vAddr = yuvMeta->uAddr;
        }

        ret = ::ioctl(device_fd, VIDIOC_QBUF, &videoIn->buf);
        if (ret < 0) {
            ALOGE("%s : VIDIOC_QBUF failed,err: %s",__FUNCTION__,strerror(errno));
        }
        return (void*)yuvMeta;
    }

    void* CameraV4L2Device::getMmapCurrentFrame(void) {
        int ret  = NO_ERROR;

        memset(&videoIn->buf, 0, sizeof(videoIn->buf));
        videoIn->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        videoIn->buf.memory = V4L2_MEMORY_MMAP;
        ret = ::ioctl(device_fd , VIDIOC_DQBUF, &videoIn->buf);
        if (ret < 0) {
            ALOGE("%s: VIDIOC_DQBUF failed,err: %s",__FUNCTION__,strerror(errno));
            return NULL;
        }
        if ((int)videoIn->buf.index > videoIn->mem_num) {
            ALOGE("%s: get mmap error: %s index > mem_num", __FUNCTION__, strerror(errno));
            return NULL;
        }

        mCurrentFrameIndex = videoIn->buf.index;

        CameraYUVMeta* yuvMeta = &preview_buffer.yuvMeta[0];
        memset(yuvMeta, 0, sizeof(CameraYUVMeta));
        yuvMeta->index = mCurrentFrameIndex;
        yuvMeta->count = 1;
        yuvMeta->width = mAllocWidth;
        yuvMeta->height = mAllocHeight;
        yuvMeta->yAddr = (uint32_t)videoIn->mem[mCurrentFrameIndex];
        yuvMeta->yPhy = 0;
        yuvMeta->uPhy = 0;
        yuvMeta->vPhy = 0;
        if(mpreviewFormatHal == PIXEL_FORMAT_YUV422I) {
            yuvMeta->format = HAL_PIXEL_FORMAT_YCbCr_422_I;
            yuvMeta->yStride = mAllocWidth << 1;
            yuvMeta->uStride = yuvMeta->yStride;
            yuvMeta->vStride = yuvMeta->yStride;

            yuvMeta->uAddr = yuvMeta->yAddr;
            yuvMeta->vAddr = yuvMeta->yAddr;
        } else {
            ALOGE("%s: error format: %d",__FUNCTION__, mpreviewFormatHal);
            return NULL;
        }

        if (-1 == ::ioctl (device_fd, VIDIOC_QBUF, &videoIn->buf)) {
            ALOGE("%s: qbuf error: %s",__FUNCTION__, strerror(errno));
        }
        return (void*)yuvMeta;
    }

    int CameraV4L2Device::getPreviewFrameSize(void)
    {
        return mPreviewFrameSize;
    }

    int CameraV4L2Device::getCaptureFrameSize(void)
    {
        return mPreviewFrameSize;
    }

    void CameraV4L2Device::getPreviewSize(int* w, int* h) {
        *w = mPreviewWidth;
        *h = mPreviewHeight;
    }

    void CameraV4L2Device::flushCache(void* buffer,int buffer_size) {

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

    int CameraV4L2Device::getNextFrame(void)
    {
        usleep(1000000L/m_BestPreviewFmt.getFps());
        return NO_ERROR;
    }

    unsigned int CameraV4L2Device::getPreviewFrameIndex(void)
    {
        ALOGV("Enter %s",__FUNCTION__);
        return mCurrentFrameIndex;
    }

    camera_memory_t* CameraV4L2Device::getPreviewBufferHandle(void)
    {
        ALOGV("Enter %s",__FUNCTION__);
        switch (io) {
        case IO_METHOD_READ:
            return videoIn->read_write_buffers;
            break;
        case IO_METHOD_USERPTR:
            return preview_buffer.common;
            break;
        case IO_METHOD_MMAP:
            uint8_t* ptr = (uint8_t*)mMmapPreviewBufferHandle->data
                + (videoIn->mem_length[mCurrentFrameIndex] * mCurrentFrameIndex);
            memcpy(ptr,
                   videoIn->mem[mCurrentFrameIndex],
                   videoIn->mem_length[mCurrentFrameIndex]);
            return mMmapPreviewBufferHandle;
            break;
        }
        ALOGE("%s: io %d not support",__FUNCTION__, io);
        return NULL;
    }

    camera_memory_t* CameraV4L2Device::getCaptureBufferHandle(void)
    {
        ALOGV("Enter %s",__FUNCTION__);
        return getPreviewBufferHandle();
    }

    int CameraV4L2Device::getFrameOffset(void)
    {
        int offset = 0;
        if (preview_buffer.common != NULL) {
            offset = mCurrentFrameIndex * preview_buffer.size;
        }
        return offset;
    }

    int CameraV4L2Device::setCommonMode(CommonMode mode_type, unsigned short mode_value)
    {
        status_t res = NO_ERROR;
        const char* control_name = NULL;
        const mode_map_t *table = NULL;
        int table_len = 0;

        switch(mode_type)
            {
            case WHITE_BALANCE:
                ALOGV("set white balance mode");
                control_name = CameraParameters::KEY_WHITE_BALANCE;
                table = JZCameraParameters::wb_map;
                table_len = JZCameraParameters::num_wb;
                return set_menu_ctrl(control_name, table, table_len, mode_value);
                break;
            case EFFECT_MODE:
                ALOGV("set effect mode");
                control_name = CameraParameters::KEY_EFFECT;
                table = JZCameraParameters::effect_map;
                table_len = JZCameraParameters::num_eb;
                return set_menu_ctrl(control_name, table, table_len, mode_value);
                break;
            case FOCUS_MODE:
                ALOGV("set focus mode");
                control_name = CameraParameters::KEY_FOCUS_MODE;
                table = JZCameraParameters::focus_map;
                table_len = JZCameraParameters::num_fb;
                return set_menu_ctrl(control_name, table, table_len, mode_value);
                break;
            case FLASH_MODE:
                ALOGV("set flash mode");
                control_name = CameraParameters::KEY_FLASH_MODE;
                table = JZCameraParameters::flash_map;
                table_len = JZCameraParameters::num_flb;
                return set_menu_ctrl(control_name, table, table_len, mode_value);
                break;
            case SCENE_MODE:
                ALOGV("set scene mode");
                control_name = CameraParameters::KEY_SCENE_MODE;
                table = JZCameraParameters::scene_map;
                table_len = JZCameraParameters::num_sb;
                return set_menu_ctrl(control_name, table, table_len, mode_value);
                break;
            case ANTIBAND_MODE:
                ALOGV("set antiband mode");
                control_name = CameraParameters::KEY_ANTIBANDING;
                table = JZCameraParameters::antibanding_map;
                table_len = JZCameraParameters::num_ab;
                return set_menu_ctrl(control_name, table, table_len, mode_value);
                break;
            case FLIP_HORIZONTALLY:
                {
                    ALOGV("set flip horizontally");
                    Control * control = find_control("Flip Horizontally",V4L2_CID_HFLIP);
                    if (control != NULL) {
                        res = set_boolean_ctrl(&control->control);
                    }
                    return res;
                }
                break;
            case FLIP_VERTICALLY:
                {
                    ALOGV("set flip vertically");
                    Control * control = find_control("Flip Vertically",V4L2_CID_VFLIP);
                    if (control != NULL) {
                        res = set_boolean_ctrl(&control->control);
                    }
                    return res;
                }
                break;
            case BRIGHTNESS_UP:
                {
                    ALOGV("set brightness up");
                    Control * control = find_control(NULL,V4L2_CID_BRIGHTNESS);
                    if (control != NULL) {
                        res = set_integer_ctrl_up(&control->control);
                    }
                    return res;
                }
                break;
            case BRIGHTNESS_DOWN:
                {
                    ALOGV("set brightness up");
                    Control * control = find_control(NULL,V4L2_CID_BRIGHTNESS);
                    if (control != NULL) {
                        res = set_integer_ctrl_down(&control->control);
                    }
                    return res;
                }
                break;
            default:
                ALOGE("don't support %x mode type to set",mode_type);
                return BAD_VALUE;
                break;
            }
        return NO_ERROR;
    }

    Control* CameraV4L2Device::find_control(const char* ctrl_name, int ctrl_id) {

        struct VidState* s = &(this->s);
        if (s->control_list == NULL) {
            ALOGE("init default control list error");
            return NULL;
        }

        Control *current_control = s->control_list;
        if (ctrl_id != -1) {
            for (int i=0; i<s->num_controls; ++i) {
                if ((int)current_control->control.id == ctrl_id) {
                    return current_control;
                }
                current_control = current_control->next;
            }
        } else if ((ctrl_name != NULL)
                   && (strlen(ctrl_name) > 0)) {
            for (int i=0; i<s->num_controls; ++i) {
                if (strcmp(ctrl_name, (const char*)current_control->control.name) == 0) {
                    return current_control;
                }
                current_control = current_control->next;
            }
        }
        ALOGE("%s: find ctrl: %s, id: %d error",__FUNCTION__, ctrl_name, ctrl_id);
        return NULL;
    }

    int CameraV4L2Device::find_menu(Control *control,const char* menu_name, int* value) {

        if ((control->control.type != V4L2_CTRL_TYPE_MENU)
            || (control->menu == NULL)) {
            ALOGE("%s: is not menu %s",__FUNCTION__,menu_name);
            return BAD_VALUE;
        }

        for (int i=0;
             strlen((const char*)control->menu[i].name) > 0;
             ++i) {
            if (strcmp(menu_name, (const char*)control->menu[i].name) == 0) {
                *value = control->menu[i].index;
                ALOGV("set menu name: %s, index: %d",
                      (const char*)control->menu[i].name,*value);
                return NO_ERROR;
            }
        }
        ALOGE("%s: menu name %s not find",__FUNCTION__, menu_name);
        return BAD_VALUE;
    }

    int CameraV4L2Device::set_menu_ctrl(const char* ctrl_name, const mode_map_t table[],
                                    int table_len, unsigned short mode_value) {

        int res = BAD_VALUE;
        const char* menu_name = NULL;
        struct v4l2_control control;
        Control * current_control = NULL;
        memset (&control, 0, sizeof (control));

        /* find mode menu name */
        for (int i=0; i<table_len; ++i) {
            if (table[i].mode == mode_value) {
                res = NO_ERROR;
                menu_name = table[i].dsc;
                break;
            }
        }

        if (res != NO_ERROR) {
            ALOGE("invalid %d value",mode_value);
            return res;
        }

#ifdef DEBUG_FLIP
        if (strcmp(menu_name, CameraParameters::WHITE_BALANCE_INCANDESCENT) == 0) {
            Control * control = find_control("Flip Horizontally", V4L2_CID_HFLIP);
            if (control != NULL) {
                res = set_boolean_ctrl(&control->control);
            }
            return res;
        }

        if (strcmp(menu_name, CameraParameters::WHITE_BALANCE_DAYLIGHT) == 0) {
            Control * control = find_control("Flip Vertically", V4L2_CID_VFLIP);
            if (control != NULL) {
                res = set_boolean_ctrl(&control->control);
            }
            return res;
        }
#endif

        current_control = find_control(ctrl_name, -1);
        if (current_control != NULL) {
            control.id = current_control->control.id;
            res = find_menu(current_control,menu_name,&control.value);
        }

        if (res != NO_ERROR) {
            ALOGE("invalid menu name: %s value",menu_name);
            return res;
        }

        ALOGV( "%s: id: %d, ctrl name: %s, menu name: %s, mode value: %d, value: %d",
               __FUNCTION__,control.id, ctrl_name, menu_name, mode_value,control.value);
        if ((device_fd > 0) && (-1 == ioctl (device_fd, VIDIOC_S_CTRL, &control))) {
            ALOGE("%s: device fd: %d, set %d id, mode value: %d, value: %d error: %s",__FUNCTION__,
                  device_fd,control.id, mode_value,control.value, strerror(errno));
            return BAD_VALUE;
        }
        return NO_ERROR;
    }

    int CameraV4L2Device::set_boolean_ctrl(struct v4l2_queryctrl *queryctrl) {

        int res = NO_ERROR;
        if (queryctrl->type != V4L2_CTRL_TYPE_BOOLEAN) {
            ALOGE("%s: is not boolean ctrl",__FUNCTION__);
            return BAD_VALUE;
        }

        struct v4l2_control control_s;
        memset(&control_s, 0, sizeof(struct v4l2_control));
        control_s.id = queryctrl->id;
        if ((res = ::ioctl(device_fd,VIDIOC_G_CTRL, &control_s)) < 0) {
            ALOGE("%s: get id %d error: %s",
                  __FUNCTION__, control_s.id, strerror(errno));
            return BAD_VALUE;
        }
        control_s.value = !control_s.value;
        if ((res = ::ioctl(device_fd, VIDIOC_S_CTRL, &control_s)) < 0) {
            ALOGE("%s: set id %d error: %s",__FUNCTION__,
                  control_s.id, strerror(errno));
            return BAD_VALUE;
        }
        return NO_ERROR;
    }

    int CameraV4L2Device::set_integer_ctrl_down(struct v4l2_queryctrl *queryctrl) {

        int res = NO_ERROR;
        if (queryctrl->type != V4L2_CTRL_TYPE_INTEGER) {
            ALOGE("%s: is not integer ctrl",__FUNCTION__);
            return BAD_VALUE;
        }
        struct v4l2_control control_s;
        memset(&control_s, 0, sizeof(struct v4l2_control));
        control_s.id = queryctrl->id;
        if ((res = ::ioctl(device_fd,VIDIOC_G_CTRL, &control_s)) < 0) {
            ALOGE("%s: get id %d error: %s",
                  __FUNCTION__, control_s.id, strerror(errno));
            return BAD_VALUE;
        }
        control_s.value -= queryctrl->step;
        if (control_s.value >= queryctrl->minimum) {
            if ((res = ::ioctl(device_fd, VIDIOC_S_CTRL, &control_s)) < 0) {
                ALOGE("%s: set id %d error: %s",__FUNCTION__,
                      control_s.id, strerror(errno));
                return BAD_VALUE;
            }
        } else {
            ALOGE("%s: value %d out of range [%d,%d]",__FUNCTION__,
                  control_s.value, queryctrl->minimum,
                  queryctrl->maximum);
            return BAD_VALUE;
        }
        return NO_ERROR;
    }

    int CameraV4L2Device::set_integer_ctrl_up(struct v4l2_queryctrl *queryctrl) {

        int res = NO_ERROR;
        if (queryctrl->type != V4L2_CTRL_TYPE_INTEGER) {
            ALOGE("%s: is not integer ctrl",__FUNCTION__);
            return BAD_VALUE;
        }
        struct v4l2_control control_s;
        memset(&control_s, 0, sizeof(struct v4l2_control));
        control_s.id = queryctrl->id;
        if ((res = ::ioctl(device_fd,VIDIOC_G_CTRL, &control_s)) < 0) {
            ALOGE("%s: get id %d error: %s",
                  __FUNCTION__, control_s.id, strerror(errno));
            return BAD_VALUE;
        }
        control_s.value += queryctrl->step;
        if (control_s.value <= queryctrl->maximum) {
            if ((res = ::ioctl(device_fd, VIDIOC_S_CTRL, &control_s)) < 0) {
                ALOGE("%s: set id %d error: %s",__FUNCTION__,
                      control_s.id, strerror(errno));
                return BAD_VALUE;
            }
        } else {
            ALOGE("%s: value %d out of range [%d,%d]",__FUNCTION__,
                  control_s.value, queryctrl->minimum,
                  queryctrl->maximum);
            return BAD_VALUE;
        }
        return NO_ERROR;
    }

    int CameraV4L2Device::getFormat(int format)
    {
        int tmp_format = HAL_PIXEL_FORMAT_YCbCr_422_I;
        switch(format)
            {
            case V4L2_PIX_FMT_YUYV:
                tmp_format = HAL_PIXEL_FORMAT_YCbCr_422_I;
                break;
            case V4L2_PIX_FMT_NV21:
                tmp_format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
                break;
            case V4L2_PIX_FMT_NV16:
                tmp_format = HAL_PIXEL_FORMAT_YCbCr_422_SP;
                break;
            case V4L2_PIX_FMT_RGB565:
                tmp_format = HAL_PIXEL_FORMAT_RGB_565;
                break;
            case V4L2_PIX_FMT_RGB24:
                tmp_format = HAL_PIXEL_FORMAT_RGB_888;
                break;
            case V4L2_PIX_FMT_RGB32:
                tmp_format = HAL_PIXEL_FORMAT_RGBA_8888;
                break;
            }
        return tmp_format;
    }

    bool CameraV4L2Device::getSupportPreviewDataCapture(void) {
        return isSupportHighResuPre;
    }

    bool CameraV4L2Device::getSupportCaptureIncrease(void){
        return true;
    }

    int CameraV4L2Device::getPreviewFormat(void)
    {
        return getFormat(mpreviewFormatHal);
    }

    int CameraV4L2Device::getCaptureFormat(void)
    {
        return getFormat(mpreviewFormatHal);
    }

    int CameraV4L2Device::init_param(int width, int height, int fps)
    {
        status_t res = NO_ERROR;

        ALOGV("device: %s size: %dx%d, fps: %d",
              device_name, width, height, fps);
        /* Set the format */
        memset(&videoIn->format,0,sizeof(videoIn->format));
        videoIn->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        videoIn->format.fmt.pix.width = width;
        videoIn->format.fmt.pix.height = height;
        videoIn->format.fmt.pix.pixelformat = mpreviewFormatV4l;
        videoIn->format.fmt.pix.field = V4L2_FIELD_ANY;
        res = ioctl(device_fd, VIDIOC_S_FMT, &videoIn->format);
        if (res < 0) {
            ALOGE("Open: VIDIOC_S_FMT Failed: %s", strerror(errno));
            return res;
        }

        /* Query for the effective video format used */
        memset(&videoIn->format,0,sizeof(videoIn->format));
        videoIn->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        res = ioctl(device_fd, VIDIOC_G_FMT, &videoIn->format);
        if (res < 0) {
            ALOGE("Open: VIDIOC_G_FMT Failed: %s", strerror(errno));
            return res;
        }

        unsigned int min = videoIn->format.fmt.pix.width * 2;
        if (videoIn->format.fmt.pix.bytesperline < min)
            videoIn->format.fmt.pix.bytesperline = min;
        min = videoIn->format.fmt.pix.bytesperline
             * videoIn->format.fmt.pix.height;
        if (videoIn->format.fmt.pix.sizeimage < min)
            videoIn->format.fmt.pix.sizeimage = min;

        /* Store the pixel formats we will use */
        videoIn->outWidth           = width;
        videoIn->outHeight          = height;
        // Calculate the expected output framesize in YUYV
        videoIn->outFrameSize       = width * height << 1;
        videoIn->capBytesPerPixel   = 2;

        ALOGV("Actual format: (%d x %d), Fps: %d,"
              "pixfmt: '%c%c%c%c', bytesperline: %d",
              videoIn->format.fmt.pix.width,
              videoIn->format.fmt.pix.height,
              videoIn->params.parm.capture.timeperframe.denominator,
              videoIn->format.fmt.pix.pixelformat & 0xFF,
              (videoIn->format.fmt.pix.pixelformat >> 8) & 0xFF,
              (videoIn->format.fmt.pix.pixelformat >> 16) & 0xFF,
              (videoIn->format.fmt.pix.pixelformat >> 24) & 0xFF,
              videoIn->format.fmt.pix.bytesperline);
        return res;
    }

    void CameraV4L2Device::setCameraFormat(int format) {

        switch (io) {
        case IO_METHOD_READ:
        case IO_METHOD_MMAP:
            setMmapFormat(format);
            break;
        case IO_METHOD_USERPTR:
            setUserPtrFormat(format);
            break;
        }
    }

    void CameraV4L2Device::setMmapFormat(int format) {
        ALOGV("Enter %s",__FUNCTION__);

        if (format != PIXEL_FORMAT_YUV422I) {
            ALOGE("%s: don't supprt format: 0x%x",__FUNCTION__, format);
        }
        mpreviewFormatHal = PIXEL_FORMAT_YUV422I;
        mpreviewFormatV4l = V4L2_PIX_FMT_YUYV;
    }

    void CameraV4L2Device::setUserPtrFormat(int format) {
        ALOGV("Enter %s format: %x",__FUNCTION__,format);
        if (mpreviewFormatHal != format) {
            mpreviewFormatHal = format;
        }
        if(mpreviewFormatHal == PIXEL_FORMAT_YUV422I) {
            mpreviewFormatV4l = V4L2_PIX_FMT_YUYV;
        } else if(mpreviewFormatHal == PIXEL_FORMAT_JZ_YUV420T) {
            mpreviewFormatV4l = V4L2_PIX_FMT_YUYV;
#ifdef V4L2_PIX_FMT_JZ420B
            mpreviewFormatV4l = V4L2_PIX_FMT_JZ420B;
#endif
        }
        return;
    }

    int CameraV4L2Device::setCameraParam(struct camera_param& params, int fps)
    {
        status_t res = NO_ERROR;
        int width, height;

        width = params.param.ptable[0].w;
        height = params.param.ptable[0].h;
        mPreviewWidth = width;
        mPreviewHeight = height;
        mPreviewFps = fps;

        ALOGV("%s: size: %dx%d",__FUNCTION__, mPreviewWidth, mPreviewHeight);
        return res;
    }

    void CameraV4L2Device::init_modes(int *mode,struct v4l2_querymenu *querymenu,
                                     const mode_map_t table[], int table_len) {
        if (table_len == 0) {
            ALOGE("%s: table len is 0",__FUNCTION__);
            return;
        }

        for (int i=0;
             strlen ((const char*)querymenu[i].name)>0;
             ++i) {
            for (int j=0; j<table_len; ++j) {
                if (strcmp((const char*)querymenu[i].name,
                           table[j].dsc) == 0) {
                    *mode |= table[j].mode;
                    break;
                }
            }
        }
    }

    void CameraV4L2Device::do_menu(struct v4l2_queryctrl &queryctrl,
                struct v4l2_querymenu *querymenu,
                struct sensor_info* s_info) {

        const char* ctl_name = (const char*) queryctrl.name;
        const mode_map_t *table = NULL;
        int table_len = 0;
        int *mode = NULL;

        if (querymenu == NULL
            || !(strlen(ctl_name) > 0)) {
            ALOGE("query menu is null");
            return;
        }

        if (strcmp (CameraParameters::KEY_WHITE_BALANCE,ctl_name) == 0) {
            s_info->modes.balance = 0;
            table_len = JZCameraParameters::num_wb;
            table = JZCameraParameters::wb_map;
            mode = (int*)&s_info->modes.balance;
        } else if (strcmp (CameraParameters::KEY_EFFECT,ctl_name) == 0) {
            s_info->modes.effect = 0;
            table_len = JZCameraParameters::num_eb;
            table = JZCameraParameters::effect_map;
            mode = (int*)&s_info->modes.effect;
        } else if (strcmp (CameraParameters::KEY_ANTIBANDING,ctl_name) == 0) {
            s_info->modes.antibanding = 0;
            table_len = JZCameraParameters::num_ab;
            table = JZCameraParameters::antibanding_map;
            mode = (int*)&s_info->modes.antibanding;
        } else if (strcmp (CameraParameters::KEY_SCENE_MODE, ctl_name) == 0) {
            s_info->modes.scene_mode = 0;
            table_len = JZCameraParameters::num_sb;
            table = JZCameraParameters::scene_map;
            mode = (int*)&s_info->modes.scene_mode;
        } else if (strcmp (CameraParameters::KEY_FLASH_MODE, ctl_name) == 0) {
            s_info->modes.flash_mode = 0;
            table_len = JZCameraParameters::num_flb;
            table = JZCameraParameters::flash_map;
            mode = (int*)&s_info->modes.flash_mode;
        } else if (strcmp (CameraParameters::KEY_FOCUS_MODE, ctl_name) == 0) {
            s_info->modes.focus_mode = 0;
            table_len = JZCameraParameters::num_fb;
            table = JZCameraParameters::focus_map;
            mode = (int*)&s_info->modes.focus_mode;
        } else {
            ALOGE("query ctrl name: %s not support", ctl_name);
            return;
        }
        init_modes(mode,querymenu,table,table_len);
        return;
    }

    void CameraV4L2Device::initModeValues(struct sensor_info* s_info)
    {
        struct VidState* s = &(this->s);

        if (s->control_list == NULL) {
            ALOGE("init default control list error");
            return;
        }

        Control *current_control = s->control_list;

        for (int i=0; i<s->num_controls; ++i) {
            switch (current_control->control.type) {
            case V4L2_CTRL_TYPE_INTEGER:
                ALOGV("type: integer, id: %x, name: %s, min: %d, max: %d, step: %d,"
                      "default value: %d, flag: %d",current_control->control.id,
                      current_control->control.name,current_control->control.minimum,
                      current_control->control.maximum,current_control->control.step,
                      current_control->control.default_value,current_control->control.flags);
                break;
            case V4L2_CTRL_TYPE_BOOLEAN:
                ALOGV("type: boolean, id: %x, name: %s, default value: %d, flag: %d",
                      current_control->control.id,current_control->control.name,
                      current_control->control.default_value,current_control->control.flags);
                break;
            case V4L2_CTRL_TYPE_MENU:
                ALOGV("type: menu, id: %x, name: %s, default value: %d, flag: %d",
                      current_control->control.id,current_control->control.name,
                      current_control->control.default_value,current_control->control.flags);
                do_menu(current_control->control,current_control->menu,s_info);
                break;
            case V4L2_CTRL_TYPE_BUTTON:
                ALOGV("type: button, id: %x, name: %s, default value: %d, flag: %d",
                      current_control->control.id,current_control->control.name,
                      current_control->control.default_value,current_control->control.flags);
                break;
            case V4L2_CTRL_TYPE_STRING:
                ALOGV("type: string, id: %x, name: %s, default value: %d, flag: %d",
                      current_control->control.id,current_control->control.name,
                      current_control->control.default_value,current_control->control.flags);
                break;
            case V4L2_CTRL_TYPE_INTEGER64:
                ALOGV("type: integer64, id: %x, name: %s, default value: %d, flag: %d",
                      current_control->control.id,current_control->control.name,
                      current_control->control.default_value,current_control->control.flags);
                break;
            case V4L2_CTRL_TYPE_CTRL_CLASS:
                ALOGV("type: ctrl class, id: %x, name: %s, default value: %d, flag: %d",
                      current_control->control.id,current_control->control.name,
                      current_control->control.default_value,current_control->control.flags);
                break;
            case V4L2_CTRL_TYPE_BITMASK:
                ALOGV("type: bitmask, id: %x, name: %s, default value: %d, flag: %d",
                      current_control->control.id,current_control->control.name,
                      current_control->control.default_value,current_control->control.flags);
                break;
            }
            current_control = current_control->next;
        }
    }

    void CameraV4L2Device::getSensorInfo(struct sensor_info* s_info,struct resolution_info* r_info )
    {
        status_t res = NO_ERROR;
        int flag = 0;

        ALOGV("Enter %s, state: %d",__FUNCTION__,V4L2DeviceState);

        if ((V4L2DeviceState & DEVICE_CONNECTED) != DEVICE_CONNECTED) {
            ALOGE("%s: don't connect %s",__FUNCTION__,device_name);
            return;
        }

        struct v4l2_fmtdesc fmt;
        memset (&fmt, 0, sizeof(struct v4l2_fmtdesc));
        fmt.index = 0;
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        while((res = ::ioctl(device_fd, VIDIOC_ENUM_FMT, &fmt)) == 0) {
            fmt.index++;
            switch(fmt.pixelformat)
                {
                case V4L2_PIX_FMT_NV16:
                    break;
                case V4L2_PIX_FMT_NV21:
                    break;
                case V4L2_PIX_FMT_YUYV:
                    flag = 1;
                    break;
                case V4L2_PIX_FMT_YUV420:
                    break;
                case V4L2_PIX_FMT_JPEG:
                    break;
                }
            if (flag)
                break;
        }

        if (flag == 0) {
            ALOGE("%s: do not support frame format YUYV, %d (%s)",__FUNCTION__,
                  errno, strerror(errno));
            return;
        }

        struct v4l2_frmsizeenum fsize;
        memset(&fsize, 0, sizeof(struct v4l2_frmsizeenum));
        fsize.index = 0;
        fsize.pixel_format = V4L2_PIX_FMT_YUYV;

        while ((res = ::ioctl(device_fd, VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0) {
            if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                (r_info->ptable)[fsize.index].w = fsize.discrete.width;
                (r_info->ptable)[fsize.index].h = fsize.discrete.height;

                (r_info->ctable)[fsize.index].w = fsize.discrete.width;
                (r_info->ctable)[fsize.index].h = fsize.discrete.height;

            } else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
                break;
            } else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
                break;
            }
            fsize.index++;
            s_info->prev_resolution_nr++;
            s_info->cap_resolution_nr++;
        }
        if (res != NO_ERROR && errno != EINVAL) {
            ALOGE("%s: enum frame size error, %d => (%s)",
                  __FUNCTION__, errno, strerror(errno));
        }

        initModeValues(s_info);

        return;
    }

    int CameraV4L2Device::getResolution(struct resolution_info* info)
    {
        ALOGV("Enter %s",__FUNCTION__);

        unsigned int count = m_AllFmts.size();
        if (m_AllFmts.size() > 16)
            count = 16;
        for (unsigned i = 0; i < count; ++i) {
            (info->ptable)[i].w = m_AllFmts[i].getWidth();
            (info->ptable)[i].h = m_AllFmts[i].getHeight();
        }
        return NO_ERROR;
    }

    int CameraV4L2Device::getCurrentCameraId(void)
    {
        return currentId;
    }

    int CameraV4L2Device::open_device(void) {
        struct stat st;
        if (-1 == stat(device_name, &st)) {
            ALOGE("Cannot identify '%s': %d, %s", device_name,
                  errno, strerror(errno));
            return NO_INIT;
        }

        if (!S_ISCHR(st.st_mode)) {
            ALOGE("%s is no device", device_name);
            return NO_INIT;
        }

        device_fd = open(device_name, O_RDWR|O_NONBLOCK);
        if (device_fd < 0) {
            memset(device_name,0,256);
            currentId = -1;
            V4L2DeviceState = DEVICE_UNINIT;
            ALOGE("%s: can not connect %s device", __FUNCTION__,device_name);
            return NO_INIT;
        }
        return NO_ERROR;
    }

    int CameraV4L2Device::init_device(void) {
        int res = NO_ERROR;

        memset(videoIn, 0, sizeof(struct vdIn));
        res = ::ioctl(device_fd, VIDIOC_QUERYCAP, &videoIn->cap);
        if (res != NO_ERROR) {
            ALOGE("%s: opening device unable to query device,err: %s",__FUNCTION__,strerror(errno));
            return NO_INIT;
        }

        ALOGV("driver: %s, card: %s, bus_info: %s"
              "version:%d.%d.%d, capabilities: 0x%08x",
              (const char*)(videoIn->cap.driver),
              (const char*)(videoIn->cap.card),
              (const char*)(videoIn->cap.bus_info),
              videoIn->cap.version>>16, (videoIn->cap.version>>8)&0xff,
              (videoIn->cap.version)&0xff, videoIn->cap.capabilities);

        if (!(videoIn->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
            ALOGE("%s: opening device: video capture not supported",
                  __FUNCTION__);
            return NO_INIT;
        }

        isSupportHighResuPre = false;
        if (videoIn->cap.capabilities & V4L2_CAP_STREAMING) {
            if (strcmp((const char*)videoIn->cap.card, "JZ4780-Camera") == 0) {
                io = IO_METHOD_USERPTR;
                if (0 == ::ioctl(device_fd, VIDIOC_DBG_G_CHIP_IDENT, &videoIn->chip_ident)) {
                    isSupportHighResuPre = (videoIn->chip_ident.ident==1)? true : false;
                }
                ALOGV("support user ptr I/O");
            } else {
                io = IO_METHOD_MMAP;
                isSupportHighResuPre = true;
                ALOGV("support mmap I/O");
            }
        } else if (videoIn->cap.capabilities & V4L2_CAP_READWRITE) {
            io = IO_METHOD_READ;
            isSupportHighResuPre = true;
            ALOGV("support read write I/O");
        } else {
            ALOGE("%s: not invalidy I/O",__FUNCTION__);
            return NO_INIT;
        }

        videoIn->cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (0 == ::ioctl(device_fd, VIDIOC_CROPCAP, &videoIn->cropcap)) {
            videoIn->crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            videoIn->crop.c = videoIn->cropcap.defrect;
            if (-1 == ::ioctl(device_fd, VIDIOC_S_CROP, &videoIn->crop)) {
                switch(errno) {
                case EINVAL:
                    //ALOGV("Cropping not supported,err: %s",strerror(errno));
                    break;
                default:
                    break;
                }
            }
        }
        return NO_ERROR;
    }

    int CameraV4L2Device::initPmem(int format) {

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

    void CameraV4L2Device::deInitPmem(void) {
        if (pmem_device_fd > 0) {
            close(pmem_device_fd);
            pmem_device_fd = -1;
            mPmemTotalSize = 0;
        }
    }

    int CameraV4L2Device::connectDevice(int id)
    {
        status_t res = NO_ERROR;
        ALOGV("connect %s camera, state: %d, fd: %d",device_name,
              V4L2DeviceState, device_fd);

        if (V4L2DeviceState & DEVICE_CONNECTED) {
            ALOGV("%s: %s already connect",__FUNCTION__, device_name);
            return res;
        }

        if (device_fd < 0) {
            res = open_device();
            if (res == NO_ERROR) {
                res = init_device();
            }

            if (id != currentId) {
                currentId = id;
                initDefaultControls();
                EnumFrameFormats();
            }
            V4L2DeviceState = 0;
            V4L2DeviceState |= DEVICE_CONNECTED;
        }

        if (mtlb_base == 0) {
            dmmu_init();
            dmmu_get_page_table_base_phys(&mtlb_base);
        }
        return res;
    }

    void CameraV4L2Device::disConnectDevice(void) {

        ALOGV("%s:disconnect %d camera, state = %d",
              __FUNCTION__,currentId,V4L2DeviceState);

        struct VidState* s = &(this->s);
        if (s->control_list != NULL) {
            free_control_list(s->control_list);
            s->control_list = NULL;
        }

        if (device_fd > 0) {
            close(device_fd);
            device_fd = -1;
            freeStream(PREVIEW_BUFFER);
        }

        V4L2DeviceState = DEVICE_UNINIT;
        currentId = -1;

        deInitPmem();
        if (mtlb_base > 0) {
            dmmu_deinit();
            mtlb_base = 0;
        }
        return ;
    }

    int CameraV4L2Device::start_device(void) {
        status_t res = NO_ERROR;

        ALOGV("%s: device %s state = %d",__FUNCTION__,device_name, V4L2DeviceState);

        if (V4L2DeviceState & DEVICE_STARTED) {
            ALOGV("%s: already start",__FUNCTION__);
            return res;
        }

        if (V4L2DeviceState & DEVICE_CONNECTED) {
            enum v4l2_buf_type type;
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (!videoIn->isStreaming) {
                if ((res = ::ioctl(device_fd, VIDIOC_STREAMON, &type)) != NO_ERROR) {
                    ALOGE("%s: Unable to start stream,err: %s",
                          __FUNCTION__,strerror(errno));
                    return res;
                } else {
                    videoIn->isStreaming = true;
                }
            }
            V4L2DeviceState &= ~DEVICE_STOPED;
            V4L2DeviceState |= DEVICE_STARTED;
            mReqLostFrameNum = LOST_FRAME_NUM;
            ALOGD_IF(videoIn->isStreaming,"start device succss");
        }
        return NO_ERROR;
    }

    int CameraV4L2Device::startDevice(void)
    {
        switch (io) {
        case IO_METHOD_READ:
            return NO_ERROR;
            break;
        case IO_METHOD_USERPTR:
        case IO_METHOD_MMAP:
            return start_device();
        }
        ALOGE("%s: invalidy io %d",__FUNCTION__, io);
        return BAD_VALUE;
    }

    int CameraV4L2Device::stop_device(void) {

        status_t res = NO_ERROR;

        ALOGV("%s: device state = %d",__FUNCTION__,V4L2DeviceState);

        if (V4L2DeviceState & DEVICE_STOPED) {
            ALOGV("%s: already stop",__FUNCTION__);
            return res;
        }

        if (is_capture) {
            ALOGV("%s: use preview data capture",__FUNCTION__);
            return NO_ERROR;
        }

        if (V4L2DeviceState & DEVICE_STARTED) {
            enum v4l2_buf_type type;
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (videoIn->isStreaming) {
                if ((res = ::ioctl(device_fd, VIDIOC_STREAMOFF, &type)) != NO_ERROR) {
                    ALOGE("%s: Unable stop device,err: %s",__FUNCTION__, strerror(errno));
                    return res;
                } else {
                    videoIn->isStreaming = false;
                }
            }
            close(device_fd);
            device_fd = -1;
            V4L2DeviceState &= ~DEVICE_CONNECTED;
            V4L2DeviceState &= ~DEVICE_STARTED;
            V4L2DeviceState |= DEVICE_STOPED;
            ALOGD_IF(!videoIn->isStreaming,"stop device success");
        }
        return NO_ERROR;
    }

    int CameraV4L2Device::stopDevice(void)
    {
        switch (io) {
        case IO_METHOD_READ:
            return NO_ERROR;
            break;
        case IO_METHOD_USERPTR:
        case IO_METHOD_MMAP:
            return stop_device();
            break;
        }
        ALOGE("%s: invalidy io %d",__FUNCTION__, io);
        return BAD_VALUE;
    }

    int CameraV4L2Device::getCameraModuleInfo(int camera_id, struct camera_info* info)
    {
        if ((mglobal_info.sensor_count == 1)
            || (camera_id == 1)) {
            info->facing = CAMERA_FACING_FRONT;
            info->orientation = 0;
            ALOGV("%s: id: %d, facing: %s, orientation: %d",__FUNCTION__, 1,"front camera",0);
        } else if (camera_id == 0) {
            info->facing = CAMERA_FACING_BACK;
            info->orientation = 0;
            ALOGV("%s: id: %d, facing: %s, orientation: %d",__FUNCTION__, 0,"back camera", 0);
        }
        return NO_ERROR;
    }

    int CameraV4L2Device::getCameraNum(void)
    {
        int nr = 0;
        nr = mglobal_info.sensor_count;
        ALOGV("%s: num camera: %d",__FUNCTION__,nr);
        return nr;
    }

    int CameraV4L2Device::sendCommand(uint32_t cmd_type, uint32_t arg1, uint32_t arg2, uint32_t result)
    {
        status_t res = NO_ERROR;

        ALOGV("%s:device state = %d, cmd_type = %d",__FUNCTION__,V4L2DeviceState, cmd_type);
        switch(cmd_type)
            {
            case FOCUS_INIT:
                ALOGV("FOCUS_INIT");
                is_capture = false;
                break;
            case PAUSE_FACE_DETECT:
                ALOGV("PAUSE_FACE_DETECT");
                break;
            case START_FOCUS:
                ALOGV("START_FOCUS");
                break;
            case GET_FOCUS_STATUS:
                ALOGV("GET_FOCUS_STATUS");
                break;
            case START_PREVIEW:
                ALOGV("START_PREVIEW");
                break;
            case STOP_PREVIEW:
                ALOGV("STOP_PREVIEW");
                break;
            case START_ZOOM:
                ALOGV("START_ZOOM");
                mEnablestartZoom = true;
                break;
            case STOP_ZOOM:
                ALOGV("STOP_ZOOM");
                mEnablestartZoom = false;
                break;
            case START_FACE_DETECT:
                ALOGV("START_FACE_DETECT");
                break;
            case STOP_FACE_DETECT:
                ALOGV("STOP_FACE_DETECT");
                break;
            case INIT_TAKE_PICTURE:
                ALOGV("INIT_TAKE_PICTURE");
                is_capture = true;
                break;
            case TAKE_PICTURE: {
                ALOGV("TAKE_PICTURE");
                void* ptr = getCurrentFrame();
                if (ptr != NULL) {
                    return (int)ptr;
                } else {
                    ALOGE("%s: get frame error",__FUNCTION__);
                    return 0;
                }
            }
                break;
            case STOP_PICTURE:
                ALOGV("STOP_PICTURE");
                is_capture = false;
                break;
            }

        return NO_ERROR;
    }

    void CameraV4L2Device::initTakePicture(int width,int height,
         camera_request_memory get_memory) {
        ALOGV("Enter %s,state: %d",__FUNCTION__, V4L2DeviceState);

        int res = NO_ERROR;
        res = connectDevice(currentId);

        if (res == NO_ERROR) {
            mpreviewFormatV4l = V4L2_PIX_FMT_YUYV;
            res = allocateStream(PREVIEW_BUFFER,get_memory,
                width,height,HAL_PIXEL_FORMAT_YCbCr_422_I);
        }

        if (res == NO_ERROR) {
            res = startDevice();
        }
        if (res == NO_ERROR) {
            mReqLostFrameNum = 0;
        }
        ALOGE_IF(res != NO_ERROR, "%s: error: %s",__FUNCTION__, strerror(errno));
    }

    void CameraV4L2Device::deInitTakePicture(void) {
        ALOGV("Enter %s",__FUNCTION__);
    }

    bool CameraV4L2Device::getZoomState(void) {
        ALOGV("Enter %s",__FUNCTION__);
        return mEnablestartZoom;
    }

    void CameraV4L2Device::EnumFrameFormats()
    {
        ALOGV( "Enter %s",__FUNCTION__);
        
        struct v4l2_fmtdesc fmt;
        m_AllFmts.clear();

        memset(&fmt, 0, sizeof(struct v4l2_fmtdesc));
        fmt.index = 0;
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        while(::ioctl(device_fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
            fmt.index++;

            if (!EnumFrameSizes(fmt.pixelformat)) {
                ALOGE("Unable to enumerate frame sizes.");
            }
        }
        
        m_BestPreviewFmt = frameInterval();
        m_BestPictureFmt = frameInterval();

        unsigned int i;
        for (i=0; i < m_AllFmts.size(); i++) {
            frameInterval f = m_AllFmts[i];

            if ((f.getWidth() > m_BestPictureFmt.getWidth()
                 && f.getHeight() > m_BestPictureFmt.getHeight())
                || ((f.getWidth() == m_BestPictureFmt.getWidth()
                    && f.getHeight() == m_BestPictureFmt.getHeight())
                    && f.getFps() < m_BestPictureFmt.getFps())) {
                m_BestPictureFmt = f;
            }

            if ((f.getFps() > m_BestPreviewFmt.getFps()) ||
                (f.getFps() == m_BestPreviewFmt.getFps()
                  && (f.getWidth() > m_BestPictureFmt.getWidth()
                  && f.getHeight() > m_BestPictureFmt.getHeight()))) {
                m_BestPreviewFmt = f;
            }
        }
        return;
    }

    bool CameraV4L2Device::EnumFrameSizes(int pixfmt)
    {
        int ret = 0;
        int fsizeind = 0;
        struct v4l2_frmsizeenum fsize;

        memset(&fsize, 0, sizeof(fsize));
        fsize.index = 0;
        fsize.pixel_format = pixfmt;

        while(::ioctl(device_fd, VIDIOC_ENUM_FRAMESIZES, &fsize) >= 0) {
            fsize.index++;
            if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                fsizeind++;
                if (!EnumFrameIntervals(pixfmt, fsize.discrete.width, fsize.discrete.height)) {
                    ALOGE("Unable to enumerate frame indervals");
                }
            }else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
                ALOGE("Will not enumerate frame intervals.");
            } else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
                ALOGE("Will not enumerate frame intervals.");
            } else {
                ALOGE("fsize.type not supported: %d", fsize.type);
            }
        }
        return fsizeind != 0;
    }

    bool CameraV4L2Device::EnumFrameIntervals(int pixfmt, int width, int height)
    {
        struct v4l2_frmivalenum fival;
        int list_fps = 0;
        memset(&fival, 0, sizeof(fival));
        fival.index = 0;
        fival.pixel_format = pixfmt;
        fival.width = width;
        fival.height = height;

        while(::ioctl(device_fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival) >= 0) {
            fival.index++;
            if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                m_AllFmts.add(frameInterval(width, height, fival.discrete.denominator));
                list_fps++;
            } else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
                break;
            } else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
                break;
            }
        }
        if (list_fps == 0) {
            m_AllFmts.add(frameInterval(width, height, 1));
        }
        return true;
    }

    void CameraV4L2Device::initDefaultControls(void) {

        struct VidState* s = &(this->s);
        if (s->control_list != NULL) {
            free_control_list(s->control_list);
            s->control_list = NULL;
        }

        s->num_controls = 0;
        s->control_list = get_control_list(device_fd, &(s->num_controls));
        
        if (s->control_list == NULL) {
            ALOGE("Error: empty control list");
            return;
        }
        return;
    }

    void CameraV4L2Device::free_control_list(Control* control_list)
    {
        Control* first = control_list;
        Control* next = control_list->next;
        while(next != NULL) {
            if (first->str != NULL) {
                free(first->str);
                first->str = NULL;
            }
            if (first->menu != NULL) {
                free(first->menu);
                first->menu = NULL;
            }
            free(first);
            first = next;
            next = first->next;
        }
        if (first->str != NULL) {
            free(first->str);
            first->str = NULL;
        }
        if (first != NULL) {
            free(first);
            first = NULL;
        }
        control_list = NULL;
    }

    Control* CameraV4L2Device::get_control_list(int hdevice, int* num_ctrls) {

        int ret = 0;
        Control* first = NULL;
        Control* current = NULL;
        Control* control = NULL;

        int n = 0;
        struct v4l2_queryctrl queryctrl;
        struct v4l2_querymenu querymenu;

        memset(&queryctrl, 0, sizeof(struct v4l2_queryctrl));
        memset(&querymenu, 0, sizeof(struct v4l2_querymenu));

        int currentctrl;
        for (currentctrl=V4L2_CID_BASE;
               currentctrl<V4L2_CID_LASTP1;
               currentctrl++) {
            struct v4l2_querymenu *menu = NULL;
            queryctrl.id = currentctrl;
            ret = ::ioctl(hdevice, VIDIOC_QUERYCTRL, &queryctrl);
            if (ret || (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED))
                continue;
            ALOGV("ID: 0x%x, Control name: %s", queryctrl.id, (const char*)queryctrl.name);
            if (queryctrl.type == V4L2_CTRL_TYPE_MENU) {
                int i = 0;
                int ret = 0;
                for (querymenu.index = queryctrl.minimum;
                     (int)(querymenu.index) <= queryctrl.maximum;
                     querymenu.index++) {
                    querymenu.id = queryctrl.id;
                    ret = ::ioctl(hdevice ,VIDIOC_QUERYMENU , &querymenu);
                    if (ret < 0)
                        break;
                    ALOGV("    menu name: %s",(const char*)querymenu.name);
                    if (!menu)
                        menu = (struct v4l2_querymenu*)calloc((i+1),sizeof(struct v4l2_querymenu));
                    else
                        menu = (struct v4l2_querymenu*)realloc(menu, (i+1) * sizeof(struct v4l2_querymenu));
                    memcpy(&(menu[i]), &querymenu, sizeof(struct v4l2_querymenu));
                    i++;
                }

                if (!menu)
                    menu = (struct v4l2_querymenu*)calloc((i+1),sizeof(struct v4l2_querymenu));
                else
                    menu = (struct v4l2_querymenu*)realloc(menu, (i+1) * sizeof(struct v4l2_querymenu));

                menu[i].id = querymenu.id;
                menu[i].index = queryctrl.maximum+1;
                menu[i].name[0] = 0;
            }

            control = (Control*)calloc(1, sizeof(Control));
            memcpy(&(control->control), &queryctrl, sizeof(struct v4l2_queryctrl));
            control->ctrl_class = 0x00980000;
            control->menu = menu;

            if (first != NULL) {
                current->next = control;
                current = control;
            } else {
                first = control;
                current = control;
            }
            n++;
        }

        for (queryctrl.id=V4L2_CID_PRIVATE_BASE; ;
             queryctrl.id++) {
            struct v4l2_querymenu *menu = NULL;
            ret = ::ioctl(hdevice, VIDIOC_QUERYCTRL, &queryctrl);
            if (ret)
                break;
            else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                continue;
            ALOGV("ID: 0x%x, Control name: %s", queryctrl.id, (const char*)queryctrl.name);
            if (queryctrl.type == V4L2_CTRL_TYPE_MENU) {
                int i = 0;
                int ret = 0;
                for (querymenu.index = queryctrl.minimum;
                     (int)(querymenu.index) <= queryctrl.maximum;
                     querymenu.index++) {
                    querymenu.id = queryctrl.id;
                    ret = ::ioctl(hdevice, VIDIOC_QUERYMENU, &querymenu);
                    if (ret < 0)
                        break;
                    ALOGV("    menu name: %s",(const char*)querymenu.name);
                    if (!menu)
                        menu = (struct v4l2_querymenu*)calloc(i+1, sizeof(struct v4l2_querymenu));
                    else
                        menu = (struct v4l2_querymenu*)realloc(menu, (i+1) * sizeof(struct v4l2_querymenu));
                    memcpy(&(menu[i]), &querymenu, sizeof( struct v4l2_querymenu));
                    i++;
                }
                if (!menu)
                    menu = (struct v4l2_querymenu*) calloc(i+1, sizeof(struct v4l2_querymenu));
                else
                    menu = (struct v4l2_querymenu*)realloc(menu, (i+1)*(sizeof(struct v4l2_querymenu)));
                menu[i].id = querymenu.id;
                menu[i].index = queryctrl.maximum+1;
                menu[i].name[0]=0;
            }

            control = (Control*)calloc(1, sizeof(Control));
            memcpy(&(control->control),&queryctrl, sizeof(struct v4l2_queryctrl));
            control->menu = menu;

            if (first != NULL) {
                current->next = control;
                current = control;
            } else {
                first = control;
                current = first;
            }
            n++;
        }
        *num_ctrls = n;
        ALOGV("%s: num ctrl: %d",__FUNCTION__, n);
        return first;
    }
};
