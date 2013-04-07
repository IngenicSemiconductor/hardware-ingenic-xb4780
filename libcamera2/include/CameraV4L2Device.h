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

#ifndef __CAMERA_V4L2_DEVICE_H_
#define __CAMERA_V4L2_DEVICE_H_

#include <utils/SortedVector.h>
#include "CameraDeviceCommon.h"
#include "CameraColorConvert.h"
#include "JZCameraParameters.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define NB_BUFFER 3

namespace android {

    typedef enum {
        IO_METHOD_READ,
        IO_METHOD_MMAP,
        IO_METHOD_USERPTR,
    }io_method;

    struct vdIn {
        struct v4l2_capability cap;
        struct v4l2_cropcap cropcap;
        struct v4l2_crop crop;
        struct v4l2_format format;              // Capture format being used
        struct v4l2_buffer buf;
        struct v4l2_requestbuffers rb;
        struct v4l2_streamparm params;          // v4l2 stream parameters struct
        struct v4l2_jpegcompression jpegcomp;   // v4l2 jpeg compression settings
        struct v4l2_dbg_chip_ident chip_ident;

        void *mem[NB_BUFFER];
        int  mem_length[NB_BUFFER];
        int  mem_num;
        bool isStreaming;

        camera_memory_t *read_write_buffers;

        int outWidth;                           // Requested Output width
        int outHeight;                          // Requested Output height
        int outFrameSize;                       // The expected output framesize (in YUYV)
        int capBytesPerPixel;                   // Capture bytes per pixel
        int capCropOffset;                      // The offset in bytes to add to the captured buffer to get to the first pixel
    };
     
    struct frameInterval {
        int width;
        int height;
        int fps;
       
        frameInterval(int w, int h, int f)
            :width(w),
             height(h),
             fps(f)
        {}

        frameInterval()
            :width(0),
             height(0),
             fps(0) {}
         
        const frameInterval& operator=(const frameInterval& v){
            this->width = v.width;
            this->height = v.height;
            this->fps = v.fps;
            return *this;
        }

        inline int getWidth() const { return width;}
        inline int getHeight() const { return height;}
        inline int getFps() const { return fps;}
        inline void setSize(int width, int height) {
            this->width = width;
            this->height = height;
        }
        inline void setFps(int pfps) { this->fps = pfps;}
        inline bool operator <(const frameInterval& other) const {
            return compare(other) < 0;
        }
        inline bool operator <=(const frameInterval& other) const {
            return compare(other) <= 0;
        }
        inline bool operator ==(const frameInterval& other) const {
            return compare(other) == 0;
        }
        inline bool operator !=(const frameInterval & other) const {
            return compare(other) != 0;
        }
        inline bool operator >=(const frameInterval& other) const {
            return compare(other) >= 0;
        }
        inline bool operator >(const frameInterval& other) const {
            return compare(other) > 0;
        }
        inline int compare(const frameInterval& other) const {

            int r = 0;
            if (this->width > other.width && this->height > other.height)
                r = 1;
            if (this->width < other.width && this->height < other.height)
                r = -1;

            if (r != 0)
                return r;
            r = this->fps - other.fps;
            return r;
        }
    };

    typedef struct _Control {
        struct v4l2_queryctrl control;
        struct v4l2_querymenu *menu;
        int32_t ctrl_class;
        int32_t value;
        int64_t value64;
        char* str;
        struct _Control* next;
    }Control;

    struct VidState {
        Control* control_list;
        int num_controls;
        int width_req;
        int height_req;
    };

    class CameraV4L2Device : public CameraDeviceCommon {

    public:
        static CameraV4L2Device* getInstance();

    protected:
        CameraV4L2Device();
        ~CameraV4L2Device();

    public:
        int allocateStream(BufferType type, camera_request_memory get_memory,
                           uint32_t width,
                           uint32_t height,
                           int format);
        void freeStream(BufferType type);
        void* getCurrentFrame(void);
        int getPreviewFrameSize(void);
        int getCaptureFrameSize(void);
        void getPreviewSize(int* w, int* h);
        int getNextFrame(void);
        int getFrameOffset(void);
        unsigned int getPreviewFrameIndex(void);
        camera_memory_t* getPreviewBufferHandle(void);
        camera_memory_t* getCaptureBufferHandle(void);
        int getCaptureFormat(void);
        int getPreviewFormat(void);
        int setCommonMode(CommonMode mode_type, unsigned short mode_value);
        void setCameraFormat(int format);
        int setCameraParam(struct camera_param &param, int fps);
        int getResolution(struct resolution_info* info);
        void getSensorInfo(struct sensor_info* s_info,struct resolution_info* r_info );
        bool getSupportPreviewDataCapture(void);
        bool getSupportCaptureIncrease(void);
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
        void setDeviceCount(int num);
        void update_device_name(const char* deviceName, int len);

        bool usePmem(void) {
            return preview_use_pmem;
        }

        unsigned long getTlbBase(void) {
            return mtlb_base;
        }

        void flushCache(void* buffer,int buffer_size);
        void* read_frame(void);
        void dump_sensor_data(void *frame_buffer);

    private:
        void initGlobalInfo(void);
        int  init_param(int width, int height, int fps);
        int  getFormat(int);
        void initModeValues(struct sensor_info*);
        void init_modes(int *mode,struct v4l2_querymenu *querymenu,
                        const mode_map_t table[], int table_len);
        void do_menu(struct v4l2_queryctrl &queryctrl,
                     struct v4l2_querymenu *querymenu,
                     struct sensor_info *s_info);
        Control* get_control_list(int hdevice, int* num_ctrls);
        void free_control_list(Control* control_list);
        void initDefaultControls(void);
        bool EnumFrameIntervals(int pixfmt, int width, int height);
        bool EnumFrameSizes(int pixfmt);
        void EnumFrameFormats();
        int set_boolean_ctrl(struct v4l2_queryctrl *queryctrl);
        int set_integer_ctrl_up(struct v4l2_queryctrl *queryctrl);
        int set_integer_ctrl_down(struct v4l2_queryctrl *queryctrl);
        int set_menu_ctrl(const char* name, const mode_map_t table[],
                  int table_len, unsigned short mode_value);
        int open_device(void);
        int stop_device(void);
        int start_device(void);
        int init_device(void);
        int init_read(unsigned int buffer_size,camera_request_memory get_memory);
        void dmmu_map_buffer(struct dmmu_mem_info *dmmu_info);
        int init_mmap(camera_request_memory get_memory);
        int init_userp(uint32_t width, uint32_t height,
                       camera_request_memory get_memory,int format);
        void freeReadWritePreviewBuffer(void);
        void freeMMapPreviewBuffer(void);
        void freePreviewBuffer(BufferType type);
        void* getReadWriteCurrentFrame(void);
        void* getUserPtrCurrentFrame(void);
        void* getMmapCurrentFrame(void);
        void  setMmapFormat(int format);
        void  setUserPtrFormat(int format);
        Control* find_control(const char* ctrl_name,int ctrl_id);
        int find_menu(Control *control,const char* menu_name, int* value);
        int initPmem(int format);
        void deInitPmem(void);
    private:
        mutable Mutex mlock;
        int device_fd;
        int V4L2DeviceState;
        int currentId;
        unsigned int mtlb_base;
        bool need_update;
        struct global_info mglobal_info;
        char device_name[256];
        struct vdIn *videoIn;
        camera_memory_t* mMmapPreviewBufferHandle;
        int mCurrentFrameIndex;
        SortedVector<frameInterval> m_AllFmts;
        struct VidState s;
        frameInterval m_BestPreviewFmt;
        frameInterval m_BestPictureFmt;
        struct camera_buffer preview_buffer;
        struct camera_buffer capture_buffer;
        void* mPreviewBuffer[NB_BUFFER];
        size_t mPreviewFrameSize;
        size_t mPmemTotalSize;
        int mPreviewWidth;
        int mPreviewHeight;
        int mPreviewFps;
        int mAllocWidth;
        int mAllocHeight;
        bool preview_use_pmem;
        bool capture_use_pmem;
        bool mEnablestartZoom;
        int pmem_device_fd;
        int mpreviewFormatHal;
        int mpreviewFormatV4l;
        bool is_capture;
        io_method io;
        bool isChangedSize;
        bool isSupportHighResuPre;
        int mReqLostFrameNum;
    public:
        static Mutex sLock;
        static CameraV4L2Device* sInstance;
    };
};
#endif
