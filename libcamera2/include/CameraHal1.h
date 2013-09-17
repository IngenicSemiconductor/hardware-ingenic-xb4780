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

#ifndef __CAMERA_HW_HAL1_H_
#define __CAMERA_HW_HAL1_H_

#include "CameraHalCommon.h"
#include "CameraColorConvert.h"
#include "CameraFaceDetect.h"

//#define USE_X2D
#define X2D_NAME "/dev/x2d"
#define X2D_SCALE_FACTOR 512.0

#define SIGNAL_RESET_PREVIEW     (SIGNAL_THREAD_COMMON_LAST<<1)
#define SIGNAL_TAKE_PICTURE      (SIGNAL_THREAD_COMMON_LAST<<2)
#define SIGNAL_RECORDING_START      (SIGNAL_THREAD_COMMON_LAST<<3)

namespace android {

    class CameraHal1 : public CameraHalCommon {

    public:
        CameraHal1(int id, CameraDeviceCommon* device);
        ~CameraHal1();

    public:
        void update_device(CameraDeviceCommon* device);
        int module_open(const hw_module_t* module, const char* id, hw_device_t** device);

        bool getModuleInit() {
            return mModuleOpened;
        }

        int get_number_cameras(void);
        int get_cameras_info(int camera_id, struct camera_info* info);

    public:
        status_t initialize(void);
        status_t setPreviewWindow(struct preview_stream_ops *window);
        void setCallbacks(camera_notify_callback notify_cb,
                                  camera_data_callback data_cb,
                                  camera_data_timestamp_callback data_cb_timestamp,
                                  camera_request_memory get_memory,
                                  void* user);
        void enableMsgType(int32_t msg_type);
        void disableMsgType(int32_t msg_type);
        int isMsgTypeEnabled(int32_t msg_type);
        status_t startPreview(void);
        void stopPreview(void);
        int isPreviewEnabled(void);
        status_t storeMetaDataInBuffers(int enable);
        status_t startRecording(void);
        void stopRecording(void);
        int isRecordingEnabled(void);
        void releaseRecordingFrame(const void* opaque);
        status_t setAutoFocus(void);
        status_t cancelAutoFocus(void);
        status_t takePicture(void);
        status_t cancelPicture(void);
        status_t setParameters(const char* parms);
        char* getParameters(void);
        void putParameters(char* params);
        status_t sendCommand(int32_t cmd, int32_t arg1, int32_t arg2);
        void releaseCamera(void);
        status_t dumpCamera(int fd);
        int deviceClose(void);

    private:

        bool NegotiatePreviewFormat(struct preview_stream_ops* win);
        void initVideoHeap(int w, int h);
        void initPreviewHeap(void);
        void resetPreview(void);
        void open_ipu_dev(void);

        int init_ipu_dev(int w, int h, int mat, int must_do);

        void close_ipu_dev(void);

        void open_x2d_dev(void);

        void close_x2d_dev(void);

        CameraDeviceCommon* getDevice(void) {
            return mDevice;
        }

        size_t num2even(int num){
            size_t even;

            even = num % 2 ? num-1 : num;
            if(even == 682)
                even = 708;
            else if(even == 778)
                even = 776;

            return even;
        }

        void dmmu_map_memory(uint8_t* addr, int size) {
            struct dmmu_mem_info dmmu_info;

            dmmu_info.vaddr = (void*)addr;
            dmmu_info.size = size;
            for (int i = 0; i < (int)size; i += 0x1000) {
                addr[i] = 0;
            }
            addr[size - 1] = 0;
            dmmu_map_user_memory(&dmmu_info);
        }

        void dmmu_unmap_memory(uint8_t* addr, int size) {
            struct dmmu_mem_info dmmu_info;

            dmmu_info.vaddr = (void*)addr;
            dmmu_info.size = size;
            dmmu_unmap_user_memory(&dmmu_info);
        }

        unsigned int GetTimer(void)
        {
            struct timeval tv;
            //float s;
            gettimeofday(&tv,NULL);
            return tv.tv_sec * 1000000 + tv.tv_usec;
        }

        void do_zoom(uint8_t* dest, uint8_t* src);

        void ipu_convert_dataformat(CameraYUVMeta* yuvMeta,
             uint8_t* dst_buf, buffer_handle_t *buffer);
        status_t ipu_zoomIn_scale(uint8_t* dest, int dest_width, int dest_height,
                 uint8_t* src, int src_width, int src_height, int src_format, int stride_mul, int src_stride);

        void x2d_convert_dataformat(CameraYUVMeta* yuvMeta, 
                                    uint8_t* dst_buf, buffer_handle_t *buffer);

        void dump_data(bool isdump);

    private:

        mutable Mutex mlock;
        mutable Mutex mlock_recording;
        int mcamera_id;
        bool mirror;
        CameraDeviceCommon* mDevice;
        CameraColorConvert* ccc;
        camera_device_t* mCameraModuleDev;
        bool mModuleOpened;

        camera_notify_callback mnotify_cb;
        camera_data_callback mdata_cb ;
        camera_data_timestamp_callback mdata_cb_timestamp;
        camera_request_memory mget_memory;
        void* mcamera_interface;

        JZCameraParameters* mJzParameters;
        int32_t mMesgEnabled;

        preview_stream_ops* mPreviewWindow;
        nsecs_t mPreviewAfter;
        int mRawPreviewWidth;
        int mRawPreviewHeight;

        int mPreviewFmt;
        int mPreviewWidth;
        int mPreviewHeight;
        int mPreviewFrameSize;
        camera_memory_t* mPreviewHeap;
        int mPreviewIndex;
        mutable Mutex mcapture_lock;
        List<camera_memory_t*> mListCaptureHeap;

        bool mPreviewEnabled;
        int mRecordingFrameSize;
        camera_memory_t* mRecordingHeap;
        int mRecordingindex;
        bool mVideoRecEnabled;
        int mtotaltime;
        int mtotalnum;
        bool mTakingPicture;
        int mPicturewidth;
        int mPictureheight;
        int mPreviewWinFmt;
        int mPrebytesPerPixel;
        int mPreviewWinWidth;
        int mPreviewWinHeight;

        nsecs_t mCurFrameTimestamp;
        nsecs_t mLastFrameTimestamp;
        CameraYUVMeta* mCurrentFrame;
        int mFaceCount;
        int mzoomVal;
        int mzoomRadio;

        bool isSoftFaceDetectStart;
        struct ipu_image_info * mipu;
        bool ipu_open_status;
        bool init_ipu_first;
        int x2d_fd;

        volatile bool mreceived_cmd;
        mutable Mutex cmd_lock;
        Condition mreceivedCmdCondition;

        sp<SensorListener> mSensorListener;
    private:
        bool thread_body(void);
        void postFrameForPreview(void);
        void postFrameForNotify(void);
        void postJpegDataToApp(void);
        void softCompressJpeg(void);
        void hardCompressJpeg(void);
        status_t fillCurrentFrame(uint8_t* img,buffer_handle_t* buffer);
        status_t convertCurrentFrameToJpeg(camera_memory_t** jpeg_buff);
        status_t softFaceDetectStart(int32_t detect_type);
        status_t softFaceDetectStop(void);
        status_t do_takePictureWithPreview(void);
        status_t do_takePicture(void);
        status_t completeTakePicture(void);
        void completeRecordingVideo(void);
        int getCurrentFrameSize(void);

    private:
        friend class Hal1SignalThread;
        class Hal1SignalThread : public SignalDrivenThread {

        private:
            CameraHal1* signalHal1;

        public:
             Hal1SignalThread(CameraHal1* hal)
                : SignalDrivenThread() {
                signalHal1 = hal;
            }

            ~Hal1SignalThread() {
            }

            void release() {
                SetSignal(SIGNAL_THREAD_TERMINATE);
            }

            status_t readyToRunInternal(void) {
                return NO_ERROR;
            }

            void threadFuntionInternal(void) {
                uint32_t signal = GetProcessingSignal();
                if (signal & SIGNAL_THREAD_TERMINATE) {
                    SetSignal(SIGNAL_THREAD_TERMINATE);
                } else if (signal & SIGNAL_RESET_PREVIEW) {
                    signalHal1->resetPreview();
                } else if (signal & SIGNAL_TAKE_PICTURE) {
                    signalHal1->completeTakePicture();
                }
                return;
            }
        };

    private:
        sp<Hal1SignalThread> mHal1SignalThread;

    private:
        friend class Hal1SignalRecordingVideo;
        class Hal1SignalRecordingVideo : public SignalDrivenThread {

        private:
            CameraHal1* signalHal1;

        public:
             Hal1SignalRecordingVideo(CameraHal1* hal)
                : SignalDrivenThread() {
                signalHal1 = hal;
            }

            ~Hal1SignalRecordingVideo() {
            }

            void release() {
                SetSignal(SIGNAL_THREAD_TERMINATE);
            }

            status_t readyToRunInternal(void) {
                return NO_ERROR;
            }

            void threadFuntionInternal(void) {
                uint32_t signal = GetProcessingSignal();
                if (signal & SIGNAL_THREAD_TERMINATE) {
                    SetSignal(SIGNAL_THREAD_TERMINATE);
                } else if (signal & SIGNAL_RECORDING_START) {
                    signalHal1->completeRecordingVideo();
                }
                return;
            }
        };

    private:
        sp<Hal1SignalRecordingVideo> mHal1SignalRecordingVideo;
        Vector<camera_memory_t*> mRecordingDataQueue;
        mutable Mutex recordingDataQueueLock;

    private:
        friend class WorkThread;
        class WorkThread : public Thread {

        private:
            CameraHal1* mCameraHal;
            int mThreadControl;
            int mControlFd;
            bool mOnce;
            mutable Mutex release_recording_frame_lock;
            Condition release_recording_frame_condition;
            int mrelease_recording_frame;
            bool changed;
            nsecs_t start;
            nsecs_t timeout;

        public:
            enum ControlCmd {
                THREAD_TIMEOUT,
                THREAD_READY,
                THREAD_IDLE,
                THREAD_EXIT,
                THREAD_STOP,
                THREAD_ERROR
            };

        public:
            inline explicit WorkThread(CameraHal1 * ch1)
                :Thread(false),
                 mCameraHal(ch1),
                 mThreadControl(-1),
                 mControlFd(-1),
                 mOnce(false),
                 release_recording_frame_lock("WorkThread::lock"),
                 mrelease_recording_frame(0),
                 changed(true),
                 start(0),
                 timeout(3000000000LL)
            {
            }

            inline ~WorkThread()
            {
                if (mThreadControl >= 0)
                    close(mThreadControl);
                if (mControlFd >= 0)
                    close(mControlFd);
                mThreadControl = -1;
                mControlFd = -1;
            }

            inline status_t startThread(bool once)
            {
                mOnce = once;
                return run ("CameraThread", ANDROID_PRIORITY_URGENT_DISPLAY, 0);
            }

            void threadResume(void)
            {
                AutoMutex lock(release_recording_frame_lock);
                changed = true;
                timeout = systemTime(SYSTEM_TIME_MONOTONIC) - start;
                mrelease_recording_frame--;
                release_recording_frame_condition.signal();
            }

            void threadPause(void)
            {
                AutoMutex lock(release_recording_frame_lock);

                if (changed) {
                    changed = false;
                    start = systemTime(SYSTEM_TIME_MONOTONIC);
                }

                while (mrelease_recording_frame == RECORDING_BUFFER_NUM) {
                    release_recording_frame_condition.waitRelative(release_recording_frame_lock,
                                                                   timeout);
                }

                mrelease_recording_frame++;
            }

            status_t readyToRun() {

                int thread_fds[2];
                if (pipe(thread_fds) == 0) {
                    mThreadControl = thread_fds[1]; //write
                    mControlFd = thread_fds[0]; //read
#ifdef USE_X2D
                    mCameraHal->open_x2d_dev();
#else
                    mCameraHal->open_ipu_dev();
#endif
                    changed = true;
                    start = 0;
                    timeout = 3000000000LL;

                    return NO_ERROR;
                }

                return errno;
            }

            int sendMesg(ControlCmd msg) {
                return sendMesgDelay(msg, 0);
            }

            int sendMesgDelay(ControlCmd msg, int64_t delay) {

                struct timespec req;
                memset(&req, 0, sizeof(struct timespec));
                req.tv_sec = delay / 1000000000LL;
                req.tv_nsec = (delay - (req.tv_sec *1000000000LL));
                if (mThreadControl >= 0) {
                    nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
                    nsecs_t uptime = now + delay;
                    while (uptime > systemTime(SYSTEM_TIME_MONOTONIC))
                        nanosleep(&req, &req);
                    const int wres = write(mThreadControl, &msg, sizeof(msg));
                    if (wres == sizeof(msg)) {
                        return 0;
                    }
                }
                ALOGE("%s: send cmd %d error, contorl fd:%d",__FUNCTION__,
                      msg, mThreadControl);
                return -1;
            }

            status_t stopThread() {

                status_t res = NO_ERROR;
                if (mThreadControl >= 0) {
                    const ControlCmd cmd = THREAD_EXIT;
                    const int wres = write(mThreadControl,&cmd,sizeof(cmd));

                    if (wres == sizeof(cmd)) {
                        res = requestExitAndWait();
                    }

                    if (res == NO_ERROR) {
                        close(mThreadControl);
                        close(mControlFd);
                        mThreadControl = -1;
                        mControlFd = -1;
                    }
                }
                return res;
            }
            ControlCmd receiveCmd(int fd, int64_t timeout);
        private:
            bool threadLoop()
            {
                if (mCameraHal->thread_body())
                    {
                        return !mOnce;
                    }
                return false;
            }
        };

        inline WorkThread* getWorkThread()
        {
            return mWorkerThread.get();
        }

    private:
        sp<WorkThread> mWorkerThread;

    private:
        bool startAutoFocus();
        friend class AutoFocusThread;
        class AutoFocusThread : public Thread {
        private:
            CameraHal1* mCameraHal;
            bool stoped;
        public:
            inline explicit AutoFocusThread(CameraHal1* ch1)
                :Thread(false),
                stoped(true)
            {
                mCameraHal = ch1;
            }

            inline status_t startThread()
            {
                return run ("FocusThread",
                    ANDROID_PRIORITY_URGENT_DISPLAY, 0);
            }

            inline status_t stopThread()
            {
                if (!stoped) {
                    requestExit();
                }
                return NO_ERROR;
            }

        private:
            bool threadLoop()
            {
                stoped = !mCameraHal->startAutoFocus();
                return false;
            }
        };

        inline AutoFocusThread* getAutoFocusThread()
        {
            return mFocusThread.get();
        }

    private:
        sp<AutoFocusThread> mFocusThread;

    private:
        friend class PostJpegUnit;
        class PostJpegUnit : public WorkQueue::WorkUnit {

        private:
            CameraHal1* mhal;

        public:
            PostJpegUnit(CameraHal1* hal):
                WorkQueue::WorkUnit(),mhal(hal)
            {
            }

            bool run() {

                if (mhal != NULL) {
                    mhal->postJpegDataToApp();
                }
                return true;
            }
        };
    private:
        friend class completeTakePictureUnit;
        class completeTakePictureUnit : public WorkQueue::WorkUnit {

        private:
            CameraHal1* mhal;

        public:
            completeTakePictureUnit(CameraHal1* hal):
                WorkQueue::WorkUnit(),mhal(hal)
            {
            }

            bool run() {
                if (mhal != NULL) {
                    mhal->completeTakePicture();
                }
                return true;
            }
        };

    private:
        WorkQueue* mWorkerQueue;

    public:
        static camera_device_ops_t mCamera1Ops;

        /**
           Set the ANativeWindow to which preview frames are sent */
        static int set_preview_window(struct camera_device *,
                                      struct preview_stream_ops *window);

        /**
           Set the notification and data callbacks */
        static void set_callbacks(struct camera_device *,
                                  camera_notify_callback notify_cb,
                                  camera_data_callback data_cb,
                                  camera_data_timestamp_callback data_cb_timestamp,
                                  camera_request_memory get_memory,
                                  void *user);

        /**
         * The following three functions all take a msg_type, which is a bitmask of
         * the messages defined in include/ui/Camera.h
         */

        /**
         * Enable a message, or set of messages.
         */
        static void enable_msg_type(struct camera_device *, int32_t msg_type);

        /**
         * Disable a message, or a set of messages.
         *
         * Once received a call to disableMsgType(CAMERA_MSG_VIDEO_FRAME), camera
         * HAL should not rely on its client to call releaseRecordingFrame() to
         * release video recording frames sent out by the cameral HAL before and
         * after the disableMsgType(CAMERA_MSG_VIDEO_FRAME) call. Camera HAL
         * clients must not modify/access any video recording frame after calling
         * disableMsgType(CAMERA_MSG_VIDEO_FRAME).
         */
        static void disable_msg_type(struct camera_device *, int32_t msg_type);

        /**
         * Query whether a message, or a set of messages, is enabled.  Note that
         * this is operates as an AND, if any of the messages queried are off, this
         * will return false.
         */
        static int msg_type_enabled(struct camera_device *, int32_t msg_type);

        /**
         * Start preview mode.
         */
        static int start_preview(struct camera_device *);

        /**
         * Stop a previously started preview.
         */
        static void stop_preview(struct camera_device *);

        /**
         * Returns true if preview is enabled.
         */
        static int preview_enabled(struct camera_device *);

        /**
         * Request the camera HAL to store meta data or real YUV data in the video
         * buffers sent out via CAMERA_MSG_VIDEO_FRAME for a recording session. If
         * it is not called, the default camera HAL behavior is to store real YUV
         * data in the video buffers.
         *
         * This method should be called before startRecording() in order to be
         * effective.
         *
         * If meta data is stored in the video buffers, it is up to the receiver of
         * the video buffers to interpret the contents and to find the actual frame
         * data with the help of the meta data in the buffer. How this is done is
         * outside of the scope of this method.
         *
         * Some camera HALs may not support storing meta data in the video buffers,
         * but all camera HALs should support storing real YUV data in the video
         * buffers. If the camera HAL does not support storing the meta data in the
         * video buffers when it is requested to do do, INVALID_OPERATION must be
         * returned. It is very useful for the camera HAL to pass meta data rather
         * than the actual frame data directly to the video encoder, since the
         * amount of the uncompressed frame data can be very large if video size is
         * large.
         *
         * @param enable if true to instruct the camera HAL to store
         *        meta data in the video buffers; false to instruct
         *        the camera HAL to store real YUV data in the video
         *        buffers.
         *
         * @return OK on success.
         */
        static int store_meta_data_in_buffers(struct camera_device *, int enable);

        /**
         * Start record mode. When a record image is available, a
         * CAMERA_MSG_VIDEO_FRAME message is sent with the corresponding
         * frame. Every record frame must be released by a camera HAL client via
         * releaseRecordingFrame() before the client calls
         * disableMsgType(CAMERA_MSG_VIDEO_FRAME). After the client calls
         * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
         * responsibility to manage the life-cycle of the video recording frames,
         * and the client must not modify/access any video recording frames.
         */
        static int start_recording(struct camera_device *);

        /**
         * Stop a previously started recording.
         */
        static void stop_recording(struct camera_device *);

        /**
         * Returns true if recording is enabled.
         */
        static int recording_enabled(struct camera_device *);

        /**
         * Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
         *
         * It is camera HAL client's responsibility to release video recording
         * frames sent out by the camera HAL before the camera HAL receives a call
         * to disableMsgType(CAMERA_MSG_VIDEO_FRAME). After it receives the call to
         * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
         * responsibility to manage the life-cycle of the video recording frames.
         */
        static void release_recording_frame(struct camera_device *,
                                            const void *opaque);

        /**
         * Start auto focus, the notification callback routine is called with
         * CAMERA_MSG_FOCUS once when focusing is complete. autoFocus() will be
         * called again if another auto focus is needed.
         */
        static int auto_focus(struct camera_device *);

        /**
         * Cancels auto-focus function. If the auto-focus is still in progress,
         * this function will cancel it. Whether the auto-focus is in progress or
         * not, this function will return the focus position to the default.  If
         * the camera does not support auto-focus, this is a no-op.
         */
        static int cancel_auto_focus(struct camera_device *);

        /**
         * Take a picture.
         */
        static int take_picture(struct camera_device *);

        /**
         * Cancel a picture that was started with takePicture. Calling this method
         * when no picture is being taken is a no-op.
         */
        static int cancel_picture(struct camera_device *);

        /**
         * Set the camera parameters. This returns BAD_VALUE if any parameter is
         * invalid or not supported.
         */
        static int set_parameters(struct camera_device *, const char *parms);

        /** Retrieve the camera parameters.  The buffer returned by the camera HAL
            must be returned back to it with put_parameters, if put_parameters
            is not NULL.
        */
        static char *get_parameters(struct camera_device *);

        /** The camera HAL uses its own memory to pass us the parameters when we
            call get_parameters.  Use this function to return the memory back to
            the camera HAL, if put_parameters is not NULL.  If put_parameters
            is NULL, then you have to use free() to release the memory.
        */
        static void put_parameters(struct camera_device *, char *);

        /**
         * Send command to camera driver.
         */
        static int send_command(struct camera_device *,
                                int32_t cmd, int32_t arg1, int32_t arg2);

        /**
         * Release the hardware resources owned by this object.  Note that this is
         * *not* done in the destructor.
         */
        static void release(struct camera_device *);

        /**
         * Dump state of the camera hardware
         */
        static int dump(struct camera_device *, int fd);

        static int device_close(struct hw_device_t* device);
    };
};
#endif
