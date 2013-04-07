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

#ifndef __CAMERA_FACE_DETECT_H_
#define __CAMERA_FACE_DETECT_H_

#include <utils/misc.h>
#include <time.h>
#include <ui/Rect.h>
#include <utils/Vector.h>
#include "CameraDeviceCommon.h"

extern "C" {
#include <fd_emb_sdk.h>
}

#define MAX(x, y) (x)>(y) ? (x) : (y)

namespace android {

    struct face_detect_param {
        btk_HFaceFinder fd;
        btk_HSDK sdk;
        btk_HDCR dcr;
        int w;
        int h;
        int maxFaces;
    };

    struct FaceData {
        float confidence;
        float midpointx;
        float midpointy;
        float eyedist;
    };

    class CameraFaceDetect {

    protected:
        CameraFaceDetect();
    public:
        static Mutex sLock;
        static CameraFaceDetect* sInstance;
    public:
        static CameraFaceDetect* getInstance();
    public:
        virtual ~CameraFaceDetect();
    public:
        int initialize(int w, int h, int maxFaces=1);
        int detect(uint16_t* imge);
        void get_face(Rect* r, int index);
        float get_confidence(void);
        void deInitialize(void);


        float getRightEyeX(void) {
            return mRightEyeX;
        }

        float getRightEyeY(void) {
            return mRightEyeY;
        }

        float getLeftEyeX(void) {
            return mLeftEyeX;
        }

        float getLeftEyeY(void) {
            return mLeftEyeY;
        }


    private:
        void getFaceData(btk_HDCR hdcr, FaceData* fdata);

    private:
        face_detect_param mparam;
        float mConfidence;
        float mRightEyeX;
        float mRightEyeY;
        float mLeftEyeX;
        float mLeftEyeY;
    };
};

#endif
