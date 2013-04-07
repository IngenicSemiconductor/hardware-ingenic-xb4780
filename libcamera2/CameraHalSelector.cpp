/*
 * Camera HAL for Ingenic android 4.2
 *
 * Copyright 2011 Ingenic Semiconductor LTD.
 *
 * author: 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define LOG_TAG "CameraHalSelect"
//#define LOG_NDEBUG 0
#include "CameraHalSelector.h"
#include <media/MediaProfiles.h>

namespace android {
    CameraHalSelector::CameraHalSelector()
        :mLock("CameraHalSelector::Lock"),
         mCameraNum(0),
         mCurrentId(-1),
         mversion(1),
         device_selector(NULL),
         mHal(NULL) {

        char prop[16];
        int media_profile_camera_num = 0;

        /**
           start camera device
        */

        media_profile_camera_num = get_profile_number_cameras();
        device_selector = new CameraDeviceSelector();

        if (device_selector == NULL) {
            return;
        }

        if (device_selector->selectDevice() != NO_ERROR) {
            delete device_selector;
            device_selector = NULL;
            ALOGE("select device error");
            return;
        }
        device_selector->resetDeviceIndex();
        mCameraNum = device_selector->getDevice()->getCameraNum();
        if (mCameraNum == 0) {
            ALOGE("real camera number == 0");
            return;
        }

        if (media_profile_camera_num != mCameraNum) {
            ALOGE("media profile camera num = %d, real number = %d", media_profile_camera_num, mCameraNum);
        }

        if (mCameraNum > MAX_CAMERAS) {
            mCameraNum = MAX_CAMERAS;
        }

        ALOGV("%s: have %d number camera", __FUNCTION__,mCameraNum);

        mHal = new CameraHalCommon*[mCameraNum];
        if (mHal == NULL) {
            ALOGE("%s: Unable to allocate camera array", __FUNCTION__);
            return;
        }

        for (int i = 0; i < mCameraNum; ++i)
            mHal[i] = NULL;

#ifdef CAMERA_VERSION1
        mversion = 1;
        for (int i = 0; i < mCameraNum; ++i) {
            mHal[i] = new CameraHal1(i, device_selector->getDevice());
        }
#endif

#ifdef CAMERA_VERSION2
        mversion = 2;
        for (int i = 0; i < mCameraNum; ++i) {
            mHal[i] = new CameraHal2(i, device_selector->getDevice());
        }
#endif

    }

    CameraHalSelector::~CameraHalSelector() {

        if (NULL != mHal) {
            for (int n = 0; n < mCameraNum; n++) {
                if (mHal[n] != NULL) {
                    delete mHal[n];
                    mHal[n] = NULL;
                }
            }
            delete [] mHal;
            mHal = NULL;
        }

        mCurrentId = -1;
        if (device_selector != NULL) {
            delete device_selector;
            device_selector = NULL;
        }
    }

    int CameraHalSelector::getNumberCamera(void) {
        AutoMutex lock(mLock);

        if (device_selector == NULL) {
            ALOGE("device selector not new is null");
            mCameraNum = 0;
            return 0;
        }

        int tmp_num = device_selector->getDevice()->getCameraNum();
        if (tmp_num >= MAX_CAMERAS) {
            tmp_num = MAX_CAMERAS;
        }
        mCameraNum = tmp_num;
        return mCameraNum;
    }

    int CameraHalSelector::get_number_of_cameras(void) {
        int camera_num = gCameraHalSelector.getNumberCamera();

        return camera_num;
    }

    int CameraHalSelector::get_profile_number_cameras() {

        int camera_num = 0;
        int i = 0;

        MediaProfiles* profile = MediaProfiles::getInstance();

        while (profile->hasCamcorderProfile(i++,CAMCORDER_QUALITY_HIGH))
            camera_num++;

        return camera_num;
    }

    int CameraHalSelector::hw_module_open(const hw_module_t* module,
                                          const char* id,
                                          hw_device_t** device) {
        if (id == NULL) {
            ALOGE("%s: camera id invalid",__FUNCTION__);
            return INVALID_OPERATION;
        }
        int tmp_id = atoi(id);

        if (tmp_id >= gCameraHalSelector.getNumberCamera()) {
            ALOGE("%s: don't exist %d camera",__FUNCTION__,tmp_id);
            return INVALID_OPERATION;
        }

        for (int i = 0; i < gCameraHalSelector.getNumberCamera(); ++i) {
            if (gCameraHalSelector.mHal[i] != NULL
                && gCameraHalSelector.mHal[i]->getModuleInit()) {
                ALOGE("%s: camera %d already open",__FUNCTION__, i);
                if (tmp_id == i) {
                    return OK;
                } else {
                    return INVALID_OPERATION;
                }
            }
        }

        if (tmp_id < gCameraHalSelector.getNumberCamera()
            && gCameraHalSelector.mHal[tmp_id] != NULL) {
            gCameraHalSelector.getDeviceSelector()->update_device(gCameraHalSelector.mHal[tmp_id], tmp_id);
            return gCameraHalSelector.mHal[tmp_id]->module_open(module, id, device);
        }

        return INVALID_OPERATION;
    }

    hw_module_methods_t CameraHalSelector::mCameraModuleMethods = {
        CameraHalSelector::hw_module_open,
    };

    int CameraHalSelector::get_camera_info(int camera_id, struct camera_info* info) {

        status_t ret = NO_ERROR;

        if ((camera_id < 0) || (camera_id >= gCameraHalSelector.getNumberCamera())) {
            return BAD_VALUE;
        }

        info->facing = CAMERA_FACING_BACK;
        info->orientation = 0;

        if (NULL != gCameraHalSelector.mHal && gCameraHalSelector.mHal[camera_id] != NULL)
            {
                if (gCameraHalSelector.getCurrentCameraId() != camera_id) {
                    gCameraHalSelector.setCurrentCameraId(camera_id);
                }

                ret = gCameraHalSelector.mHal[camera_id]->get_cameras_info(camera_id, info);
            }
        return ret;
    }
    CameraHalSelector gCameraHalSelector ;
};
