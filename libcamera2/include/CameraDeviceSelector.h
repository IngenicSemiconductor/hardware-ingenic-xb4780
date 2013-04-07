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


#ifndef __CAMERA_DEVICE_SELECTOR_H_
#define __CAMERA_DEVICE_SELECTOR_H_
//#define LOG_NDEBUG 0

#include "CameraCIMDevice.h"
#include "CameraV4L2Device.h"
#include "CameraHalCommon.h"

#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#define MAX_CAMERA_DEVICES 8
#define MAX_DEVICE_DRIVER_NAME 80

namespace android {
  
    class CameraDeviceSelector {
    
    public:
        CameraDeviceSelector()
            :mDevice(NULL),
             current_device(NULL),
             device_count(0),
             current_device_index(-1) {

            for (int i=0; i<MAX_CAMERA_DEVICES; ++i) {
                memset(deviceName[i], 0, MAX_DEVICE_DRIVER_NAME);
            }

            if (access(CameraCIMDevice::path, R_OK|W_OK) == 0) {
                add_device(CameraCIMDevice::getInstance(),CameraCIMDevice::path);
            }

            add_v4l2_devices();
        }

        ~CameraDeviceSelector() {

            int i = 0;
                   
            if (mDevice != NULL) {
                for (i = 0; i < 1; ++i) {
                    if (mDevice[i] != NULL) {
                        delete mDevice[i];
                        mDevice[i] = NULL;
                    }
                }
            }

            free(mDevice);
            device_count = 0;
            mDevice = NULL;
            current_device = NULL;
            current_device_index = -1;
        }

        CameraDeviceCommon* getDevice(void) {
            return current_device;
        }

        void tryAddDevice(void) {
            add_v4l2_devices();
        }

        void resetDeviceIndex(void) {
            current_device_index = -1;
        }

        void update_device(CameraHalCommon* hal, int id = 0) {
            tryAddDevice();
            if (selectDevice(id) == NO_ERROR) {
                hal->update_device(getDevice());
                getDevice()->update_device_name(deviceName[current_device_index], 
                  strlen(deviceName[current_device_index]) + 1);
            }
        }

        int selectDevice(int id = 0) {

            int ret = -1;
            int device_index = current_device_index;

            if (device_count > 0) {
                for (int i=0; i<device_count; ++i) {

                    if (device_count == 2) {
                        device_index = (id == 0) ? 1 : 0;
                    } else {
                        device_index = (device_index+1)%device_count;
                    }
                    if (access(deviceName[device_index],R_OK|W_OK) == 0) {
                        current_device_index = device_index;
                        current_device = mDevice[current_device_index];
                        ret = 0;
                        break;
                    } else {
                        ALOGE("remove device: %s",deviceName[device_index]);
                        memset(deviceName[device_index], 0, MAX_DEVICE_DRIVER_NAME);
                        device_index = (device_index-1)%device_count;
                        if (device_index < 0) {
                            device_index = device_count;
                            device_index = (device_index-1)%device_count;
                        }
                        device_count--;
                        continue;
                    }
                }
            }
            return ret;
        }

    private:

        void add_device(CameraDeviceCommon* device, const char* device_name) {

            if (mDevice == NULL) {
                mDevice = (CameraDeviceCommon**) malloc(sizeof(CameraDeviceCommon*));
                device_count = 0;
                current_device_index = -1;
                current_device = NULL;
            }

            if (device_count == MAX_CAMERA_DEVICES) {
                int index = -1;

                if ((index = findValidSlot()) < 0) {
                    return;
                }
                strcpy(deviceName[index], device_name);
                mDevice[index] = device;
                return;
            }

            for (int i = 0; i < device_count; ++i) {
                if (strcmp(deviceName[i], device_name) == 0) {
                    ALOGV("%s already add", device_name);
                    return;
                }
            }
            memset(deviceName[device_count], 0, MAX_DEVICE_DRIVER_NAME);
            strcpy(deviceName[device_count], device_name);
            ALOGE("add device path: %s",deviceName[device_count]);
            mDevice[device_count] = device;
            device_count += 1;
            mDevice = (CameraDeviceCommon**)realloc((CameraDeviceCommon**)mDevice,
                        (device_count + 1) * sizeof(CameraDeviceCommon*));
        }

        int findValidSlot(void) {

            int index = -1;
            int i = 0;

            for (; i < device_count; ++i) {
                if (access(deviceName[i],R_OK|W_OK) != 0) {
                    index = i;
                    memset(deviceName[i], 0, MAX_DEVICE_DRIVER_NAME);
                    break;
                }
            }
            return index;
        }

        void add_v4l2_devices(void) {

            int offset = 0;
            char device_name[MAX_DEVICE_DRIVER_NAME];
            char path[] = "/sys/class/video4linux";
            struct v4l2_capability v4l2_cap;
            DIR* dir = NULL;
            struct dirent *ent;
            int num = 0;

            memset (device_name, 0, MAX_DEVICE_DRIVER_NAME);
            dir = opendir(path);
            if (dir == NULL) {
                ALOGV("%s don't exists.",path);
                return;
            }

            while ((ent = readdir(dir))) {
                if (strncmp(ent->d_name,"video",5)) {
                    continue;
                }

                offset = snprintf(device_name, MAX_DEVICE_DRIVER_NAME,
                                  "/dev/%s",ent->d_name);
                device_name[offset+1] = '\0';
                if (access(device_name, R_OK|W_OK) == 0) {
                    num++;
                    add_device(CameraV4L2Device::getInstance(),device_name);
                } else {
                    ALOGE("could not open %s,error: %s",
                          device_name, strerror(errno));
                    continue;
                }
            }
            closedir(dir);
            dir = NULL;
            CameraV4L2Device::getInstance()->setDeviceCount(num);
        }

    private:
        CameraDeviceCommon** mDevice;
        CameraDeviceCommon* current_device;
        char deviceName[MAX_CAMERA_DEVICES][MAX_DEVICE_DRIVER_NAME];
        int device_count;
        int current_device_index;
        mutable Mutex mlock;
    };

};//end namespace

#endif
