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
#define LOG_TAG  "SensorListener"
//#define LOG_NDEBUG 0

#include "SensorListener.h"
#include <math.h>
#include <sys/types.h>
#include <stdint.h>

namespace android {

    /*** static declarations ***/
    // measured values on device...might need tuning
    static const int DEGREES_90_THRESH = 50;
    static const int DEGREES_180_THRESH = 170;
    static const int DEGREES_270_THRESH = 250;

    static const int ROTATION_0 = 0x0;
    static const int ROTATION_90 = 0x1;
    static const int ROTATION_180 = 0x2;
    static const int ROTATION_270 = 0x3;

    static const int ORIENTATION_UNKNOWN = -1;

    static int sensor_events_listener(int fd, int events, void* data) {
        SensorListener* listener = (SensorListener*) data;
        ASensorEvent sen_events;
        status_t res;
        res = listener->mSensorEventQueue->read(&sen_events, 1);
        if (res == 0) {
            res = listener->mSensorEventQueue->waitForEvent();
            if (res != NO_ERROR) {
                ALOGE("%s; waitForEvent error",__FUNCTION__);
                return -1;
            }
        }

        if (res <= 0) {
            ALOGE("%s: error",__FUNCTION__);
            return -1;
        }

        if (sen_events.sensor == listener->getRegisterHandle()) {
            float x = -sen_events.vector.v[0];
            float y = -sen_events.vector.v[1];
            float z = -sen_events.vector.v[2];
            float magnitude = x*x + y*y;
            int orientation = 0;
            // Don't trust the angle if the magnitude is small compared to the y value
            if (magnitude * 4 >= z*z) {
                float OneEightyOverPi = 57.29577957855f;
                float angle = (float)atan2f(-y, x) * OneEightyOverPi;
                orientation = 90 - (int)round(angle);
                while (orientation >= 360) {
                    orientation -= 360;
                }

                while (orientation < 0) {
                    orientation += 360;
                }
            }

            listener->setOrientation(orientation);
        }

        return 1;
    }

    SensorListener::SensorListener() {
        mCurOrientation = 0;
        sensorsEnabled = 0;
        mRegisterHandle = -1;
        mOrientationCompensation = 0;
    }

    SensorListener::~SensorListener() {
        if (mSensorLooperThread.get()) {
            mSensorLooperThread->requestExit();
            mSensorLooperThread->wake();
            mSensorLooperThread->join();
            mSensorLooperThread.clear();
            mSensorLooperThread = NULL;
        }

        if (mLooper.get()) {
            mLooper->removeFd(mSensorEventQueue->getFd());
            mLooper.clear();
            mLooper = NULL;
        }
    }

    status_t SensorListener::initialize() {
        status_t ret =  NO_ERROR;

        SensorManager& mgr(SensorManager::getInstance());

        // Sensor const* const* sensorList;
        // size_t count = mgr.getSensorList(&sensorList);
        // ALOGV("%s: sensor have %d num",__FUNCTION__, count);
        // for (unsigned int i = 0; i < count; ++i) {
        //     ALOGV(" sensor name: %s", sensorList[i]->getName().string());
        //     ALOGV(" sensor vendor: %s",sensorList[i]->getVendor().string());
        //     ALOGV(" sensor version: %d", sensorList[i]->getVersion());
        //     ALOGV(" sensor handle: %d", sensorList[i]->getHandle());
        //     ALOGV(" sensor type: %d", sensorList[i]->getType());
        //     ALOGV(" sensor range: %f", sensorList[i]->getMaxValue());
        //     ALOGV(" sensor resolution: %f", sensorList[i]->getResolution());
        //     ALOGV(" sensor power: %f", sensorList[i]->getPowerUsage());
        //     ALOGV(" sensor minDelay: %d", sensorList[i]->getMinDelay());
        // }

        mSensorEventQueue = mgr.createEventQueue();
        if (mSensorEventQueue == NULL) {
            ALOGE("createEventQueue returned NULL");
            ret = NO_INIT;
            goto out;
        }

        mLooper = new Looper(false);
        mLooper->addFd(mSensorEventQueue->getFd(), 0, ALOOPER_EVENT_INPUT,
                       sensor_events_listener, this);
        
        if (mSensorLooperThread.get() == NULL) {
            mSensorLooperThread = new SensorLooperThread(mLooper.get());
        }
        if (mSensorLooperThread.get() == NULL) {
            ALOGE("Could't create sensor looper thread");
            ret = NO_MEMORY;
            goto out;
        }

        ret = mSensorLooperThread->run("Sensor looper thread", PRIORITY_URGENT_DISPLAY);
        if (ret == INVALID_OPERATION) {
            ALOGE("thread already running");
        } else if (ret != NO_ERROR) {
            ALOGE("could't run thread");
            goto out;
        }

    out:
        return ret;
    }

    void SensorListener::enableSensor(sensor_type_t type) {
        const Sensor* sensor;
        SensorManager& mgr(SensorManager::getInstance());

        AutoMutex lock(mLock);

        if ((type & SENSOR_ORIENTATION) && !(sensorsEnabled & SENSOR_ORIENTATION)) {
            sensor = mgr.getDefaultSensor(Sensor::TYPE_ACCELEROMETER);
            ALOGV("%s:name: %s, type: %d, handle: %d",__FUNCTION__,
                  sensor->getName().string(), sensor->getType(), sensor->getHandle());
            mSensorEventQueue->enableSensor(sensor->getHandle(), 200000);
            sensorsEnabled |= SENSOR_ORIENTATION;
            mRegisterHandle = sensor->getHandle();
        }
    }

    void SensorListener::disableSensor(sensor_type_t type) {
        const Sensor *sensor;

        SensorManager& mgr(SensorManager::getInstance());

        AutoMutex lock(mLock);

        if ((type & SENSOR_ORIENTATION) && (sensorsEnabled & SENSOR_ORIENTATION)) {
            sensor = mgr.getDefaultSensor(Sensor::TYPE_ACCELEROMETER);
            ALOGV("%s: name: %s, type: %d, handle: %d",
                  __FUNCTION__,
                  sensor->getName().string(), sensor->getType(), sensor->getHandle());
            mSensorEventQueue->disableSensor(sensor->getHandle());
            sensorsEnabled &= ~SENSOR_ORIENTATION;
            mRegisterHandle = -1;
        }
    }

    int SensorListener::getDefaultDisplayRotation(void) {
	//forest_gun
        int rotation = ROTATION_0;//SurfaceComposerClient::getDisplayOrientation(DisplayID(0));
        switch (rotation) {
        case ROTATION_0:
            return 0;
        case ROTATION_90:
            return 90;
        case ROTATION_180:
            return 180;
        case ROTATION_270:
            return 270;
        }
        return 0;
    }

    void SensorListener::setOrientation(int orient) {

        if (orient == ORIENTATION_UNKNOWN) {
            return;
        }

        bool changeOrientation = false;
        if (mCurOrientation == 0) {
            changeOrientation = true;
        } else {
            int dist = abs(orient - mCurOrientation);
            dist = (dist > (360 - dist)) ? (360 - dist) : dist;
            changeOrientation = (dist >= (45 + 5)); // 5 is hysteresis
        }

        if (changeOrientation) {
            mCurOrientation = ((orient + 45) / 90 * 90) % 360;
        }

        // ALOGV("%s: sensor orientation: %d", __FUNCTION__, orient);
        // ALOGV("%s: displayOrientation: %d", __FUNCTION__, displayOrientation);
        // ALOGV("%s: cur orientation: %d", __FUNCTION__, mCurOrientation);

        int orientationCompensation = (mCurOrientation + getDefaultDisplayRotation()) % 360;
        if (mOrientationCompensation != orientationCompensation) {
            mOrientationCompensation = orientationCompensation;
            ALOGV("%s: mOrientationCompensation: %d", __FUNCTION__, mOrientationCompensation);
        }
    }
}
