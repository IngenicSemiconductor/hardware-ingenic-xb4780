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

#define LOG_TAG "CameraFaceDetect"
//#define LOG_NDEBUG 0
#include "CameraFaceDetect.h"

namespace android {

    Mutex CameraFaceDetect::sLock;
    CameraFaceDetect* CameraFaceDetect::sInstance = NULL;

    CameraFaceDetect* CameraFaceDetect::getInstance() {
        ALOGV("%s", __FUNCTION__);
        Mutex::Autolock _l(sLock);
        CameraFaceDetect* instance = sInstance;
        if (instance == NULL) {
            instance = new CameraFaceDetect();
            sInstance = instance;
        }
        return instance;
    }

    CameraFaceDetect::CameraFaceDetect() {
        memset(&mparam, 0, sizeof(struct face_detect_param));
        mLeftEyeX = 0;
        mLeftEyeY = 0;
        mRightEyeX = 0;
        mRightEyeY = 0;
    }

    CameraFaceDetect::~CameraFaceDetect() {
        deInitialize();
    }

    int CameraFaceDetect::initialize(int w, int h, int maxFaces) {
        const char* root = getenv("ANDROID_ROOT");
        String8 path(root);
        path.appendPath("usr/share/bmd/RFFstd_501.bmd");

        const int MAX_FILE_SIZE = 65536;
        void* initData = calloc(1, MAX_FILE_SIZE);
        int filedesc = open(path.string(), O_RDONLY);
        int initDataSize = read(filedesc, initData, MAX_FILE_SIZE);
        close(filedesc);

        btk_HSDK sdk = NULL;
        btk_SDKCreateParam sdkParam = btk_SDK_defaultParam();
        sdkParam.fpMalloc = malloc;
        sdkParam.fpFree = free;
        sdkParam.maxImageWidth = w;
        sdkParam.maxImageHeight = h;

        mparam.w = w;
        mparam.h = h;
        mparam.maxFaces = maxFaces;

        btk_Status status = btk_SDK_create(&sdkParam, &sdk);
        if (status != btk_STATUS_OK) {
            ALOGE("%s: line (%d), btk_SDK_create error, init fail",__FUNCTION__, __LINE__);
            return NO_INIT;
        }

        btk_HDCR dcr = NULL;
        btk_DCRCreateParam dcrParam = btk_DCR_defaultParam();
        btk_DCR_create(sdk, &dcrParam, &dcr);

        btk_HFaceFinder fd = NULL;
        btk_FaceFinderCreateParam fdParam = btk_FaceFinder_defaultParam();
        fdParam.pModuleParam = initData;
        fdParam.moduleParamSize = initDataSize;
        fdParam.maxDetectableFaces = maxFaces;
        status = btk_FaceFinder_create(sdk, &fdParam, &fd);
        btk_FaceFinder_setRange(fd, 20, w/2);

        if (status != btk_STATUS_OK) {
            ALOGE("%s: line (%d), error , btk_FaceFinder_create init fail", __FUNCTION__, __LINE__);
            return NO_INIT;
        }

        free(initData);

        mparam.fd = fd;
        mparam.sdk = sdk;
        mparam.dcr = dcr;
        return NO_ERROR;
    }

    void CameraFaceDetect::deInitialize(void) {

        btk_FaceFinder_close(mparam.fd);

        btk_DCR_close(mparam.dcr);

        btk_SDK_close(mparam.sdk);
    }

    int CameraFaceDetect::detect(uint16_t* imge) {

        btk_HDCR hdcr = mparam.dcr;
        btk_HFaceFinder hfd = mparam.fd;
        u32 width = mparam.w;
        u32 height = mparam.h;

        uint8_t* bwbuffer = (uint8_t*)calloc(1, width*height);
        uint8_t* dst = (uint8_t*)bwbuffer;

        uint16_t const* src = imge;
        int wpr = width;
        for (u32 y=0; y < height; y++) {
            for (u32 x=0; x<width; x++) {
                uint16_t rgb = src[x];
                int r = rgb >> 11;
                int g2 = (rgb >> 5) & 0x3F;
                int b = rgb & 0x1F;
                int L = (r<<1) + (g2<<1) + (g2>>1) + b;
                *dst++ = L;
            }
            src += wpr;
        }

        btk_DCR_assignGrayByteImage(hdcr, bwbuffer, width, height);

        int numberOfFaces = 0;
        if (btk_FaceFinder_putDCR(hfd, hdcr) == btk_STATUS_OK) {
            numberOfFaces = btk_FaceFinder_faces(hfd);
        } else {
            ALOGE("ERROR: Return 0 faces because error exists in btk_FaceFinder_putDCR.\n");
        }
        
        if (bwbuffer != NULL) {
            free(bwbuffer);
            bwbuffer = NULL;
        }

        return numberOfFaces;
    }

    float CameraFaceDetect::get_confidence(void) {
        return mConfidence;
    }

    void CameraFaceDetect::get_face(Rect* r, int index) {

        btk_HDCR hdcr = mparam.dcr;
        btk_HFaceFinder hfd = mparam.fd;

        FaceData faceData;
        btk_FaceFinder_getDCR(hfd, hdcr);
        getFaceData(hdcr, &faceData);

        float rx = faceData.eyedist * 2.0;
        float ry = rx;
        float aspect = (float)((float)(mparam.w) / (float)(mparam.h));
        if (aspect != -1.0) {
            if (aspect > 1) {
                rx = ry * aspect;
            } else {
                ry = rx / aspect;
            }
        }

        r->left = (faceData.midpointx - rx) < 0 ? 0 : (faceData.midpointx - rx);
        r->top = (faceData.midpointy - ry) < 0 ? 0 : (faceData.midpointy - ry);
        r->right = (faceData.midpointx + rx) > mparam.w ? mparam.w : (faceData.midpointx + rx);
        r->bottom = (faceData.midpointy + ry) > mparam.h ? mparam.h : (faceData.midpointy + ry); 

        if (aspect != -1.0) {
            if (r->width() / r->height() > aspect) {
                float w = r->height() * aspect;
                r->left = (r->left + r->right - w) * 0.5f;
                r->right = r->left + w;
            } else {
                float h = r->width() / aspect;
                r->top = (r->top + r->bottom - h) * 0.5f;
                r->bottom = r->top + h;
            }
        }

        r->left /= mparam.w;
        r->right /= mparam.w;
        r->top /= mparam.h;
        r->bottom /= mparam.h;
    }

    void CameraFaceDetect::getFaceData(btk_HDCR hdcr, FaceData* fdata) {
        btk_Node leftEye, rightEye;

        btk_DCR_getNode(hdcr, 0, &leftEye);
        btk_DCR_getNode(hdcr, 1, &rightEye);

        fdata->eyedist = (float)(rightEye.x - leftEye.x) / (1<<16);
        fdata->midpointx = (float)(rightEye.x + leftEye.x) / (1 << 17);
        fdata->midpointy = (float)(rightEye.y + leftEye.y) / (1 << 17);
        fdata->confidence = (float)btk_DCR_confidence(hdcr) / (1 << 24);
        mConfidence = fdata->confidence;
        mRightEyeX = rightEye.x;
        mRightEyeY = rightEye.y;
        mLeftEyeX = leftEye.x;
        mLeftEyeY = leftEye.y;
    }
};
