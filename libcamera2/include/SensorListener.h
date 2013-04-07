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
#ifndef ANDROID_CAMERA_HARDWARE_SENSOR_LISTENER_H
#define ANDROID_CAMERA_HARDWARE_SENSOR_LISTENER_H

#include <android/sensor.h>
#include <gui/Sensor.h>
#include <gui/SensorManager.h>
#include <gui/SurfaceComposerClient.h>
#include <utils/Looper.h>

namespace android {

    class SensorLooperThread : public Thread {
    public:
        SensorLooperThread(Looper* looper)
            :Thread(false) {
            mLooper = sp<Looper>(looper);
        }

        ~SensorLooperThread() {
            mLooper.clear();
        }

        virtual bool threadLoop() {
            int32_t ret = mLooper->pollOnce(-1);
            return true;
        }

        void wake() {
            mLooper->wake();
        }

    private:
        sp<Looper> mLooper;       
    };

    class SensorListener : public RefBase
    {
    public:
        typedef enum {
            SENSOR_ACCELEROMETER  = 1 << 0,
            SENSOR_MAGNETIC_FIELD = 1 << 1,
            SENSOR_GYROSCOPE      = 1 << 2,
            SENSOR_LIGHT          = 1 << 3,
            SENSOR_PROXIMITY      = 1 << 4,
            SENSOR_ORIENTATION    = 1 << 5,
        } sensor_type_t;

    public:
        SensorListener();
        ~SensorListener();
        status_t initialize(void);

        int getCurOrientation(void) {
            return mCurOrientation;
        }

        int getOrientationCompensation() {
            return mOrientationCompensation;
        }

        void setOrientation(int orient);

        int getRegisterHandle(void) {
            return mRegisterHandle;
        }

        int getDefaultDisplayRotation(void);
        void enableSensor(sensor_type_t type);
        void disableSensor(sensor_type_t type);
        
    public:
        sp<SensorEventQueue> mSensorEventQueue;

    private:
        int mCurOrientation;
        int sensorsEnabled;
        int mRegisterHandle;
        int mOrientationCompensation;
        sp<Looper> mLooper;
        sp<SensorLooperThread> mSensorLooperThread;
        mutable Mutex mLock;
    };
}
#endif
