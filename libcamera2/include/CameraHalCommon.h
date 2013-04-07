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

#ifndef __CAMERA_HAL_COMMON_H__
#define __CAMERA_HAL_COMMON_H__
//#define LOG_NDEBUG 0

#include "CameraCompressor.h"
#ifdef CAMERA_SUPPORT_VIDEOSNAPSHORT
#include "CameraCompressorHW.h"
#endif

#include "SensorListener.h"

namespace android {

    class CameraHalCommon : public RefBase
    {

    public:

        CameraHalCommon(){};

        virtual ~CameraHalCommon(){};

    public:
  
        virtual void update_device(CameraDeviceCommon* device) = 0;

        virtual int module_open(const hw_module_t* module, const char* id, hw_device_t** device) = 0;

        virtual bool getModuleInit() = 0;

        virtual int get_number_cameras(void) = 0;

        virtual int get_cameras_info(int camera_id, struct camera_info* info) = 0;   
    };

#define SIGNAL_THREAD_TERMINATE   (1<<0)
#define SIGNAL_THREAD_PAUSE       (1<<1)
#define SIGNAL_THREAD_COMMON_LAST (1<<3)
#define SIG_WAITING_TICK          (5000)

    class SignalDrivenThread :public Thread {
    public:
        SignalDrivenThread()
            :Thread(false) {
            ALOGV("%s",__FUNCTION__);
            m_processingSignal = 0;
            m_receivedSignal = 0;
            m_pendingSignal = 0;
            m_isTerminated = false;
        }

        SignalDrivenThread(const char* name, int32_t priority, size_t stack)
            :Thread(false) {
            ALOGV("%s: name: %s, priority: %d, stack: %d",__FUNCTION__,
                     name?name:"null",priority, stack);
            m_processingSignal = 0;
            m_receivedSignal = 0;
            m_pendingSignal = 0;
            m_isTerminated = false;
            run(name, priority, stack);
            return;
        }

        virtual ~SignalDrivenThread() {
            ALOGV("%s",__FUNCTION__);
            return;
        }

        status_t SetSignal(uint32_t signal) {
            ALOGV("%s: Setting Signal: %x",__FUNCTION__, signal);

            AutoMutex lock(m_signal_lock);
            ALOGV(" set: %d -- pre: %x",signal, m_receivedSignal);
            if (m_receivedSignal & signal) {
                m_pendingSignal |= signal;
            } else {
                m_receivedSignal |= signal;
            }
            m_signal_condition.signal();
            return NO_ERROR;
        }

        uint32_t GetProcessingSignal(void) {
            ALOGV("%s: signal: %x",__FUNCTION__, m_processingSignal);
            AutoMutex lock(m_signal_lock);
            return m_processingSignal;
        }

        void ClearProcessingSignal (uint32_t signal) {
            AutoMutex lock(m_signal_lock);
            m_processingSignal &= (~signal);
        }

        void Start(const char* name, int32_t priority, size_t stack) {
            ALOGV("%s: name: %s, priority: %d, stack: %d",__FUNCTION__,
                     name?name:"null", priority, stack);
            run(name, priority, stack);
        }

        bool IsTerminated(void) {
            AutoMutex lock(m_signal_lock);
            return m_isTerminated;
        }

        status_t readyToRunInternal(void) {
            ALOGV("%s",__FUNCTION__);
            return NO_ERROR;
        }

        virtual void threadFuntionInternal(void) = 0;

    private:

        uint32_t m_processingSignal;
        uint32_t m_receivedSignal;
        uint32_t m_pendingSignal;
        bool m_isTerminated;

        mutable Mutex m_signal_lock;
        Condition m_signal_condition;

    private:

        status_t readyToRun() {
            ALOGV("%s",__FUNCTION__);
            return readyToRunInternal();
        }

        bool threadLoop() {
            {
                AutoMutex lock(m_signal_lock);
                ALOGV("%s: wait signal",__FUNCTION__);
                while (!m_receivedSignal) {
                    m_signal_condition.wait(m_signal_lock);
                }
                m_processingSignal = m_receivedSignal;
                m_receivedSignal = m_pendingSignal;
                m_pendingSignal = 0;
            }

            ALOGV("%s: get signal: %x",__FUNCTION__, m_processingSignal);
            if (m_processingSignal & SIGNAL_THREAD_TERMINATE) {
                ALOGV("%s: thread termination",__FUNCTION__);
                AutoMutex lock(m_signal_lock);
                m_isTerminated = true;
                return false;
            } else if (m_processingSignal & SIGNAL_THREAD_PAUSE) {
                ALOGV("%s: thread pause",__FUNCTION__);
                return true;
            }

            if (m_isTerminated) {
                m_isTerminated = false;
            }

            threadFuntionInternal();
            return true;
        }
    };

};//end namespace

#endif
