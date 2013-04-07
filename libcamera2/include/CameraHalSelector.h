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

#ifndef __CAMERA_HAL_SELECTOR_H_
#define __CAMERA_HAL_SELECTOR_H_

#ifdef CAMERA_VERSION1
#include "CameraHal1.h"
#endif

#ifdef CAMERA_VERSION2
#include "CameraHal2.h"
#endif

#include "CameraDeviceSelector.h"
#include <CameraService.h>

namespace android {

    class CameraHalSelector {

    public:
        CameraHalSelector();
        virtual ~CameraHalSelector();

    public:
        virtual int get_profile_number_cameras(void);

        int getHalVersion() {
            return mversion;
        }

        void setCurrentCameraId(int id) {
            mCurrentId = id;
        }

        int getCurrentCameraId(void) {
            return mCurrentId;
        }

        CameraDeviceSelector* getDeviceSelector(void) {
            return device_selector;
        }
    
    public:
        static struct hw_module_methods_t mCameraModuleMethods;

        static int get_number_of_cameras(void);

        static int get_camera_info(int camera_id, struct camera_info* info);

    private:
        static int hw_module_open(const hw_module_t* module,
                                  const char* id,
                                  hw_device_t** device);
    private :
        mutable Mutex mLock;
        int mCameraNum;
        int mCurrentId;
        int mversion;
        CameraDeviceSelector* device_selector;

    public:
        void selector_lock(void) {
            mLock.lock();
        }

        void selector_unlock(void) {
            mLock.unlock();
        }
        int getNumberCamera(void);
        CameraDeviceCommon* getDevice() {
            if (device_selector != NULL) {
                return device_selector->getDevice();
            } else {
                return NULL;
            }
        }

    public:
        CameraHalCommon** mHal;
    };

    extern  CameraHalSelector gCameraHalSelector ;
}; // end namespace

#endif
