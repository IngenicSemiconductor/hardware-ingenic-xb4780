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
#define LOG_TAG  "CameraHal1"
//#define LOG_NDEBUG 0

#define LOST_FRAME_NUM 1

#include "CameraHalSelector.h"
#include "CameraFaceDetect.h"

#ifndef PIXEL_FORMAT_YV16
#define PIXEL_FORMAT_YV16  0x36315659 /* YCrCb 4:2:2 Planar */
#endif

#define WAIT_TIME (1000000000LL * 60)
#define START_CAMERA_COLOR_CONVET_THREAD
//#define CONVERTER_PMON

namespace android{

    CameraHal1::CameraHal1(int id, CameraDeviceCommon* device)
        :mlock("CameraHal1::lock"),
         mcamera_id(id),
         mirror(false),
         mDevice(device),
         ccc(NULL),
         mCameraModuleDev(NULL),
         mModuleOpened(false),
         mnotify_cb(NULL),
         mdata_cb(NULL),
         mdata_cb_timestamp(NULL),
         mget_memory(NULL),
         mcamera_interface(NULL),
         mJzParameters(NULL),
         mMesgEnabled(0),
         mPreviewWindow(NULL),
         mPreviewAfter(0),
         mRawPreviewWidth(0),
         mRawPreviewHeight(0),
         mPreviewFmt(HAL_PIXEL_FORMAT_YV12),
         mPreviewWidth(0),
         mPreviewHeight(0),
         mPreviewFrameSize(0),
         mPreviewHeap(NULL),
         mPreviewIndex(0),
         mPreviewEnabled(false),
         mRecordingFrameSize(0),
         mRecordingHeap(NULL),
         mRecordingindex(0),
         mVideoRecEnabled(false),
         mtotaltime(0),
         mtotalnum(0),
         mTakingPicture(false),
         mPicturewidth(0),
         mPictureheight(0),
         mPreviewWinFmt(HAL_PIXEL_FORMAT_RGB_565),
         mPrebytesPerPixel(2),
         mPreviewWinWidth(0),
         mPreviewWinHeight(0),
         mCurFrameTimestamp(0),
         mCurrentFrame(NULL),
         mFaceCount(0),
         mzoomVal(0),
         mzoomRadio(100),
         isSoftFaceDetectStart(false),
         mipu(NULL),
         ipu_open_status(false),
         init_ipu_first(false),
         x2d_fd(-1),
         mreceived_cmd(false),
         mSensorListener(NULL),
         mWorkerThread(NULL),
         mFocusThread(NULL) {

        if (NULL != mDevice) {
            ccc = new CameraColorConvert();

            mCameraModuleDev = new camera_device_t();
            if (mCameraModuleDev != NULL) {
                mCameraModuleDev->common.tag = HARDWARE_DEVICE_TAG;
                mCameraModuleDev->common.version = HARDWARE_DEVICE_API_VERSION(1,0);
                mCameraModuleDev->common.close = CameraHal1::device_close;
                memset(mCameraModuleDev->common.reserved, 0, 37 - 2);
            }
            mJzParameters = new JZCameraParameters(mDevice, mcamera_id);
            if (mJzParameters == NULL) {
                ALOGE("%s: create parameters object fail",__FUNCTION__);
            }
            mWorkerThread = new WorkThread(this);
            if (getWorkThread() == NULL) {
                ALOGE("%s: could not create work thread", __FUNCTION__);
            }
            mFocusThread = new AutoFocusThread(this);
            if (getAutoFocusThread() == NULL) {
                ALOGE("%s: could not create focus thread", __FUNCTION__);
            }
        }
    }

    CameraHal1::~CameraHal1() {

        if (ccc != NULL) {
            delete ccc;
            ccc = NULL;
        }

        if (NULL != mCameraModuleDev) {
            delete mCameraModuleDev;
            mCameraModuleDev = NULL;
        }

        if (mJzParameters != NULL) {
            delete mJzParameters;
            mJzParameters = NULL;
        }
        mWorkerThread.clear();
        mWorkerThread = NULL;
        mFocusThread.clear();
        mFocusThread = NULL;
        mModuleOpened = false;
        delete CameraFaceDetect::getInstance();
    }

    void CameraHal1::update_device(CameraDeviceCommon* device) {
        mDevice = device;
        mJzParameters->update_device(mDevice);
    }

    int CameraHal1::module_open(const hw_module_t* module, const char* id, hw_device_t** device) {
        status_t ret = NO_MEMORY;

        if ((NULL == mDevice) || (atoi(id) != mcamera_id)) {
            ALOGE("%s: create camera device fail",__FUNCTION__);
            return ret;
        }

        if (NULL != mCameraModuleDev) {
            mCameraModuleDev->common.module = const_cast<hw_module_t*>(module);
            mCameraModuleDev->ops = &(CameraHal1::mCamera1Ops);
            mCameraModuleDev->priv = this;
            *device = &(mCameraModuleDev->common);
            ret = initialize();
            mModuleOpened = true;
        }
        return ret;
    }

    int CameraHal1::get_number_cameras(void) {
        if (NULL != mDevice)
            return mDevice->getCameraNum();
        else
            return 0;
    }

    int CameraHal1::get_cameras_info(int camera_id, struct camera_info* info) {
        status_t ret = BAD_VALUE;

        if (mcamera_id != camera_id) {
            ALOGE("%s: you will get id = %d, but mcamra_id = %d",__FUNCTION__, camera_id, mcamera_id);
            return ret;
        }

        info->device_version = CAMERA_MODULE_API_VERSION_1_0;
        info->static_camera_characteristics = (camera_metadata_t*)0xcafef00d;
        if (NULL != mDevice) {
            ALOGV("%s: will getCameraModuleInfo id = %d, ",__FUNCTION__,camera_id);
            ret = mDevice->getCameraModuleInfo(mcamera_id, info);
            if (ret == NO_ERROR) {
                if (info->facing == CAMERA_FACING_FRONT) {
                    mirror = true;
                } else {
                    mirror = false;
                }
            }
        }
        return ret;
    }

    status_t CameraHal1::initialize() {
        status_t ret = NO_ERROR;
        mDevice->connectDevice(mcamera_id);
        mJzParameters->initDefaultParameters(mirror?CAMERA_FACING_FRONT:CAMERA_FACING_BACK);
        getWorkThread()->startThread(false);
        mWorkerQueue = new WorkQueue(10,false);

        mHal1SignalThread = new Hal1SignalThread(this);
        mHal1SignalThread->Start("SignalThread",PRIORITY_DEFAULT, 0);

        mHal1SignalRecordingVideo = new Hal1SignalRecordingVideo(this);
        mHal1SignalRecordingVideo->Start("RecordingThread",PRIORITY_DEFAULT, 0);
        //register for sensor events
        mSensorListener = new SensorListener();
        if (mSensorListener.get()) {
            if (mSensorListener->initialize() == NO_ERROR) {
                mSensorListener->enableSensor(SensorListener::SENSOR_ORIENTATION);
            } else {
                mSensorListener.clear();
                mSensorListener = NULL;
            }
        }
        return ret;
    }

    status_t CameraHal1::setPreviewWindow(struct preview_stream_ops *window) {
        status_t res = NO_ERROR;

        AutoMutex lock(mlock);
        int preview_fps = mJzParameters->getCameraParameters().getPreviewFrameRate();

        if (window != NULL) {
            res = window->set_usage(window,GRALLOC_USAGE_SW_WRITE_OFTEN);
            if (mPreviewEnabled)
                NegotiatePreviewFormat(window);
            if (res == NO_ERROR) {
                mPreviewAfter = 1000000000LL / preview_fps;
            } else {
                window = NULL;
                res = -res;
                ALOGE("%s: set preview window usage %d -> %s",
                      __FUNCTION__, res, strerror(res));
            }
        }
        mPreviewWindow = window;
        return res;
    }

    void CameraHal1::setCallbacks(camera_notify_callback notify_cb,
                                  camera_data_callback data_cb,
                                  camera_data_timestamp_callback data_cb_timestamp,
                                  camera_request_memory get_memory,
                                  void* user) {
        AutoMutex lock(mlock);
        mnotify_cb = notify_cb;
        mdata_cb = data_cb;
        mdata_cb_timestamp = data_cb_timestamp;
        mget_memory = get_memory;
        mcamera_interface = user;
        return;
    }

    void CameraHal1::enableMsgType(int32_t msg_type) {
        const char* valstr;
        int32_t old = 0;

        valstr = mJzParameters->getCameraParameters().get(CameraParameters::KEY_ZOOM_SUPPORTED);
        if ((NULL != valstr) && (strcmp(valstr,"false") == 0)) {
            AutoMutex lock(mlock);
            msg_type &=~CAMERA_MSG_ZOOM;
        }
        {
            AutoMutex lock(mlock);
            old = mMesgEnabled;
            mMesgEnabled |= msg_type;
        }

        valstr = mJzParameters->getCameraParameters().get(CameraParameters::KEY_RECORDING_HINT);
        if ((msg_type & CAMERA_MSG_VIDEO_FRAME) &&
            (mMesgEnabled ^ old) & CAMERA_MSG_VIDEO_FRAME && mVideoRecEnabled) {
            ALOGV("You must alloc preview buffer for video recording ");
        } else if ((msg_type & CAMERA_MSG_VIDEO_FRAME)
                   && strcmp("true",valstr)) {
            ALOGV("%s: reset preview because format is not yuv420b",__FUNCTION__);
            mJzParameters->getCameraParameters().set(CameraParameters::KEY_RECORDING_HINT,"true");
            //mHal1SignalThread->SetSignal(SIGNAL_RESET_PREVIEW);
        }
    }

    void CameraHal1::resetPreview(void) {

        ALOGV("%s: ",__FUNCTION__);
        stopPreview();
        mJzParameters->setParameters(mJzParameters->getCameraParameters().flatten());
        startPreview();
    }

    void CameraHal1::disableMsgType(int32_t msg_type) {
        int32_t old = 0;

        {
            AutoMutex lock(mlock);
            old = mMesgEnabled;
            mMesgEnabled &= ~msg_type;
        }

        if ((msg_type & CAMERA_MSG_VIDEO_FRAME) &&
            (mMesgEnabled^old) & CAMERA_MSG_VIDEO_FRAME && mVideoRecEnabled) {
            ALOGV("You must alloc preview buffer for video recording.");
        }

        return;
    }

    int CameraHal1::isMsgTypeEnabled(int32_t msg_type) {
        AutoMutex lock(mlock);
        int enable = (mMesgEnabled & msg_type) == msg_type;
        return enable;
    }

    status_t CameraHal1::startPreview() {

        status_t res = NO_ERROR;

        AutoMutex lock(mlock);
        res = mDevice->connectDevice(mcamera_id);
        mDevice->getPreviewSize(&mRawPreviewWidth, &mRawPreviewHeight);
        mJzParameters->resetSizeChanged();

        if (res == NO_ERROR) {
            initVideoHeap(mRawPreviewWidth, mRawPreviewHeight);
            initPreviewHeap();
            res = mDevice->allocateStream(PREVIEW_BUFFER,mget_memory,mRawPreviewWidth, mRawPreviewHeight,
                                          mDevice->getPreviewFormat());
        } else {
            ALOGE("%s: connect device error",__FUNCTION__);
            mPreviewEnabled = false;
        }

        if (res == NO_ERROR) {
            res = mDevice->startDevice();
        } else {
            ALOGE("%s: allocate preview stream error",__FUNCTION__);
            mPreviewEnabled = false;
        }

        if (res == NO_ERROR) {
            NegotiatePreviewFormat(mPreviewWindow);
            res = getWorkThread()->sendMesg(WorkThread::THREAD_READY);
            AutoMutex lock(cmd_lock);
            while (!mreceived_cmd) {
                mreceivedCmdCondition.wait(cmd_lock);
            }
            mreceived_cmd = false;
        } else {
            ALOGE("%s: start device error",__FUNCTION__);
            mDevice->freeStream(PREVIEW_BUFFER);
            mPreviewEnabled = false;
        }

        if (res == NO_ERROR) {
            mPreviewEnabled = true;
            mDevice->sendCommand(FOCUS_INIT);
        } else {
            ALOGE("%s: start preview thread error",__FUNCTION__);
            getWorkThread()->sendMesg(WorkThread::THREAD_IDLE);
            mDevice->stopDevice();
            mDevice->freeStream(PREVIEW_BUFFER);
            mPreviewEnabled = false;
        }

        return mPreviewEnabled ? NO_ERROR : INVALID_OPERATION;
    }

    void CameraHal1::stopPreview() {

        status_t ret = NO_ERROR;
        if (mPreviewEnabled) {
            AutoMutex lock(cmd_lock);
            getWorkThread()->sendMesg(WorkThread::THREAD_IDLE);
            while (!mreceived_cmd) {
                mreceivedCmdCondition.wait(cmd_lock);
            }
            mreceived_cmd = false;
        }
        ret = mDevice->stopDevice();
        ALOGV("%s",__FUNCTION__);
        if (mPreviewHeap) {
            mPreviewFrameSize = 0;
            mPreviewIndex = 0;
            mPreviewWidth = 0;
            mPreviewHeight = 0;
            dmmu_unmap_memory((uint8_t*)mPreviewHeap->data,mPreviewHeap->size);
            mPreviewHeap->release(mPreviewHeap);
            mPreviewHeap = NULL;
        }

        AutoMutex lock(mlock);
        isSoftFaceDetectStart = false;
        if ((ret == NO_ERROR) && mPreviewEnabled) {
            mPreviewEnabled = false;
            mDevice->freeStream(PREVIEW_BUFFER);
        }

        return;
    }

    int CameraHal1::isPreviewEnabled() {

        int enable = 0;
        {
            AutoMutex lock(mlock);
            enable = (mPreviewEnabled != false);
        }

        return enable;
    }

    status_t CameraHal1::storeMetaDataInBuffers(int enable) {
        return enable ? INVALID_OPERATION : NO_ERROR;
    }

    status_t CameraHal1::startRecording() {

        mtotaltime=0;
        mtotalnum=0;
        ALOGV("Enter %s mVideoRecEnable=%s",__FUNCTION__,mVideoRecEnabled?"true":"false");
        if (mVideoRecEnabled == false) {
            AutoMutex lock(mlock_recording);
#ifdef START_CAMERA_COLOR_CONVET_THREAD
        ccc->mCC_SMPThread->startthread();
#endif
            mVideoRecEnabled = true;
            mRecordingindex = 0;
            if (mRecordingHeap == NULL) {
                initVideoHeap(mRawPreviewWidth, mRawPreviewHeight);
            }
        }

        return NO_ERROR;
    }

    void CameraHal1::completeRecordingVideo() {

        if (mRecordingDataQueue.isEmpty()) {
            ALOGE("%s: recording queue is empty",__FUNCTION__);
            return;
        }

        camera_memory_t* recordingData = NULL;
        camera_memory_t* tmpHeap = NULL;
        do {

            if (!mVideoRecEnabled) {
                break;
            }

            {
                AutoMutex lock(recordingDataQueueLock);
                recordingData = mRecordingDataQueue[0];
                mRecordingDataQueue.erase(mRecordingDataQueue.begin());
            }

            if (recordingData != NULL) {
                if ((mRecordingHeap != NULL) && (mRecordingHeap->data != NULL) && ccc) {
                    uint8_t* dest = (uint8_t*)((int)(mRecordingHeap->data)
                                               + mRecordingFrameSize * mRecordingindex);
                    if (mzoomVal != 0) {
                        if (mget_memory != NULL) {
                            tmpHeap = mget_memory(-1, getCurrentFrameSize(), 1, NULL);
                            if (tmpHeap != NULL) {
                                dmmu_map_memory((uint8_t*)recordingData->data,recordingData->size);
                                do_zoom((uint8_t*)tmpHeap->data, (uint8_t*)recordingData->data);
                                dmmu_unmap_memory((uint8_t*)recordingData->data,recordingData->size);
                                recordingData->release(recordingData);
                                recordingData = tmpHeap;
                            }
                        }
                    }

                    ccc->cimvyuy_to_tile420((uint8_t*)recordingData->data,
                                            mCurrentFrame->width,
                                            mCurrentFrame->height,
                                            dest,
                                            0,
                                            mCurrentFrame->height/16);
                    recordingData->release(recordingData);
                    recordingData = NULL;
                    int64_t timestamp = systemTime(SYSTEM_TIME_MONOTONIC);
                    mdata_cb_timestamp(timestamp,CAMERA_MSG_VIDEO_FRAME,
                                       mRecordingHeap, mRecordingindex, mcamera_interface);
                    mRecordingindex = (mRecordingindex+1)%RECORDING_BUFFER_NUM;
                }
            }
        } while (!mRecordingDataQueue.isEmpty());
        return;
    }

    void CameraHal1::stopRecording() {

        ALOGV("Enter %s mVideoRecEnable=%s",
              __FUNCTION__,mVideoRecEnabled?"true":"false");

        if (mVideoRecEnabled) {
            {
                AutoMutex lock(mlock_recording);
                getWorkThread()->threadResume();
                mVideoRecEnabled = false;
#ifdef START_CAMERA_COLOR_CONVET_THREAD
        ccc->mCC_SMPThread->stopthread();
#endif
#ifdef CONVERTER_PMON
        ALOGV("average convert time = %d, num=%d",mtotaltime/mtotalnum,mtotalnum);
#endif
                mRecordingindex = 0;
            }

            if (mRecordingHeap) {
                dmmu_unmap_memory((uint8_t*)mRecordingHeap->data,mRecordingHeap->size);
                mRecordingFrameSize = 0;
                mRecordingHeap->release(mRecordingHeap);
                mRecordingHeap = NULL;
            }

            {
                AutoMutex lock(recordingDataQueueLock);
                for (unsigned i = 0; i < mRecordingDataQueue.size(); i++) {
                    camera_memory_t* recordingData = mRecordingDataQueue[i];
                    recordingData->release(recordingData);
                    recordingData = NULL;
                }
                mRecordingDataQueue.clear();
            }
        }
    }

    int CameraHal1::isRecordingEnabled() {

        ALOGV("Enter %s mVideoRecEnable=%s",
              __FUNCTION__,mVideoRecEnabled?"true":"false");
        int enabled;
        {
            AutoMutex lock(mlock);
            enabled = mVideoRecEnabled; 
        }
        return enabled;
    }

    void CameraHal1::releaseRecordingFrame(const void* opaque) {
        getWorkThread()->threadResume();
        return ;
    }

    status_t CameraHal1::setAutoFocus() {
        status_t ret = NO_ERROR;
        AutoMutex lock(mlock);
        mDevice->sendCommand(PAUSE_FACE_DETECT);
        if (getAutoFocusThread() != NULL) {
            getAutoFocusThread()->startThread();
        }
        return ret;
    }

    status_t CameraHal1::cancelAutoFocus() {
        status_t ret = NO_ERROR;
        if (getAutoFocusThread() != NULL) {
            ret = getAutoFocusThread()->stopThread();
        }
        return ret;
    }

    bool CameraHal1::startAutoFocus() {
        status_t ret = NO_ERROR;
        if (mMesgEnabled & CAMERA_MSG_FOCUS) {
            ret = mDevice->sendCommand(START_FOCUS);
            if (ret == NO_ERROR) {
                if (mMesgEnabled & CAMERA_MSG_FOCUS)
                    mnotify_cb(CAMERA_MSG_FOCUS,0,0,mcamera_interface);
                if (mMesgEnabled & CAMERA_MSG_FOCUS_MOVE) {
                    int focus_state = mDevice->sendCommand(GET_FOCUS_STATUS);
                    if (focus_state == NO_ERROR)
                        mnotify_cb(CAMERA_MSG_FOCUS_MOVE, false, 0, mcamera_interface);
                    else
                        mnotify_cb(CAMERA_MSG_FOCUS_MOVE, false, 0, mcamera_interface);
                }
            } else if (mMesgEnabled & CAMERA_MSG_FOCUS)
                mnotify_cb(CAMERA_MSG_FOCUS,1, 0, mcamera_interface);
        }

        ALOGV("AutoFocus thread exit");
        return false;
    }

    status_t CameraHal1::takePicture() {
        mJzParameters->getCameraParameters().getPictureSize(&mPicturewidth, &mPictureheight);
        if (mDevice->getSupportPreviewDataCapture()) {
            return do_takePictureWithPreview();
        } else {
            return do_takePicture();
        }
    }

    status_t CameraHal1::do_takePictureWithPreview(void) {
        if (mPreviewEnabled) {
            AutoMutex lock(mlock);
            mDevice->sendCommand(INIT_TAKE_PICTURE);
            mTakingPicture = true;
        }
        return mTakingPicture? NO_ERROR : INVALID_OPERATION;
    }

    status_t CameraHal1::do_takePicture(void) {
        const char* str = mJzParameters->getCameraParameters()
            .get(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED);

        if (mVideoRecEnabled && (strncmp(str, "true",4) == 0)) {
            AutoMutex lock(mlock);
            mTakingPicture = true;
            return NO_ERROR;
        }
        mHal1SignalThread->SetSignal(SIGNAL_TAKE_PICTURE);
        return NO_ERROR;
    }

    status_t CameraHal1::completeTakePicture(void) {

        if (mPreviewEnabled) {
            AutoMutex lock(cmd_lock);
            getWorkThread()->sendMesg(WorkThread::THREAD_IDLE);
            while (!mreceived_cmd) {
                mreceivedCmdCondition.wait(cmd_lock);
            }
            mreceived_cmd = false;
            mPreviewEnabled = false;
        }

        camera_memory_t* takingPictureHeap = NULL;
        {
            AutoMutex lock(mlock);
            mDevice->sendCommand(STOP_PICTURE);
            mDevice->stopDevice();
            mDevice->initTakePicture(mPicturewidth,mPictureheight,mget_memory);
            mCurrentFrame =
                (CameraYUVMeta*)(mDevice->sendCommand(TAKE_PICTURE, mPicturewidth, mPictureheight));

            if (mCurrentFrame == NULL) {
                mDevice->deInitTakePicture();
                mHal1SignalThread->SetSignal(SIGNAL_RESET_PREVIEW);
                ALOGE("%s: take picture error",__FUNCTION__);
                return NO_ERROR;
            }

            int size = getCurrentFrameSize();

            if ((mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) && ccc) {
                ccc->cimyuv420b_to_tile420(mCurrentFrame); //1- 4ms
            }

            takingPictureHeap = mget_memory(-1, size,1, NULL);
            memset(takingPictureHeap->data, 0, size);
            dmmu_map_memory((uint8_t*)takingPictureHeap->data,takingPictureHeap->size);
            status_t ret = NO_ERROR;

            if(mDevice->getSupportCaptureIncrease() && mCurrentFrame->width > 1600){
                if(mCurrentFrame->width > 2048){
                    ret = ipu_zoomIn_scale((uint8_t*)takingPictureHeap->data,
                                           mCurrentFrame->width >> 1, mCurrentFrame->height,
                                           (uint8_t*)mCurrentFrame->yAddr, 800, 1200,
                                           mCurrentFrame->format, 2, 800);
                    ret = ipu_zoomIn_scale((uint8_t*)((unsigned int)takingPictureHeap->data + mCurrentFrame->width),
                                           mCurrentFrame->width >> 1, mCurrentFrame->height,
                                           (uint8_t*)mCurrentFrame->yAddr + 1600, 800, 1200,
                                           mCurrentFrame->format, 2, 800);
                }else{
                    ret = ipu_zoomIn_scale((uint8_t*)takingPictureHeap->data,
                                           mCurrentFrame->width, mCurrentFrame->height,
                                           (uint8_t*)mCurrentFrame->yAddr, 1600, 1200,
                                           mCurrentFrame->format, 0, 1600);
                }
                if (ret != NO_ERROR)
                    ALOGE("%s: ipu up scale error",__FUNCTION__);
                if(mzoomVal != 0){
                    memcpy((uint8_t*)mCurrentFrame->yAddr, takingPictureHeap->data, size);
                    do_zoom((uint8_t*)takingPictureHeap->data,(uint8_t*)mCurrentFrame->yAddr);
                }
            }else {
                if(mzoomVal != 0)
                    do_zoom((uint8_t*)takingPictureHeap->data,(uint8_t*)mCurrentFrame->yAddr);
                else
                    memcpy(takingPictureHeap->data,(uint8_t*)mCurrentFrame->yAddr,size);
            }

#if 0
            FILE *file = NULL;
            void * vaddr = takingPictureHeap->data;

#define MY_FILE "/data/jczheng/456.txt"
            ALOGE("yAddr = 0x%p, size = %d, format = 0x%x\n", vaddr, size, mCurrentFrame->format);

            if(file == NULL)
                file = fopen(MY_FILE, "w+");
            if (file == NULL) {
                ALOGE("errno = %d\n", errno);
                ALOGV("open My");
                return false;
            }
            ret = fwrite((void *)vaddr, size, 1, file);
            if(ret < 0)
                ALOGE("<0>""file write failed/n");
            if(file != NULL)
                fclose(file);
#endif
            {
                AutoMutex lock(mcapture_lock);
                mListCaptureHeap.push_back(takingPictureHeap);
            }
        }

        if (mMesgEnabled & CAMERA_MSG_SHUTTER) {
            mnotify_cb(CAMERA_MSG_SHUTTER, 0, 0, mcamera_interface);
        }

        if (mMesgEnabled & CAMERA_MSG_RAW_IMAGE_NOTIFY) {
            mnotify_cb(CAMERA_MSG_RAW_IMAGE_NOTIFY, 0, 0, mcamera_interface);
        }

        if ((takingPictureHeap != NULL) && (mMesgEnabled & CAMERA_MSG_RAW_IMAGE)) {
            mdata_cb(CAMERA_MSG_RAW_IMAGE,takingPictureHeap, 0, NULL, mcamera_interface);
        }

        if ((takingPictureHeap != NULL) && (mMesgEnabled & CAMERA_MSG_POSTVIEW_FRAME)) {
            mdata_cb(CAMERA_MSG_RAW_IMAGE,takingPictureHeap, 0, NULL, mcamera_interface);
        }

        if (mMesgEnabled & CAMERA_MSG_COMPRESSED_IMAGE) {
            ALOGV("%s start compress picture",__FUNCTION__);
            mWorkerQueue->schedule(new PostJpegUnit(this));
        }

        return NO_ERROR;
    }

    status_t CameraHal1::cancelPicture() {

        {
            AutoMutex lock(mlock);
            mDevice->sendCommand(STOP_PICTURE);
            mTakingPicture = false;
        }

        AutoMutex lock(mcapture_lock);
        if (!mListCaptureHeap.empty()) {
            List<camera_memory_t*>::iterator it = mListCaptureHeap.begin();
            for (;it != mListCaptureHeap.end(); ++it) {
                camera_memory_t* captureHeap = *it;
                if (captureHeap != NULL) {
                    dmmu_unmap_memory((uint8_t*)captureHeap->data,captureHeap->size);
                    captureHeap->release(captureHeap);
                    captureHeap = NULL;
                }
            }
            mListCaptureHeap.clear();
        }
        ALOGV("%s: line=%d",__FUNCTION__,__LINE__);
        return NO_ERROR;
    }

    status_t CameraHal1::setParameters(const char* parms) {
        status_t res = NO_ERROR;
        AutoMutex lock(mlock);
        String8 str_param(parms);
        res = mJzParameters->setParameters(str_param);
        return res;
    }

    static char noParams = '\0';
    char* CameraHal1::getParameters() {
        String8 params(mJzParameters->getCameraParameters().flatten());
        char* ret_str = (char*)malloc(sizeof(char) * params.length()+1);
        memset(ret_str, 0, params.length()+1);
        if (ret_str != NULL) {
            memcpy(ret_str, params.string(), params.length()+1);
            return ret_str;
        }
        ALOGE("%s: getParameters error",__FUNCTION__);
        return &noParams;
    }

    void CameraHal1::putParameters(char* params) {
        if (NULL != params && params != &noParams) {
            free(params);
            params = NULL;
        }
    }

    status_t CameraHal1::sendCommand(int32_t cmd, int32_t arg1, int32_t arg2) {
        status_t res = NO_ERROR;
        AutoMutex lock(mlock);
        switch(cmd)
            {
            case CAMERA_CMD_ENABLE_FOCUS_MOVE_MSG:
                bool enable = static_cast<bool>(arg1);
                AutoMutex lock(mlock);
                if (enable) {
                    mMesgEnabled |= CAMERA_MSG_FOCUS_MOVE;
                } else {
                    mMesgEnabled &= ~CAMERA_MSG_FOCUS_MOVE;
                }
                return res;
            }

        if (mPreviewEnabled == false) {
            ALOGE("%s: Preview is not running",__FUNCTION__);
            return BAD_VALUE;
        }
        switch(cmd)
            {
            case CAMERA_CMD_START_SMOOTH_ZOOM:
                res = mDevice->sendCommand(START_ZOOM);
                if (mMesgEnabled & CAMERA_MSG_ZOOM) {
                    mnotify_cb(CAMERA_MSG_ZOOM,mzoomVal,1,mcamera_interface);
                }
                break;
            case CAMERA_CMD_STOP_SMOOTH_ZOOM:
                res = mDevice->sendCommand(STOP_ZOOM);
                if (mMesgEnabled & CAMERA_MSG_ZOOM) {
                    mnotify_cb(CAMERA_MSG_ZOOM,mzoomVal,0,mcamera_interface);
                }
                break;
            case CAMERA_CMD_START_FACE_DETECTION:
                res = softFaceDetectStart(arg1);
                // res = mDevice->sendCommand(START_FACE_DETECT);
                break;
            case CAMERA_CMD_STOP_FACE_DETECTION:
                res = softFaceDetectStop();
                //  res = mDevice->sendCommand(STOP_FACE_DETECT);
                break;
            default:
                break;
            }
        return res;
    }

    void CameraHal1::do_zoom(uint8_t* dest, uint8_t* src) {

        status_t ret = NO_ERROR;
        int bytes_per_pixel = 2;
        unsigned int cut_width = num2even(mCurrentFrame->width * 100 / mzoomRadio);
        unsigned int cut_height = num2even(mCurrentFrame->height * 100 / mzoomRadio);
        int cropLeft = num2even((mCurrentFrame->width - cut_width) / 2);
        cropLeft = (cropLeft < 0) ? 0 : cropLeft;
        int cropTop = num2even((mCurrentFrame->height - cut_height) / 2);
        cropTop = (cropTop < 0) ? 0 : cropTop;
        int offset = (cropTop * mCurrentFrame->width + cropLeft) * bytes_per_pixel;
        offset = (offset < 0) ? 0 : offset;
        int reviseval = 0;

        ALOGV("mzoomVal = %d, cut_width = %d, cut_height = %d, cropLeft =%d, cropTop = %d, offset = %d, mzoomRadio: %d",
              mzoomVal, cut_width, cut_height, cropLeft, cropTop, offset, mzoomRadio);

        if(mCurrentFrame->width > 2048){
            ret = ipu_zoomIn_scale(dest,
                                   mCurrentFrame->width >> 1, mCurrentFrame->height,
                                   (uint8_t*)(src+offset),
                                   cut_width >> 1, cut_height,
                                   mCurrentFrame->format, 2, mCurrentFrame->width >> 1);
            if(cropLeft == 776)
                reviseval = 2;
            else
                reviseval = 0;
            ret = ipu_zoomIn_scale((uint8_t*)((unsigned int)dest + mCurrentFrame->width),
                                   mCurrentFrame->width >> 1, mCurrentFrame->height,
                                   (uint8_t*)(src+offset + cut_width + reviseval),
                                   cut_width >> 1, cut_height,
                                   mCurrentFrame->format, 2, mCurrentFrame->width >> 1);
        }else{
            ret = ipu_zoomIn_scale(dest,
                                   mCurrentFrame->width, mCurrentFrame->height,
                                   (uint8_t*)(src+offset),
                                   cut_width, cut_height,
                                   mCurrentFrame->format, 0, mCurrentFrame->width);
        }
        if (ret != NO_ERROR)
            ALOGE("%s: ipu up scale error",__FUNCTION__);
    }

    void CameraHal1::releaseCamera() {

        if (mWorkerQueue != NULL) {
            delete mWorkerQueue;
            mWorkerQueue = NULL;
        }

        getWorkThread()->stopThread();

        AutoMutex lock(mlock);

        mMesgEnabled = 0;
        mnotify_cb = NULL;
        mdata_cb = NULL;
        mdata_cb_timestamp = NULL;
        mget_memory = NULL;
        mModuleOpened = false;
        ALOGV("%s: line=%d",__FUNCTION__,__LINE__);
        return ;
    }

    status_t CameraHal1::dumpCamera(int fd) {

        char buffer[256];
        int offset = 0;
        String8 msg;

        memset(buffer, 0, 256);
        snprintf(buffer, 256, "status: previewEnable=%s,",mPreviewEnabled?"true":"false");
        msg.append(buffer);
        memset(buffer, 0, 256);
        snprintf(buffer, 256, "mTakingPicture=%s,",mTakingPicture?"true":"false");
        msg.append(buffer);
        memset(buffer, 0, 256);
        snprintf(buffer, 256, "mPreviewAfter=%lld,",mPreviewAfter);
        msg.append(buffer);
        memset(buffer, 0, 256);
        snprintf(buffer,256, "mRawPreviewWidth=%d,mRawPreviewHeight=%d,",
                 mRawPreviewWidth, mRawPreviewHeight);
        msg.append(buffer);
        memset(buffer, 0, 256);
        snprintf(buffer,256, "mPreviewWidth=%d,mPreviewHeight=%d,",
                 mPreviewWidth, mPreviewHeight);
        msg.append(buffer);
        memset(buffer, 0, 256);
        snprintf(buffer,256, "mPreviewWinWidth=%d,mPreviewWinHeight=%d,",
                 mPreviewWinWidth, mPreviewWinHeight);
        msg.append(buffer);
        memset(buffer, 0, 256);
        snprintf(buffer, 256, "mVideoRecordingEnable=%s,",mVideoRecEnabled?"true":"false");
        msg.append(buffer);
        msg.append(mJzParameters->getCameraParameters().flatten());
        write(fd, msg.string(),msg.length());

        ALOGV("%s: line=%d",__FUNCTION__,__LINE__);
        return NO_ERROR;
    }

    int CameraHal1::deviceClose(void) {

        int count = 20;
        if (mHal1SignalThread != NULL) {
            mHal1SignalThread->release();
            while(!mHal1SignalThread->IsTerminated()) {
                usleep(SIG_WAITING_TICK);
                if (--count < 0) {
                    count = 20;
                    break;
                }
            }
            mHal1SignalThread.clear();
            mHal1SignalThread = NULL;
        }

        if (mHal1SignalRecordingVideo != NULL) {
            mHal1SignalRecordingVideo->release();
            while(!mHal1SignalRecordingVideo->IsTerminated()) {
                usleep(SIG_WAITING_TICK);
                if (--count < 0) {
                    count = 20;
                    break;
                }
            }
            mHal1SignalRecordingVideo.clear();
            mHal1SignalRecordingVideo = NULL;
        }

        if (mSensorListener.get()) {
            mSensorListener->disableSensor(SensorListener::SENSOR_ORIENTATION);
            mSensorListener.clear();
            mSensorListener = NULL;
        }
        mDevice->disConnectDevice();

        return NO_ERROR;
    }

    bool CameraHal1::NegotiatePreviewFormat(struct preview_stream_ops* win) {

        if (win == NULL) {
            ALOGE("%s: preview win not set",__FUNCTION__);
            return false;
        }

        if (mRawPreviewWidth < mPreviewWidth || mRawPreviewHeight < mPreviewHeight) {
            mPreviewWinWidth = mRawPreviewWidth;
            mPreviewWinHeight = mRawPreviewHeight;
        } else {
            mPreviewWinWidth = mPreviewWidth;
            mPreviewWinHeight = mPreviewHeight;
        }

        mPreviewWinFmt = HAL_PIXEL_FORMAT_RGB_565;
        if (win->set_buffers_geometry(win,mPreviewWinWidth,mPreviewWinHeight,mPreviewWinFmt) != NO_ERROR) {
            ALOGE("Unable to set buffer geometry");
            mPreviewWinFmt = 0;
            mPreviewWinWidth = 0;
            mPreviewWinHeight = 0;
            return false;
        }
        ALOGV("%s: previewWindow: %dx%d,format:0x%x",
              __FUNCTION__, mPreviewWinWidth,mPreviewWinHeight,mPreviewWinFmt);
        return true;
    }

    int CameraHal1::getCurrentFrameSize(void) {
        int size = 0;

        if ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)
            || (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_SP)) {
            size = (mCurrentFrame->width * mCurrentFrame->height) * 2;
        } else {
            size = (mCurrentFrame->width * mCurrentFrame->height) * 12 / 8;
        }
        return size;
    }

    void CameraHal1::initVideoHeap(int width, int height) {

        int video_width = width, video_height = height;
        int how_recording_big = 0;

        if (mget_memory == NULL) {
            ALOGE("No memory allocator available");
            return;
        }

        how_recording_big = (video_width * video_height) * 12/8;
        if (how_recording_big != mRecordingFrameSize) {
            mRecordingFrameSize = how_recording_big;

            if (mRecordingHeap) {
                dmmu_unmap_memory((uint8_t*)mRecordingHeap->data,mRecordingHeap->size);
                mRecordingHeap->release(mRecordingHeap);
                mRecordingHeap = NULL;
            }

	    mRecordingFrameSize = (mRecordingFrameSize + 255) & ~0xFF; // Encoding buffer must be 256 aligned
            mRecordingHeap = mget_memory(-1, mRecordingFrameSize,RECORDING_BUFFER_NUM, NULL);
            dmmu_map_memory((uint8_t*)mRecordingHeap->data,mRecordingHeap->size);
            ALOGV("%s: line=%d",__FUNCTION__,__LINE__);
        }
    }

    void CameraHal1::initPreviewHeap(void) {

        int how_preview_big = 0;
        const char* format = NULL;

        if (!mget_memory) {
            ALOGE("No memory allocator available");
            return;
        }

        mJzParameters->getCameraParameters().getPreviewSize(&mPreviewWidth, &mPreviewHeight);
        format = mJzParameters->getCameraParameters().getPreviewFormat();

        ALOGV("%s: init preview:%dx%d,format:%s",__FUNCTION__,mPreviewWidth,mPreviewHeight,format);
        if (strcmp(format ,CameraParameters::PIXEL_FORMAT_YUV422I) == 0) {
            mPreviewFmt = HAL_PIXEL_FORMAT_YCbCr_422_I;
            how_preview_big = mPreviewWidth * mPreviewHeight << 1;
        } else if(strcmp(format,CameraParameters::PIXEL_FORMAT_YUV420SP) == 0) {
            mPreviewFmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
            how_preview_big = (mPreviewWidth * mPreviewHeight * 12/8);
        } else if (strcmp(format, CameraParameters::PIXEL_FORMAT_YUV422SP) == 0) {
            mPreviewFmt = HAL_PIXEL_FORMAT_YCbCr_422_SP;
            how_preview_big = (mPreviewWidth * mPreviewHeight * 2);
        } else if (strcmp(format, CameraParameters::PIXEL_FORMAT_YUV420P) == 0) {
            mPreviewFmt = HAL_PIXEL_FORMAT_YV12;
            int stride = (mPreviewWidth + 15) & (-16);
            int y_size = stride * mPreviewHeight;
            int c_stride = ((stride >> 1) + 15) & (-16);
            int c_size = c_stride * mPreviewHeight >> 1;
            int cr_offset = y_size;
            int cb_offset = y_size + c_size;
            int size = y_size + (c_size << 1);

            how_preview_big = size;
        }

        if (how_preview_big != mPreviewFrameSize) {
            mPreviewFrameSize = how_preview_big;

            if (mPreviewHeap) {
                mPreviewIndex = 0;
                dmmu_unmap_memory((uint8_t*)mPreviewHeap->data, mPreviewHeap->size);
                mPreviewHeap->release(mPreviewHeap);
                mPreviewHeap = NULL;
            }

            mPreviewHeap = mget_memory(-1, mPreviewFrameSize,PREVIEW_BUFFER_CONUT,NULL);
            dmmu_map_memory((uint8_t*)mPreviewHeap->data, mPreviewHeap->size);
        }
    }

    bool CameraHal1::thread_body(void) {

        static int j = 0;
        static int dropframe = 0;
        static int64_t timeout = WAIT_TIME;
        static int64_t startTime = 0;
        static int64_t workTime = 0;
        static int thread_state = WorkThread::THREAD_IDLE;

        WorkThread::ControlCmd res = getWorkThread()->receiveCmd(-1,timeout);

        switch (res) {

        case WorkThread::THREAD_ERROR:
            {
                if (j++ > 10) {
                    ALOGE("%s: received cmd error, %s",__FUNCTION__, strerror(errno));
                    goto exit_thread;
                }
                return true;
            }

        case WorkThread::THREAD_STOP:
            {
                ALOGV("%s: thread stop",__FUNCTION__);
                return false;
            }

        case WorkThread::THREAD_EXIT:
            {
            exit_thread:
                j = 0;
                close_x2d_dev();
                close_ipu_dev();
                thread_state = WorkThread::THREAD_EXIT;
                timeout = WAIT_TIME;
                startTime = 0;
                dropframe = 0;
                workTime = 0;
                ALOGV("%s: Worker thread has been exit.",__FUNCTION__);
                return false;
            }

        case WorkThread::THREAD_TIMEOUT:
            {
                mreceived_cmd = false;
                break;
            }

        case WorkThread::THREAD_IDLE:
            {
                thread_state = WorkThread::THREAD_IDLE;
                dropframe = 0;
                {
                    AutoMutex lock(cmd_lock);
                    mreceived_cmd = true;
                    mCurrentFrame = NULL;
                    mreceivedCmdCondition.signal();
                }
                ALOGV("%s: thread idle",__FUNCTION__);
                break;
            }

        case WorkThread::THREAD_READY:
            {
                thread_state = WorkThread::THREAD_READY;
                {
                    AutoMutex lock(cmd_lock);
                    mreceived_cmd = true;
                    mreceivedCmdCondition.signal();
                }
                ALOGV("%s: thread ready",__FUNCTION__);
                break;
            }
        }

        if (thread_state == WorkThread::THREAD_READY) {

            startTime = systemTime(SYSTEM_TIME_MONOTONIC);

            mDevice->flushCache(NULL,0);

            if (mJzParameters->is_preview_size_change() ||
                mJzParameters->is_picture_size_change() ||
                mJzParameters->is_video_size_change()) {
                ALOGD("%s:SetSignal reset preview",__FUNCTION__);
                thread_state = WorkThread::THREAD_IDLE;
                mDevice->sendCommand(STOP_PICTURE);
                mHal1SignalThread->SetSignal(SIGNAL_RESET_PREVIEW);
                return true;
            }

            mCurrentFrame = (CameraYUVMeta*)mDevice->getCurrentFrame(); //40ms

            if (mCurrentFrame == NULL) {
                timeout = WAIT_TIME;
                thread_state = WorkThread::THREAD_IDLE;
                //CHECK(!"current frame is null");
                return true;
            }

            if ((mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) && ccc) {
                ccc->cimyuv420b_to_tile420(mCurrentFrame); //1- 4ms
            }

            dump_data(false);

            mCurFrameTimestamp = systemTime(SYSTEM_TIME_MONOTONIC);

            postFrameForNotify(); // 5ms
            if (dropframe == LOST_FRAME_NUM) {
                postFrameForPreview(); // 5ms
            } else {
                dropframe++;
            }

            workTime = systemTime(SYSTEM_TIME_MONOTONIC) - mCurFrameTimestamp;

            timeout = mPreviewAfter - workTime - (mCurFrameTimestamp - startTime); // 50ms - 12ms - (45 + 2) = -9ms
        } else {
            timeout = WAIT_TIME;
        }

        return true;
    }

    void CameraHal1::postFrameForPreview() {

        int res = NO_ERROR;
        if ((mPreviewEnabled == false) || mPreviewWindow == NULL)
            return ;

        buffer_handle_t* buffer = NULL;
        int stride = 0;
        res = mPreviewWindow->dequeue_buffer(mPreviewWindow, &buffer, &stride);
        if ((res != NO_ERROR) || (buffer == NULL)) {
            //CHECK(!"dequeue buffer error");
            return ;
        }

        res = mPreviewWindow->lock_buffer(mPreviewWindow,buffer);
        if (res != NO_ERROR) {
            mPreviewWindow->cancel_buffer(mPreviewWindow, buffer);
            return ;
        }

        void *img = NULL;
        const Rect rect(mPreviewWinWidth,mPreviewWinHeight);
        GraphicBufferMapper& mapper(GraphicBufferMapper::get());
        res = mapper.lock(*buffer, GRALLOC_USAGE_SW_WRITE_OFTEN, rect, &img);
        if (res != NO_ERROR) {
            mPreviewWindow->cancel_buffer(mPreviewWindow, buffer);
            return ;
        }

        res = fillCurrentFrame((uint8_t*)img,buffer);
        if (res == NO_ERROR) {
            mPreviewWindow->set_timestamp(mPreviewWindow, mCurFrameTimestamp);
            mPreviewWindow->enqueue_buffer(mPreviewWindow, buffer);
        } else {
            mPreviewWindow->cancel_buffer(mPreviewWindow, buffer);
        }
        mapper.unlock(*buffer);
    }

    status_t CameraHal1::fillCurrentFrame(uint8_t* img,buffer_handle_t* buffer) {

        int srcStride = mCurrentFrame->yStride;
        uint8_t*src = (uint8_t*)(mCurrentFrame->yAddr);
        int srcWidth = mCurrentFrame->width;
        int srcHeight = mCurrentFrame->height;
        camera_memory_t* tmp_mem = NULL;
        int ret = NO_ERROR;

        int xStart = 0;
        int yStart = 0;

        IMG_native_handle_t* dst_handle = NULL;
        dst_handle = (IMG_native_handle_t*)(*buffer);

        ALOGV("src:%dx%d, preview: %dx%d, mPreWin:%dx%d",mCurrentFrame->width,
               mCurrentFrame->height, mPreviewWidth, mPreviewHeight, mPreviewWinWidth, mPreviewWinHeight);
        if (mPreviewWinFmt == HAL_PIXEL_FORMAT_RGB_565) {
            mPrebytesPerPixel = 2;
        } else if (mPreviewWinFmt == HAL_PIXEL_FORMAT_JZ_YUV_420_P ||
                   mPreviewWinFmt == HAL_PIXEL_FORMAT_YCbCr_422_SP ||
                   mPreviewWinFmt == HAL_PIXEL_FORMAT_YCrCb_420_SP ||
                   mPreviewWinFmt == HAL_PIXEL_FORMAT_YV12 ) {
            mPrebytesPerPixel = 1;
        } else if (mPreviewWinFmt == HAL_PIXEL_FORMAT_RGB_888) {
            mPrebytesPerPixel = 3;
        } else if (mPreviewWinFmt == HAL_PIXEL_FORMAT_RGBA_8888 ||
                   mPreviewWinFmt == HAL_PIXEL_FORMAT_RGBX_8888 ||
                   mPreviewWinFmt == HAL_PIXEL_FORMAT_BGRA_8888) {
            mPrebytesPerPixel = 4;
        } else if (mPreviewWinFmt == HAL_PIXEL_FORMAT_YCbCr_422_I) {
            mPrebytesPerPixel = 2;
        }
        int dest_size = mPreviewWinWidth * mPreviewWinHeight * mPrebytesPerPixel;
        int dstStride = dst_handle->iStride * (dst_handle->uiBpp >> 3);
        uint8_t* dst = ((uint8_t*)img) + (xStart*mPrebytesPerPixel) + (dstStride*yStart);

        switch (mPreviewWinFmt) {
        case HAL_PIXEL_FORMAT_YCbCr_422_SP:
            if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I))
                ccc->yuyv_to_yvu420sp(dst,mPreviewWinWidth,
                                      mPreviewWinHeight, src, srcStride, srcWidth, srcHeight);
            break;

        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I))
                ccc->yuyv_to_yvu420sp(dst,mPreviewWinWidth,
                                      mPreviewWinHeight,src, srcStride, srcWidth, srcHeight);
            break;

        case HAL_PIXEL_FORMAT_YV12:
            if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I))
                ccc->yuyv_to_yvu420p( dst, dstStride, mPreviewWinHeight, 
                                      src, srcStride, srcWidth, srcHeight);
            break;

        case PIXEL_FORMAT_YV16:
            if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I))
                ccc->yuyv_to_yvu422p(dst, dstStride, mPreviewWinHeight, 
                                     src, srcStride, srcWidth, srcHeight);
            break;

        case HAL_PIXEL_FORMAT_YCbCr_422_I:
            {
                uint8_t* pdst = dst;
                uint8_t* psrc = src;
                int h;
                for (h=0; h<srcHeight; h++) {
                    memcpy(pdst, psrc, srcWidth<<1);
                    pdst += dstStride;
                    psrc += srcStride;
                }
            }
            break;
        case HAL_PIXEL_FORMAT_RGB_888:
            if (ipu_open_status) {
                ipu_convert_dataformat(mCurrentFrame,dst,buffer);
            } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                ccc->yuyv_to_rgb24(src, srcStride, dst, dstStride, srcWidth, srcHeight);
            }
            break;

        case HAL_PIXEL_FORMAT_RGBA_8888:
            if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I))
                ccc->yuyv_to_rgb32(src, srcStride, dst, dstStride, srcWidth, srcHeight);
            break;

        case HAL_PIXEL_FORMAT_RGBX_8888:
            if (ipu_open_status) {
                ipu_convert_dataformat(mCurrentFrame,dst,buffer);
            } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                ccc->yuyv_to_rgb32(src, srcStride, dst, dstStride, srcWidth, srcHeight);
            }

            break;

        case HAL_PIXEL_FORMAT_BGRA_8888:
            if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                ccc->yuyv_to_bgr32(src, srcStride, dst, dstStride, srcWidth, srcHeight);
            }
            break;

        case HAL_PIXEL_FORMAT_RGB_565:
#ifdef SOFT_CONVERT
            if (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
                ccc->tile420_to_rgb565(mCurrentFrame, dst);
            } else if (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I){
                ipu_convert_dataformat(mCurrentFrame,dst,buffer);
                //ccc->yuyv_to_rgb565(src, srcStride, dst, dstStride, srcWidth, srcHeight);
            }
#else
            ipu_convert_dataformat(mCurrentFrame,dst,buffer);
#endif
            break;

        default:
            ALOGE("Unhandled pixel format");
            goto preview_win_format_error;
        }

        if (isSoftFaceDetectStart == true && ccc) {
            if (mPreviewWinFmt == HAL_PIXEL_FORMAT_RGB_565) {
                mFaceCount = CameraFaceDetect::getInstance()->detect((uint16_t*)dst);
            } else {
                camera_memory_t* rgb565 = mget_memory(-1, (srcWidth*srcHeight*2), 1, NULL);
                if ((rgb565 != NULL) && (rgb565->data != NULL)) {
                    if (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
                        ccc->yuyv_to_rgb565(src, srcStride, (uint8_t*)(rgb565->data),
                                            mPreviewWinWidth*2, srcWidth, srcHeight);
                        mFaceCount = CameraFaceDetect::getInstance()->detect((uint16_t*)rgb565->data);
                    } else if (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
#ifdef SOFT_CONVERT
                        ccc->tile420_to_rgb565(mCurrentFrame, (uint8_t*)(rgb565->data));
#else
#ifdef USE_X2D
                        x2d_convert_dataformat(mCurrentFrame, 
                                               (uint8_t*)(rgb565->data), buffer);
#else
                        if (ipu_open_status)
                            ipu_convert_dataformat(mCurrentFrame,(uint8_t*)(rgb565->data), buffer);
#endif
#endif
                        mFaceCount = CameraFaceDetect::getInstance()->detect((uint16_t*)rgb565->data);
                    } else if (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P) {
                        ccc->yuv420p_to_rgb565(src, (uint8_t*)(rgb565->data),srcWidth, srcHeight);
                        mFaceCount = CameraFaceDetect::getInstance()->detect((uint16_t*)rgb565->data);
                    }
                    rgb565->release(rgb565);
                    rgb565 = NULL;
                }
            }
        }
    preview_win_format_error:
        if (tmp_mem != NULL) {
            tmp_mem->release(tmp_mem);
            tmp_mem = NULL;
        }
        return NO_ERROR;
    }

    void CameraHal1::postFrameForNotify() {

        {
            AutoMutex lock(mlock_recording);
            if ((mMesgEnabled & CAMERA_MSG_VIDEO_FRAME) && mVideoRecEnabled) {
#ifdef START_CAMERA_COLOR_CONVET_THREAD
                if ((NULL != mRecordingHeap)
                    && (mRecordingHeap->data != NULL)
                    && ccc) {
                    uint8_t* dest = (uint8_t*)((int)(mRecordingHeap->data)
                                               + mRecordingFrameSize * mRecordingindex);
#ifdef CONVERTER_PMON
                    int time0 = GetTimer();
#endif
                    ccc->mCC_SMPThread->SetConverterParameters(mCurrentFrame,dest,0,(mCurrentFrame->height/16)/2);
                    ccc->mCC_SMPThread->start_guest();
                    ccc->cimvyuy_to_tile420((uint8_t*)mCurrentFrame->yAddr,
                                            mCurrentFrame->width,
                                            mCurrentFrame->height,
                                            dest,
                                            (mCurrentFrame->height/16)/2,
                                            (mCurrentFrame->height/16) - (mCurrentFrame->height/16)/2);//half of the frame
                    ccc->mCC_SMPThread->wait_guest();
#ifdef CONVERTER_PMON
                    int time_use = GetTimer()-time0;
                    mtotaltime += time_use;
                    mtotalnum ++;
                    ALOGV("Dualll CONVERT TIME=%d,gettid=%d",time_use,gettid());
#endif
                    mdata_cb_timestamp(mCurFrameTimestamp,CAMERA_MSG_VIDEO_FRAME,
                                       mRecordingHeap, mRecordingindex, mcamera_interface);
                    mRecordingindex = (mRecordingindex+1)%RECORDING_BUFFER_NUM;
                    getWorkThread()->threadPause();
                }
#else
                if (mget_memory) {
                    int size = getCurrentFrameSize();
                    camera_memory_t* recordingData = mget_memory(-1,size,1,NULL);
                    if ((recordingData != NULL) && (recordingData->data != NULL)) {
                        memcpy(recordingData->data, (uint8_t*)mCurrentFrame->yAddr, size);
                        AutoMutex lock(recordingDataQueueLock);
                        mRecordingDataQueue.push_back(recordingData);
                        mHal1SignalRecordingVideo.get()->SetSignal(SIGNAL_RECORDING_START);
                    }
                }
#endif
            }
        }

        if (mMesgEnabled & CAMERA_MSG_PREVIEW_FRAME) {

            int srcWidth = mCurrentFrame->width;
            int srcHeight = mCurrentFrame->height;
            uint8_t* src = (uint8_t*)mCurrentFrame->yAddr;
            camera_memory_t* tmp_mem = NULL;
            bool convert_result = true;
            int ret = NO_ERROR;

            if (mJzParameters->is_preview_size_change()) {
                ALOGV("%s:reset preview heap android format",__FUNCTION__);
                initPreviewHeap();
                NegotiatePreviewFormat(mPreviewWindow);
            }

            int cwidth = mPreviewWidth;
            int cheight = mPreviewHeight;
            int cFrameSize = mPreviewFrameSize;
            int cFormat = mPreviewFmt;

            if (cwidth < mCurrentFrame->width ||
                cheight < mCurrentFrame->height) {
                tmp_mem = mget_memory(-1, cwidth*cheight*2, 1, NULL);
                ret = ipu_zoomIn_scale((uint8_t*)tmp_mem->data, cwidth, cheight, (uint8_t*)mCurrentFrame->yAddr,
                                       mCurrentFrame->width, mCurrentFrame->height, mCurrentFrame->format, 0, mCurrentFrame->width);
                if (ret != NO_ERROR) {
                    ALOGE("%s: ipu up scale error",__FUNCTION__);
                    cFormat = 0;
                } else {
                    src = (uint8_t*)tmp_mem->data;
                    srcWidth = cwidth;
                    srcHeight = cheight;
                    ALOGV("%s: srcSize: %dx%d",__FUNCTION__, srcWidth, srcHeight);
                }
            }

            //modify for weixin video chat mirror, rotation src data
            int rot = 0;
            if (mirror && ccc ) {
                rot = mSensorListener->getOrientationCompensation();
                // if (rot == 90 || rot == 270) {
                //     ccc->yuyv_upturn(src,srcWidth, srcHeight);
                // } else if (rot == 0 || rot == 180) {
                //     ccc->yuyv_mirror(src, srcWidth, srcHeight);
                // }
            }

            ALOGV("preview size:%dx%d, raw size:%dx%d, dest format:0x%x, src fromat:0x%x, rot: %d",
                  cwidth, cheight, mCurrentFrame->width, mCurrentFrame->height,cFormat, mCurrentFrame->format, rot);
            if ((mPreviewHeap != NULL) && (mPreviewHeap->data != NULL)) {
                void* dest = (void*)((int)(mPreviewHeap->data) + mPreviewFrameSize*mPreviewIndex);
                switch(cFormat) {

                case HAL_PIXEL_FORMAT_YCbCr_422_SP:
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                        ccc->yuyv_to_yvu420sp((uint8_t*)(dest),
                                              cwidth, cheight, src, 
                                              srcWidth<<1, srcWidth, srcHeight);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                        ccc->yuv420p_to_yuv420sp(src,(uint8_t*)(dest),srcWidth, srcHeight);
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YCrCb_420_SP) ||
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_SP))) {
                        if (cwidth*cheight > mCurrentFrame->width * mCurrentFrame->height) {
                            memcpy((uint8_t*)(dest), src, mCurrentFrame->width * mCurrentFrame->height*12/8);
                        } else {
                            memcpy((uint8_t*)(dest), src, mPreviewFrameSize);
                        }
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12) || 
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P))) {
                        ccc->yuv420p_to_yuv420sp(src, (uint8_t*)(dest),
                                                 srcWidth, srcHeight);
                    } else {
                        convert_result = false;
                    }
                    break;
                case HAL_PIXEL_FORMAT_YCrCb_420_SP:
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                        ccc->yuyv_to_yvu420sp((uint8_t*)(dest), cwidth, cheight, src, srcWidth<<1 ,srcWidth,srcHeight);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                        ccc->yuv420p_to_yuv420sp(src,(uint8_t*)(dest), srcWidth, srcHeight);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCrCb_420_SP)) {
                        if (cwidth*cheight > mCurrentFrame->width * mCurrentFrame->height) {
                            memcpy((uint8_t*)(dest), src, mCurrentFrame->width * mCurrentFrame->height*12/8);
                        } else {
                            memcpy((uint8_t*)(dest), src, mPreviewFrameSize);
                        }
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_SP)) {
                        ccc->yuv422sp_to_yuv420sp((uint8_t*)(dest),src,srcWidth, srcHeight);
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12) ||
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P))) {
                        ccc->yuv420p_to_yuv420sp(src, (uint8_t*)(dest), srcWidth, srcHeight);
                    } else {
                        convert_result = false;
                    }
                    break;
                case HAL_PIXEL_FORMAT_YV12:
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                        ccc->yuyv_to_yvu420p((uint8_t*)(dest),cwidth, cheight, src,srcWidth<<1, srcWidth, srcHeight);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12) ||
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P))) {
                        if (cwidth*cheight > mCurrentFrame->width * mCurrentFrame->height) {
                            memcpy((uint8_t*)(dest), src, mCurrentFrame->width * mCurrentFrame->height*12/8);
                        } else {
                            memcpy((uint8_t*)(dest), src, mPreviewFrameSize);
                        }
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCrCb_420_SP)) {
                        ccc->yuv420sp_to_yuv420p(src, (uint8_t*)(dest),srcWidth, srcHeight);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_SP)) {
                        ccc->yuv422sp_to_yuv420p((uint8_t*)dest, src,srcWidth, srcHeight);
                    } else {
                        convert_result = false;
                    }
                    break;
                case HAL_PIXEL_FORMAT_YCbCr_422_I:
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                        //ccc->yuv420p_to_yuyv(mCurrentFrame, (uint8_t*)(dest));
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                        if (cwidth*cheight > mCurrentFrame->width * mCurrentFrame->height) {
                            memcpy((uint8_t*)(dest), src, mCurrentFrame->width * mCurrentFrame->height*2);
                        } else {
                            memcpy((uint8_t*)(dest), src, mPreviewFrameSize);
                        }
                    } else {
                        convert_result = false;
                    }
                    break;
                case HAL_PIXEL_FORMAT_JZ_YUV_420_B:
                    if (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
                        if (cwidth*cheight > mCurrentFrame->width * mCurrentFrame->height) {
                            memcpy((uint8_t*)(dest), src, mCurrentFrame->width * mCurrentFrame->height*12/8);
                        } else {
                            memcpy((uint8_t*)(dest), src, mPreviewFrameSize);
                        }
                    } else {
                        memset((uint8_t*)(dest), 0xff, mPreviewFrameSize);
                    }
                    break;
                case HAL_PIXEL_FORMAT_JZ_YUV_420_P:
                    if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I)) {
                        ccc->yuyv_to_yvu420p((uint8_t*)(dest),cwidth, cheight, src,srcWidth<<1,srcWidth, srcHeight);
                    } else if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B)) {
                        ccc->tile420_to_yuv420p(mCurrentFrame, (uint8_t*)(dest));
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P) ||
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12))) {
                        if (cwidth*cheight > mCurrentFrame->width * mCurrentFrame->height) {
                            memcpy((uint8_t*)(dest), src, mCurrentFrame->width * mCurrentFrame->height*12/8);
                        } else {
                            memcpy((uint8_t*)(dest), src, mPreviewFrameSize);
                        }
                    } else if (ccc && ((mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_SP) ||
                                       (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCrCb_420_SP))) {
                        ccc->yuv420sp_to_yuv420p(src, (uint8_t*)(dest),srcWidth, srcHeight);
                    } else {
                        convert_result = false;
                    }
                    break;
                default:
                    convert_result = false;
                    ALOGE("Unhandled pixel format");
                }
                if (convert_result) {
                    mdata_cb(CAMERA_MSG_PREVIEW_FRAME, mPreviewHeap, mPreviewIndex, 
                             NULL,mcamera_interface);
                    mPreviewIndex = (mPreviewIndex+1)%PREVIEW_BUFFER_CONUT;
                } else {
                    ALOGE("%s: format 0x%x is not support",__FUNCTION__,cFormat);
                }
                if (tmp_mem != NULL) {
                    tmp_mem->release(tmp_mem);
                    tmp_mem = NULL;
                }
            }
        }

        if ((mMesgEnabled & CAMERA_MSG_PREVIEW_METADATA) && (isSoftFaceDetectStart == true)) {
            Rect **faceRect = NULL;
            camera_frame_metadata_t frame_metadata;
            int maxFaces = mJzParameters->getCameraParameters()
                .getInt(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW);
            status_t ret = NO_ERROR;
            float lx = 0, ly = 0, rx = 0, ry = 0;
            float fl = 0, fr = 0, ft = 0, fb = 0;

            if (mFaceCount > 0) {
                if (mFaceCount > maxFaces)
                    mFaceCount = maxFaces;
                faceRect = new Rect*[mFaceCount];
                frame_metadata.faces = (camera_face_t*)calloc(mFaceCount, sizeof(camera_face_t));
                frame_metadata.number_of_faces = mFaceCount;
                for (int i = 0; i < mFaceCount; ++i) {
                    faceRect[i] = new Rect();
                    CameraFaceDetect::getInstance()->get_face(faceRect[i],i);
                    fl = faceRect[i]->left;
                    fr = faceRect[i]->right;
                    ft = faceRect[i]->top;
                    fb = faceRect[i]->bottom;

                    if (fl >= -1000 && fl <= 1000) {
                        ;
                    } else {
                        fl = fl - 1000;
                        fr = fr - 1000;
                        ft = ft - 1000;
                        fb = fb - 1000;
                    }

                    frame_metadata.faces[i].rect[0] = (int32_t)fl;
                    frame_metadata.faces[i].rect[1] = (int32_t)fr;
                    frame_metadata.faces[i].rect[2] = (int32_t)ft;
                    frame_metadata.faces[i].rect[3] = (int32_t)fb;

                    frame_metadata.faces[i].id = i;
                    frame_metadata.faces[i].score = CameraFaceDetect::getInstance()->get_confidence();
                    frame_metadata.faces[i].mouth[0] = -2000; frame_metadata.faces[i].mouth[1] = -2000;
                    lx = CameraFaceDetect::getInstance()->getLeftEyeX();
                    ly = CameraFaceDetect::getInstance()->getLeftEyeY();
                    rx = CameraFaceDetect::getInstance()->getRightEyeX();
                    ry = CameraFaceDetect::getInstance()->getRightEyeY();
                    if ((lx >= -1000 && lx <= 1000)) {
                        ;
                    } else {
                        lx = lx - 1000;
                        ly = ly - 1000;
                        rx = rx - 1000;
                        ry = ry - 1000;
                    }
                    frame_metadata.faces[i].left_eye[0] = (int32_t)lx;
                    frame_metadata.faces[i].left_eye[1] = (int32_t)ly;
                    frame_metadata.faces[i].right_eye[0] = (int32_t)rx;
                    frame_metadata.faces[i].right_eye[1] = (int32_t)ry;
                }

                camera_memory_t *tmpBuffer = mget_memory(-1, 1, 1, NULL);
                mdata_cb(CAMERA_MSG_PREVIEW_METADATA, tmpBuffer, 0, &frame_metadata,mcamera_interface);

                if ( NULL != tmpBuffer ) {
                    tmpBuffer->release(tmpBuffer);
                    tmpBuffer = NULL;
                }

                for (int i = 0; i < mFaceCount; ++i) {
                    delete faceRect[i];
                    faceRect[i] = NULL;
                }
                delete [] faceRect;
                faceRect = NULL;
                   
                if (frame_metadata.faces != NULL) {
                    free(frame_metadata.faces);
                    frame_metadata.faces = NULL;
                }
            }
        }

        if (mTakingPicture) {

            if (mJzParameters->is_picture_size_change()
                || mJzParameters->is_video_size_change()) {
                mDevice->sendCommand(STOP_PICTURE);
                return;
            }

            mTakingPicture = false;
            int size = getCurrentFrameSize();

            camera_memory_t* takingPictureHeap = mget_memory(-1, size,1, NULL);
            memset(takingPictureHeap->data, 0, size);
            dmmu_map_memory((uint8_t*)takingPictureHeap->data,takingPictureHeap->size);

            if(mzoomVal != 0){
                do_zoom((uint8_t*)takingPictureHeap->data,(uint8_t*)mCurrentFrame->yAddr);
            } else {
                memcpy(takingPictureHeap->data, (uint8_t*)mCurrentFrame->yAddr,size);
            }

            {
                AutoMutex lock(mcapture_lock);
                mListCaptureHeap.push_back(takingPictureHeap);
            }

            if (mMesgEnabled & CAMERA_MSG_SHUTTER)
                mnotify_cb(CAMERA_MSG_SHUTTER, 0, 0, mcamera_interface);

            if (mMesgEnabled & CAMERA_MSG_RAW_IMAGE_NOTIFY)
                mnotify_cb(CAMERA_MSG_RAW_IMAGE_NOTIFY, 0, 0, mcamera_interface);

            if (mMesgEnabled & CAMERA_MSG_RAW_IMAGE) {
                mdata_cb(CAMERA_MSG_RAW_IMAGE,takingPictureHeap, 0, NULL, mcamera_interface);
            }

            if (mMesgEnabled & CAMERA_MSG_POSTVIEW_FRAME) {
                mdata_cb(CAMERA_MSG_RAW_IMAGE,takingPictureHeap, 0, NULL, mcamera_interface);
            }

            if (mMesgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)
                mWorkerQueue->schedule(new PostJpegUnit(this));
        }

        return;
    }

    void CameraHal1::postJpegDataToApp(void) {

        if ((mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) && ccc) {
            hardCompressJpeg();
        } else if (mCurrentFrame->format == HAL_PIXEL_FORMAT_YCbCr_422_I
                   || mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12
                   || mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P) {
            softCompressJpeg();
        } else {
            ALOGE("%s: don't support other format: 0x%x for compress jpeg",
                  __FUNCTION__,mCurrentFrame->format);
        }
        mDevice->deInitTakePicture();
        if (mVideoRecEnabled) {
            mDevice->sendCommand(STOP_PICTURE);
        }
        return;
    }

    void CameraHal1::softCompressJpeg(void) {

        ALOGV("%s: Enter", __FUNCTION__);

        camera_memory_t* jpeg_buff = NULL;
        int ret = convertCurrentFrameToJpeg(&jpeg_buff);

        if (ret == NO_ERROR && jpeg_buff != NULL && jpeg_buff->data != NULL) {
            mdata_cb(CAMERA_MSG_COMPRESSED_IMAGE, jpeg_buff, 0, NULL, mcamera_interface);
            jpeg_buff->release(jpeg_buff);
        } else if (jpeg_buff != NULL && jpeg_buff->data != NULL) {
            jpeg_buff->release(jpeg_buff);
        }
    }

    void CameraHal1::hardCompressJpeg(void) {

        ALOGV("%s: Enter",__FUNCTION__);

        status_t ret = UNKNOWN_ERROR;

        if (mListCaptureHeap.empty()) {
            ALOGE("%s: don't have capture heap",__FUNCTION__);
            return;
        }

        int picQuality = mJzParameters->getCameraParameters().getInt(CameraParameters::KEY_JPEG_QUALITY);
        int thumQuality = mJzParameters->getCameraParameters()
            .getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
        if (picQuality <= 0 || picQuality == 100) picQuality = 90;
        if (thumQuality <= 0 || thumQuality == 100) thumQuality = 90;

        int csize = getCurrentFrameSize();
        int th_width = mJzParameters->getCameraParameters().
            getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
        int th_height =  mJzParameters->getCameraParameters().
            getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
        int ctnsize = th_width * th_height;

        int jpeg_size = csize;
        int thumb_size = ctnsize;
        camera_memory_t* jpeg_buff = mget_memory(-1,csize+1000 ,1,NULL);
        camera_memory_t* jpeg_tn_buff = (ctnsize == 0) ? NULL : (mget_memory(-1, ctnsize, 1, NULL));
        camera_memory_t* jpegMem = NULL;
        camera_memory_t* captureHeap = NULL;
        {
            AutoMutex lock(mcapture_lock);
            captureHeap = *(mListCaptureHeap.begin());
            mListCaptureHeap.erase(mListCaptureHeap.begin());
        }

#if CAMERA_SUPPORT_VIDEOSNAPSHORT
        CameraCompressorHW ccHW;
        compress_params_hw_t hw_cinfo;
        memset(&hw_cinfo, 0, sizeof(compress_params_hw_t));
        hw_cinfo.pictureYUV420_y = (uint8_t*)(captureHeap->data);
        hw_cinfo.pictureYUV420_c = (uint8_t*)((uint8_t*)captureHeap->data
                                              + (mCurrentFrame->width*mCurrentFrame->height));
        hw_cinfo.pictureWidth = mCurrentFrame->width;
        hw_cinfo.pictureHeight = mCurrentFrame->height;
        hw_cinfo.pictureQuality = picQuality;
        hw_cinfo.thumbnailWidth = th_width;
        hw_cinfo.thumbnailHeight = th_height;
        hw_cinfo.thumbnailQuality = thumQuality;
        hw_cinfo.format = HAL_PIXEL_FORMAT_JZ_YUV_420_B;
        hw_cinfo.jpeg_out = (unsigned char*)(jpeg_buff->data);
        hw_cinfo.jpeg_size = &jpeg_size;
        hw_cinfo.th_jpeg_out = (jpeg_tn_buff==NULL) ? NULL : ((unsigned char*)(jpeg_tn_buff->data));
        hw_cinfo.th_jpeg_size = &thumb_size;
        hw_cinfo.tlb_addr = mDevice->getTlbBase();
        hw_cinfo.requiredMem = mget_memory;

        ccHW.setPrameters(&hw_cinfo);
        ccHW.hw_compress_to_jpeg();

        ExifElementsTable* exif = new ExifElementsTable();
        if (NULL != exif) {
            mJzParameters->setUpEXIF(exif);
            exif->insertExifToJpeg((unsigned char*)(jpeg_buff->data),jpeg_size);
            if (NULL != jpeg_tn_buff
                && jpeg_tn_buff->data != NULL) {
                if (th_width*th_height >= mCurrentFrame->width*mCurrentFrame->height) {
                    exif->insertExifThumbnailImage((const char*)(jpeg_buff->data), (int)jpeg_size);
                } else {
                    ccHW.rgb565_to_jpeg((uint8_t*)jpeg_tn_buff->data,
                                        &thumb_size,(uint8_t*)(captureHeap->data),
                                        th_width, th_height,thumQuality);
                    exif->insertExifThumbnailImage((const char*)(jpeg_tn_buff->data), (int)thumb_size);
                }
            }
            Section_t* exif_section = NULL;
            exif_section = FindSection(M_EXIF);
            if (NULL != exif_section) {
                jpegMem = mget_memory(-1, (jpeg_size + exif_section->Size), 1, NULL);
                if ((NULL != jpegMem) && (jpegMem->data != NULL)) {
                    exif->saveJpeg((unsigned char*)(jpegMem->data),(jpeg_size + exif_section->Size));
                    mdata_cb(CAMERA_MSG_COMPRESSED_IMAGE, jpegMem, 0, NULL, mcamera_interface);
                    jpegMem->release(jpegMem);
                    jpegMem = NULL;
                }
            }
        }
#else
        mdata_cb(CAMERA_MSG_COMPRESSED_IMAGE, captureHeap, 0, NULL, mcamera_interface);
#endif

        if (jpeg_buff != NULL) {
            jpeg_buff->release(jpeg_buff);
            jpeg_buff = NULL;
        }

        if (jpeg_tn_buff != NULL) {
            jpeg_tn_buff->release(jpeg_tn_buff);
            jpeg_tn_buff = NULL;
        }

        if (jpegMem) {
            jpegMem->release(jpegMem);
            jpegMem = NULL;
        }

        if (captureHeap != NULL) {
            dmmu_unmap_memory((uint8_t*)captureHeap->data,captureHeap->size);
            captureHeap->release(captureHeap);
            captureHeap = NULL;
        }
    }

    status_t CameraHal1::convertCurrentFrameToJpeg(camera_memory_t** jpeg_buff) {

        status_t ret = UNKNOWN_ERROR;
        camera_memory_t* tmp_buf = NULL;

        if (mListCaptureHeap.empty()) {
            ALOGE("%s: don't have capture heap",__FUNCTION__);
            return ret;
        }

        int picQuality = mJzParameters->getCameraParameters().getInt(CameraParameters::KEY_JPEG_QUALITY);
        int thumQuality = mJzParameters->getCameraParameters()
            .getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
        if (picQuality <= 0 || picQuality == 100) picQuality = 75;
        if (thumQuality <= 0 || thumQuality == 100) thumQuality = 75;

        compress_params_t params;
        memset(&params, 0, sizeof(compress_params_t));
        camera_memory_t* captureHeap = NULL;
        {
            AutoMutex lock(mcapture_lock);
            captureHeap = *(mListCaptureHeap.begin());
            mListCaptureHeap.erase(mListCaptureHeap.begin());
        }
        if (ccc && (mCurrentFrame->format == HAL_PIXEL_FORMAT_YV12
                    || mCurrentFrame->format == HAL_PIXEL_FORMAT_JZ_YUV_420_P)) {
            tmp_buf = mget_memory(-1,captureHeap->size,1,NULL);
            ccc->yuv420p_to_yuv420sp((uint8_t*)(captureHeap->data),
                                     (uint8_t*)tmp_buf->data,mCurrentFrame->width,mCurrentFrame->height);
            params.src = (uint8_t*)(tmp_buf->data);
            params.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
        } else {
            params.src = (uint8_t*)(captureHeap->data);
            params.format = mCurrentFrame->format;
        }
        params.pictureWidth = mCurrentFrame->width;
        params.pictureHeight = mCurrentFrame->height;
        params.pictureQuality = picQuality;
        params.thumbnailWidth =
            mJzParameters->getCameraParameters()
            .getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
        params.thumbnailHeight =
            mJzParameters->getCameraParameters()
            .getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
        params.thumbnailQuality = thumQuality;
        params.jpegSize = 0;
        params.requiredMem = mget_memory;

        int rot = mJzParameters->getCameraParameters()
            .getInt(CameraParameters::KEY_ROTATION);
        CameraCompressor compressor(&params,mirror,rot);
        ExifElementsTable* exif = new ExifElementsTable();
        if (NULL != exif) {
            mJzParameters->setUpEXIF(exif);
            ret = compressor.compress_to_jpeg(exif, jpeg_buff);
        }

        if (captureHeap != NULL) {
            dmmu_unmap_memory((uint8_t*)captureHeap->data,captureHeap->size);
            captureHeap->release(captureHeap);
            captureHeap = NULL;
        }
        if (tmp_buf != NULL) {
            tmp_buf->release(tmp_buf);
            tmp_buf = NULL;
        }
        return ret;
    }


    status_t CameraHal1::softFaceDetectStart(int32_t detect_type) {

        int w = mRawPreviewWidth;
        int h = mRawPreviewHeight;
        int maxFaces = 0;
        status_t res = NO_ERROR;

        switch(detect_type)
            {
            case CAMERA_FACE_DETECTION_HW:
                ALOGE("start hardware face detect");
                maxFaces = mJzParameters->getCameraParameters()
                    .getInt(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW);
                goto hard_detect_method;
                break;
            case CAMERA_FACE_DETECTION_SW:
                ALOGE("start Software face detection");
                maxFaces = mJzParameters->getCameraParameters()
                    .getInt(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_SW);
                goto soft_detect_method;
                break;
            }

    hard_detect_method:
    soft_detect_method:

        if (maxFaces == 0) {
            isSoftFaceDetectStart = false;
            return BAD_VALUE;
        }

        ALOGV("%s: max Face = %d", __FUNCTION__,maxFaces);

        res = CameraFaceDetect::getInstance()->initialize(w, h, maxFaces);
        if (res == NO_ERROR) {
            isSoftFaceDetectStart = true;
        } else {
            isSoftFaceDetectStart = false;
        }

        return res;
    }

    status_t CameraHal1::softFaceDetectStop(void) {
        if (isSoftFaceDetectStart) {
            isSoftFaceDetectStart = false;
            CameraFaceDetect::getInstance()->deInitialize();
        }
        return NO_ERROR;
    }

    void CameraHal1::x2d_convert_dataformat(CameraYUVMeta* yuvMeta, 
                                            uint8_t* dst_buf, buffer_handle_t *buffer) {

        struct jz_x2d_config x2d_cfg;
        IMG_native_handle_t* dst_handle = NULL;
        int map_size = 0;
        int ret = NO_ERROR;

        if (x2d_fd < 0) {
            ALOGE("%s: open %s error or not open it", __FUNCTION__, X2D_NAME);
            return;
        }

        dst_handle = (IMG_native_handle_t*)(*buffer);
        map_size = dst_handle->iStride * dst_handle->iHeight * (dst_handle->uiBpp >> 3);
        dmmu_map_memory((uint8_t*)dst_buf,map_size);

        mDevice->flushCache((void*)yuvMeta->yAddr, map_size);
        /* set dst configs */
        x2d_cfg.dst_address = (int)dst_buf;
        x2d_cfg.dst_width = dst_handle->iWidth;
        x2d_cfg.dst_height = dst_handle->iHeight;
        if (mPreviewWinFmt == HAL_PIXEL_FORMAT_RGB_565)
            x2d_cfg.dst_format = X2D_OUTFORMAT_RGB565;
        else if (mPreviewWinFmt == HAL_PIXEL_FORMAT_RGBA_8888
                 || mPreviewWinFmt == HAL_PIXEL_FORMAT_RGBX_8888
                 || mPreviewWinFmt == HAL_PIXEL_FORMAT_BGRA_8888)
            x2d_cfg.dst_format = X2D_OUTFORMAT_XRGB888;
        else if (mPreviewWinFmt == HAL_PIXEL_FORMAT_RGB_888)
            x2d_cfg.dst_format = X2D_OUTFORMAT_ARGB888;
        x2d_cfg.dst_stride = dst_handle->iStride * (dst_handle->uiBpp >> 3);
        x2d_cfg.dst_back_en = 0;
        x2d_cfg.dst_glb_alpha_en = 1;
        x2d_cfg.dst_preRGB_en = 0;
        x2d_cfg.dst_mask_en = 1;
        x2d_cfg.dst_alpha_val = 0x80;
        x2d_cfg.dst_bcground = 0xff0ff0ff;

        x2d_cfg.tlb_base = mDevice->getTlbBase();

        /* layer num */
        x2d_cfg.layer_num = 1;

        /* src yuv address */
        if (yuvMeta->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
            x2d_cfg.lay[0].addr = yuvMeta->yAddr;
            x2d_cfg.lay[0].u_addr = yuvMeta->yAddr + (yuvMeta->width*yuvMeta->height);

            x2d_cfg.lay[0].v_addr = (int)(x2d_cfg.lay[0].u_addr);
            x2d_cfg.lay[0].y_stride = yuvMeta->yStride/16;
            x2d_cfg.lay[0].v_stride = yuvMeta->vStride/16;

            /* src data format */
            x2d_cfg.lay[0].format = X2D_INFORMAT_TILE420;
        } else if (yuvMeta->format ==  HAL_PIXEL_FORMAT_JZ_YUV_420_P) {
            x2d_cfg.lay[0].addr = yuvMeta->yAddr;
            x2d_cfg.lay[0].u_addr = yuvMeta->uAddr;
            x2d_cfg.lay[0].v_addr = yuvMeta->vAddr;
            x2d_cfg.lay[0].y_stride = yuvMeta->yStride;
            x2d_cfg.lay[0].v_stride = yuvMeta->vStride;

            /* src data format */
            x2d_cfg.lay[0].format = X2D_INFORMAT_YUV420SP;
        } else {
            ALOGE("%s: preview format %d not support",__FUNCTION__, yuvMeta->format);
            return;
        }

        /* src rotation degree */
        x2d_cfg.lay[0].transform = X2D_ROTATE_0;

        /* src input geometry && output geometry */
        x2d_cfg.lay[0].in_width =  yuvMeta->width;
        x2d_cfg.lay[0].in_height = yuvMeta->height;
        x2d_cfg.lay[0].out_width = dst_handle->iWidth;
        x2d_cfg.lay[0].out_height = dst_handle->iHeight;
        x2d_cfg.lay[0].out_w_offset = 0;
        x2d_cfg.lay[0].out_h_offset = 0;
        x2d_cfg.lay[0].mask_en = 0;
        x2d_cfg.lay[0].msk_val = 0xffffffff;
        x2d_cfg.lay[0].glb_alpha_en = 1;
        x2d_cfg.lay[0].global_alpha_val = 0xff;
        x2d_cfg.lay[0].preRGB_en = 1;

        /* src scale ratio set */
        float v_scale, h_scale;
        switch (x2d_cfg.lay[0].transform) {
        case X2D_H_MIRROR:
        case X2D_V_MIRROR:
        case X2D_ROTATE_0:
        case X2D_ROTATE_180:
            h_scale = (float)x2d_cfg.lay[0].in_width / (float)x2d_cfg.lay[0].out_width;
            v_scale = (float)x2d_cfg.lay[0].in_height / (float)x2d_cfg.lay[0].out_height;
            x2d_cfg.lay[0].h_scale_ratio = (int)(h_scale * X2D_SCALE_FACTOR);
            x2d_cfg.lay[0].v_scale_ratio = (int)(v_scale * X2D_SCALE_FACTOR);
            break;
        case X2D_ROTATE_90:
        case X2D_ROTATE_270:
            h_scale = (float)x2d_cfg.lay[0].in_width / (float)x2d_cfg.lay[0].out_height;
            v_scale = (float)x2d_cfg.lay[0].in_height / (float)x2d_cfg.lay[0].out_width;
            x2d_cfg.lay[0].h_scale_ratio = (int)(h_scale * X2D_SCALE_FACTOR);
            x2d_cfg.lay[0].v_scale_ratio = (int)(v_scale * X2D_SCALE_FACTOR);
            break;
        default:
            dmmu_unmap_memory((uint8_t*)dst_buf,map_size);
            ALOGE("%s %s %d:undefined rotation degree!!!!", __FILE__, __FUNCTION__, __LINE__);
            return;
        }

        /* ioctl set configs */
        ret = ioctl(x2d_fd, IOCTL_X2D_SET_CONFIG, &x2d_cfg);
        if (ret < 0) {
            dmmu_unmap_memory((uint8_t*)dst_buf,map_size);
            ALOGE("%s %s %d: IOCTL_X2D_SET_CONFIG failed", __FILE__, __FUNCTION__, __LINE__);
            return ;
        }

        /* ioctl start compose */
        ret = ioctl(x2d_fd, IOCTL_X2D_START_COMPOSE);
        if (ret < 0) {
            dmmu_unmap_memory((uint8_t*)dst_buf,map_size);
            ALOGE("%s %s %d: IOCTL_X2D_START_COMPOSE failed", __FILE__, __FUNCTION__, __LINE__);
            return ;
        }
        dmmu_unmap_memory((uint8_t*)dst_buf,map_size);
    }

    void CameraHal1::ipu_convert_dataformat(CameraYUVMeta* yuvMeta,
                                            uint8_t* dst_buf, buffer_handle_t *buffer) {

        struct source_data_info *srcInfo;
        struct ipu_data_buffer* srcBuf;
        struct dest_data_info* dstInfo;
        struct ipu_data_buffer* dstBuf;

        int err = 0;
        static int oldzoomval = 0;
        int must_do = 0;
        int bytes_per_pixel = 2;
        int cropLeft = 0;
        int cropTop = 0;
        int offset = 0;
        int map_size = 0;

        if (ipu_open_status == false) {
            ALOGE("%s: open ipu error or not open it", __FUNCTION__);
            return;
        }

        IMG_native_handle_t* dst_handle = NULL;
        dst_handle = (IMG_native_handle_t*)(*buffer);

        map_size = dst_handle->iStride * dst_handle->iHeight * (dst_handle->uiBpp >> 3);
        dmmu_map_memory((uint8_t*)dst_buf,map_size);

        mDevice->flushCache((void*)yuvMeta->yAddr, map_size);
        mzoomVal = mJzParameters->getCameraParameters().getInt(CameraParameters::KEY_ZOOM);
        switch(mzoomVal){
        case 0:
            mzoomRadio = 100;
            break;
        case 1:
            mzoomRadio = 200;
            break;
        case 2:
            mzoomRadio = 250;
            break;
        case 3:
            mzoomRadio = 250;
            break;
        case 4:
            mzoomRadio = 400;
            break;
        }

        if(mzoomVal != oldzoomval){
            oldzoomval = mzoomVal;
            must_do = 1;
        }

        srcInfo = &(mipu->src_info);
        srcBuf = &(mipu->src_info.srcBuf);
        memset(srcInfo, 0, sizeof(struct source_data_info));
        srcInfo->fmt = yuvMeta->format;
        srcInfo->is_virt_buf = 1;
        srcInfo->width = num2even(yuvMeta->width * 100 / mzoomRadio);
        srcInfo->height = num2even(yuvMeta->height * 100 / mzoomRadio);
        srcInfo->stlb_base = mDevice->getTlbBase();

        cropLeft = num2even((dst_handle->iWidth - srcInfo->width) / 2);
        cropLeft = (cropLeft < 0) ? 0 : cropLeft;
        cropTop = num2even((dst_handle->iHeight - srcInfo->height) / 2);
        cropTop = (cropTop < 0) ? 0 : cropTop;
        offset = (cropTop * dst_handle->iWidth + cropLeft) * bytes_per_pixel;
        offset = (offset < 0) ? 0 : offset;
        ALOGV("srcInfo->width = %d, srcInfo->height = %d, cropLeft =%d, cropTop = %d, offset = %d, mzoomRadio: %d",
               srcInfo->width, srcInfo->height, cropLeft, cropTop, offset, mzoomRadio);

        if (yuvMeta->format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
            srcBuf->y_buf_v = (void*)(yuvMeta->yAddr + offset);
            srcBuf->u_buf_v = (void*)(yuvMeta->yAddr + offset);
            srcBuf->v_buf_v = (void*)(yuvMeta->yAddr + offset);

            if (mDevice->usePmem()) {
                srcBuf->y_buf_phys = yuvMeta->yPhy + offset; 
                srcBuf->u_buf_phys = 0;
                srcBuf->v_buf_phys = 0;
            } else {
                srcBuf->y_buf_phys = 0;
                srcBuf->u_buf_phys = 0;
                srcBuf->v_buf_phys = 0;
            }

            srcBuf->y_stride = yuvMeta->yStride;
            srcBuf->u_stride = yuvMeta->uStride;
            srcBuf->v_stride = yuvMeta->vStride;
        } else if (yuvMeta->format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
            srcBuf->y_buf_v = (void*)yuvMeta->yAddr;
            srcBuf->u_buf_v = (void*)(yuvMeta->yAddr + (yuvMeta->width*yuvMeta->height)) ;
            srcBuf->v_buf_v = (void*)srcBuf->u_buf_v;
            srcBuf->y_stride = yuvMeta->yStride;
            srcBuf->u_stride = yuvMeta->uStride;
            srcBuf->v_stride = yuvMeta->vStride;
        } else if (yuvMeta->format ==  HAL_PIXEL_FORMAT_JZ_YUV_420_P) {
            srcBuf->y_buf_v = (void*)yuvMeta->yAddr;
            srcBuf->u_buf_v = (void*)yuvMeta->uAddr;
            srcBuf->v_buf_v = (void*)yuvMeta->vAddr;

            if (mDevice->usePmem()) {
                srcBuf->y_buf_phys = yuvMeta->yPhy;
                srcBuf->u_buf_phys = yuvMeta->uPhy;
                srcBuf->v_buf_phys = yuvMeta->vPhy;
            } else {
                srcBuf->y_buf_phys = 0;
                srcBuf->u_buf_phys = 0;
                srcBuf->v_buf_phys = 0;
            }

            srcBuf->y_stride = yuvMeta->yStride;
            srcBuf->u_stride = yuvMeta->uStride;
            srcBuf->v_stride = yuvMeta->vStride;
        } else {
            ALOGE("%s: preview format %d not support",__FUNCTION__, mCurrentFrame->format);
            dmmu_unmap_memory((uint8_t*)dst_buf,map_size);
            return;
        }

        dstInfo = &(mipu->dst_info);
        dstBuf = &(dstInfo->dstBuf);
        memset(dstInfo, 0, sizeof(struct dest_data_info));

        dstInfo->dst_mode = IPU_OUTPUT_TO_FRAMEBUFFER | IPU_OUTPUT_BLOCK_MODE;
        dstInfo->fmt = dst_handle->iFormat;
        dstInfo->dtlb_base = mDevice->getTlbBase();

        dstInfo->left = 0;
        dstInfo->top = 0;

        dstInfo->width = dst_handle->iWidth;
        dstInfo->height = dst_handle->iHeight;
        dstInfo->out_buf_v = dst_buf;
        dstBuf->y_buf_v = (void*)(dst_buf);
        dstBuf->y_stride = dst_handle->iStride * (dst_handle->uiBpp >> 3);
        err = init_ipu_dev(yuvMeta->width, yuvMeta->height, yuvMeta->format, must_do);
        if (err < 0) {
            ALOGE("ipu init failed ipuHalder = %p", mipu);
            dmmu_unmap_memory((uint8_t*)dst_buf,map_size);
            return;
        }
        ipu_postBuffer(mipu);
        dmmu_unmap_memory((uint8_t*)dst_buf,map_size);
    }

    status_t CameraHal1::ipu_zoomIn_scale(uint8_t* dest, int dest_width, int dest_height,
                                          uint8_t* src, int src_width, int src_height, int src_format,
                                          int stride_mul, int src_stride) {

        struct source_data_info *srcInfo;
        struct ipu_data_buffer* srcBuf;
        struct dest_data_info* dstInfo;
        struct ipu_data_buffer* dstBuf;

        int err = 0;
        int stride_shift = 0;
        int must_do = 0;
        int map_size = 0;
        static int oldzoomval = 0;

        if (ipu_open_status == false) {
            ALOGE("%s: open ipu error or not open it", __FUNCTION__);
            return BAD_VALUE;
        }

        if (src_format != HAL_PIXEL_FORMAT_YCbCr_422_I
            && src_format != HAL_PIXEL_FORMAT_JZ_YUV_420_P
            && src_format != HAL_PIXEL_FORMAT_YV12
            && src_format != HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
            ALOGE("%s: don't support format 0x%x for up scale",__FUNCTION__,
                  src_format);
            return BAD_VALUE;
        }

        if(stride_mul == 2){
            stride_shift = 1;
            must_do = 1;
        }else{
            stride_shift = 0;
            if((mzoomVal == 1) && (dest_width == 1600)) {
                must_do = 1;
            } else {
                must_do = 0;
            }
        }

        srcInfo = &(mipu->src_info);
        srcBuf = &(mipu->src_info.srcBuf);
        memset(srcInfo, 0, sizeof(struct source_data_info));
        srcInfo->fmt = src_format;
        srcInfo->is_virt_buf = 1;
        srcInfo->width = src_width;
        srcInfo->height = src_height;
        srcInfo->stlb_base = mDevice->getTlbBase();

        if (src_format == HAL_PIXEL_FORMAT_YV12
            || src_format == HAL_PIXEL_FORMAT_JZ_YUV_420_P) {
            srcBuf->y_buf_v = (void*)src;
            srcBuf->u_buf_v = (void*)(src + src_width*src_height);
            srcBuf->v_buf_v = (void*)((uint8_t*)srcBuf->u_buf_v + (src_width*src_height>>2));
            srcBuf->y_stride = src_width;
            srcBuf->u_stride = src_width>>1;
            srcBuf->v_stride = src_width>>1;
        } else if (src_format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
            srcBuf->y_buf_v = (void*)src;
            srcBuf->u_buf_v = (void*)src;
            srcBuf->v_buf_v = (void*)src;
            srcBuf->y_stride = src_stride<<1 << stride_shift;
            srcBuf->u_stride = src_stride<<1 << stride_shift;
            srcBuf->v_stride = src_stride<<1 << stride_shift;
        } else if (src_format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
            srcBuf->y_buf_v = (void*)src;
            srcBuf->u_buf_v = (void*)(src + (src_width*src_height));
            srcBuf->v_buf_v = srcBuf->u_buf_v;
            srcBuf->y_stride = src_width;
            srcBuf->u_stride = src_width>>1;
            srcBuf->v_stride = src_width>>1;
        }
        //-----------------------------
        map_size = dest_width * dest_height * 2;
        dmmu_map_memory((uint8_t*)dest,map_size);
        int flush_size = src_width * src_height * 2;
        mDevice->flushCache((void*)src, flush_size);

        dstInfo = &(mipu->dst_info);
        dstBuf = &(dstInfo->dstBuf);
        memset(dstInfo, 0, sizeof(struct dest_data_info));
        dstInfo->dst_mode = IPU_OUTPUT_TO_FRAMEBUFFER
            | IPU_OUTPUT_BLOCK_MODE;
        dstInfo->fmt = src_format;
        dstInfo->dtlb_base = mDevice->getTlbBase();

        dstInfo->left = 0;
        dstInfo->top = 0;
        dstInfo->width = dest_width;
        dstInfo->height = dest_height;
        dstInfo->out_buf_v = dest;
        if (src_format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
            dstBuf->y_buf_v = (void*)(dest);
            dstBuf->u_buf_v = (void*)(dest);
            dstBuf->v_buf_v = (void*)(dest);
            dstBuf->y_stride = dest_width<<1 << stride_shift;
            dstBuf->u_stride = dest_width<<1 << stride_shift;
            dstBuf->v_stride = dest_width<<1 << stride_shift;
        } else if (src_format == HAL_PIXEL_FORMAT_JZ_YUV_420_P
                   || src_format == HAL_PIXEL_FORMAT_YV12) {
            dstBuf->y_buf_v = (void*)(dest);
            dstBuf->u_buf_v = (void*)(dest + dest_width*dest_height);
            dstBuf->v_buf_v = (void*)((uint8_t*)dstBuf->u_buf_v + (dest_width*dest_height>>2));
            dstBuf->y_stride = dest_width;
            dstBuf->u_stride = dest_width>>1;
            dstBuf->v_stride = dest_width>>1;
        } else if (src_format == HAL_PIXEL_FORMAT_JZ_YUV_420_B) {
            dstBuf->y_buf_v = (void*)(dest);
            dstBuf->u_buf_v = (void*)(dest + dest_width*dest_height);
            dstBuf->v_buf_v = dstBuf->u_buf_v;
            dstBuf->y_stride = dest_width;
            dstBuf->u_stride = dest_width>>1;
            dstBuf->v_stride = dest_width>>1;
        }

        err = init_ipu_dev(src_width,src_height,src_format, must_do);
        if (err < 0) {
            ALOGE("ipu init failed ipuHalder = %p", mipu);
            dmmu_unmap_memory((uint8_t*)dest,map_size);
            return BAD_VALUE;
        }

        ipu_postBuffer(mipu);
        dmmu_unmap_memory((uint8_t*)dest,map_size);
        return NO_ERROR;
    }

    void CameraHal1::open_ipu_dev(void) {

        if (ipu_open_status)
            return;

        if (ipu_open(&mipu) < 0) {
            ALOGE("ipu_open() failed ipuHandler");
            if (mipu != NULL) ipu_close(&mipu);
            mipu = NULL;
            ipu_open_status = false;
            init_ipu_first = true;
            return ;
        }

        ipu_open_status = true;
        init_ipu_first = true;
        return ;
    }

    int CameraHal1::init_ipu_dev(int w, int h, int mat, int must_do) {

        static int width = 0;
        static int height= 0;
        static int format = 0;

        if (!ipu_open_status)
            return -1;

        if (init_ipu_first) {
            init_ipu_first = false;
            width = 0;
            height = 0;
            format = 0;
        }

        if (w != width || h != height || mat != format || must_do != 0) {

            width = w;
            height = h;
            format = mat;

            ALOGV("%s: width: %d, height: %d, format: 0x%x,must_do: %d",
                  __FUNCTION__, width, height, format,must_do);

            if((mzoomVal == 1) && (must_do == 1) && (w == 800))
                init_ipu_first = true;

            if (ipu_init(mipu) < 0) {
                if (mipu != NULL) ipu_close(&mipu);
                mipu = NULL;
                ipu_open_status = false;
                init_ipu_first = true;
                ALOGE("%s: ipu init fail",__FUNCTION__);
                return -1;
            }
        }
        return 0;
    }

    void CameraHal1::close_ipu_dev(void) {

        AutoMutex lock(mlock);

        if (ipu_open_status == false)
            return;

        int err = 0;
        if (mipu != NULL) err = ipu_close(&mipu);
        if (err < 0) {
            ALOGE("ipu_close failed ipuHalder = %p", mipu);
        }
        mipu = NULL;
        ipu_open_status = false;
        init_ipu_first = true;
    }

    void CameraHal1::open_x2d_dev(void) {

        x2d_fd = open (X2D_NAME, O_RDWR);
        if (x2d_fd < 0) {
            ALOGE("%s: open %s error, %s",
                  __FUNCTION__, X2D_NAME, strerror(errno));
        }
    }

    void CameraHal1::close_x2d_dev(void) {

        AutoMutex lock(mlock);

        if (x2d_fd > 0) {
            close(x2d_fd);
            x2d_fd = -1;
        }
    }

    void CameraHal1::dump_data(bool isdump) {

        static int j = 0;

        if (!isdump)
            return;

        if (j < 10 ) {
            int size = getCurrentFrameSize();
            char filename1[20] = {0};
            sprintf(filename1, "/data/vpu/cameraHalb%d-%x.raw",j++,mCurrentFrame->yAddr);
            FILE* fp = fopen(filename1, "w+");

            if (fp != NULL) {
                fwrite((uint8_t*)(mCurrentFrame->yAddr), size,1, fp);
                fclose(fp);
                fp = NULL;
            }
        }

    }

    //------------------------------------------------------------------//

    /**
       there is call by camera service */

    camera_device_ops_t CameraHal1::mCamera1Ops = {
        CameraHal1::set_preview_window,
        CameraHal1::set_callbacks,
        CameraHal1::enable_msg_type,
        CameraHal1::disable_msg_type,
        CameraHal1::msg_type_enabled,
        CameraHal1::start_preview,
        CameraHal1::stop_preview,
        CameraHal1::preview_enabled,
        CameraHal1::store_meta_data_in_buffers,
        CameraHal1::start_recording,
        CameraHal1::stop_recording,
        CameraHal1::recording_enabled,
        CameraHal1::release_recording_frame,
        CameraHal1::auto_focus,
        CameraHal1::cancel_auto_focus,
        CameraHal1::take_picture,
        CameraHal1::cancel_picture,
        CameraHal1::set_parameters,
        CameraHal1::get_parameters,
        CameraHal1::put_parameters,
        CameraHal1::send_command,
        CameraHal1::release,
        CameraHal1::dump
    };

    int CameraHal1::set_preview_window(struct camera_device* device,
                                       struct preview_stream_ops *window)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(device->priv);
          
        if (ch1 == NULL)
            {
                ALOGE("%s: camera hal1 is null",__FUNCTION__);
                return NO_MEMORY;
            }
        return ch1->setPreviewWindow(window);
    }

    void CameraHal1::set_callbacks(
                                   struct camera_device* dev,
                                   camera_notify_callback notify_cb,
                                   camera_data_callback data_cb,
                                   camera_data_timestamp_callback data_cb_timestamp,
                                   camera_request_memory get_memory,
                                   void* user)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->setCallbacks(notify_cb, data_cb, data_cb_timestamp, get_memory, user);
    }

    void CameraHal1::enable_msg_type(struct camera_device* dev, int32_t msg_type)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->enableMsgType(msg_type);
    }

    void CameraHal1::disable_msg_type(struct camera_device* dev, int32_t msg_type)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->disableMsgType(msg_type);
    }

    int CameraHal1::msg_type_enabled(struct camera_device* dev, int32_t msg_type)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->isMsgTypeEnabled(msg_type);
    }

    int CameraHal1::start_preview(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->startPreview();
    }

    void CameraHal1::stop_preview(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->stopPreview();
    }

    int CameraHal1::preview_enabled(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->isPreviewEnabled();
    }

    int CameraHal1::store_meta_data_in_buffers(struct camera_device* dev,
                                               int enable)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->storeMetaDataInBuffers(enable);
    }

    int CameraHal1::start_recording(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->startRecording();
    }

    void CameraHal1::stop_recording(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->stopRecording();
    }

    int CameraHal1::recording_enabled(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->isRecordingEnabled();
    }

    void CameraHal1::release_recording_frame(struct camera_device* dev,
                                             const void* opaque)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->releaseRecordingFrame(opaque);
    }

    int CameraHal1::auto_focus(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->setAutoFocus();
    }

    int CameraHal1::cancel_auto_focus(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->cancelAutoFocus();
    }

    int CameraHal1::take_picture(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->takePicture();
    }

    int CameraHal1::cancel_picture(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->cancelPicture();
    }

    int CameraHal1::set_parameters(struct camera_device* dev, const char* parms)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->setParameters(parms);
    }

    char* CameraHal1::get_parameters(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NULL;
        }
        return ch1->getParameters();
    }

    void CameraHal1::put_parameters(struct camera_device* dev, char* params)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->putParameters(params);
    }

    int CameraHal1::send_command(struct camera_device* dev,
                                 int32_t cmd,
                                 int32_t arg1,
                                 int32_t arg2)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->sendCommand(cmd, arg1, arg2);
    }

    void CameraHal1::release(struct camera_device* dev)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return;
        }
        ch1->releaseCamera();
    }

    int CameraHal1::dump(struct camera_device* dev, int fd)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(dev->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->dumpCamera(fd);
    }

    int CameraHal1::device_close(struct hw_device_t* device)
    {
        CameraHal1* ch1 = reinterpret_cast<CameraHal1*>(reinterpret_cast<struct camera_device*>(device)->priv);
        if (ch1 == NULL) {
            ALOGE("%s: camera hal1 is null",__FUNCTION__);
            return NO_MEMORY;
        }
        return ch1->deviceClose();
    }

    CameraHal1::WorkThread::ControlCmd 
    CameraHal1::WorkThread::receiveCmd(int fd, int64_t timeout) {

        fd_set fds[1];
        struct timeval tv;

        const int fd_num = (fd >= 0) ? MAX(fd, mControlFd) + 1:
            mControlFd + 1;

        FD_ZERO(fds);
        FD_SET(mControlFd,fds);
        if (fd >= 0)
            FD_SET(fd, fds);

        if (timeout > 0 && ((timeout/1000) > 1000)) {
            tv.tv_sec = timeout / 1000000000LL;
            tv.tv_usec = (timeout % 1000000000LL)/1000;
        } else {
            tv.tv_sec = 0;
            tv.tv_usec = 0;
        }

        int res = select(fd_num, fds, NULL, NULL, &tv);
        if ((res > 0) && FD_ISSET(mControlFd, fds)) {
            ControlCmd msg;
            res = read(mControlFd, &msg, sizeof(msg));
            if (res == sizeof(msg))
                return msg;
        }
        if (res == 0)
            return THREAD_TIMEOUT;
        return THREAD_ERROR;
    }
}; // end namespace
