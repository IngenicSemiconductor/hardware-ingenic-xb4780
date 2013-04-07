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

#ifndef ANDROID_HARDWARE_CAMERAJPEG_HARDWARE_H
#define ANDROID_HARDWARE_CAMERAJPEG_HARDWARE_H

#ifdef CAMERA_VERSION1
#include "JZCameraParameters.h"
#endif
#ifdef CAMERA_VERSION2
#include "JZCameraParameters2.h"
#endif
#include <YuvToJpegEncoder.h>

namespace android {

    typedef struct compress_params{
        uint8_t* src;
        int pictureWidth;
        int pictureHeight;
        int pictureQuality;
        int thumbnailWidth;
        int thumbnailHeight;
        int thumbnailQuality;
        int format;
        int jpegSize;
        camera_request_memory requiredMem;
    }compress_params_t;

    class CameraCompressor {

    protected:

        uint8_t* mSrc;
        int mPictureWidth;
        int mPictureHeight;
        int mPictureQuality;
        int mThumbnailWidth;
        int mThumbnailHeight;
        int mThumbnailQuality;
        int mFormat;
        camera_request_memory mRequiredMem;
        SkDynamicMemoryWStream mStream;
        YuvToJpegEncoder* mEncoder;
        int mStrides[2];

    private:
          
        void yuyv_mirror (uint8_t* src_frame , int width , int height)
        {
            int h = 0;
            int w = 0;
            int sizeline = width * 2; /* 2 bytes per pixel*/
            uint8_t*pframe;
            pframe = src_frame;
            uint8_t line[sizeline - 1];
            for (h = 0; h < height; h++) {
                for (w = sizeline - 1; w > 0; w = w - 4) {
                    line[w - 1] = *pframe++;
                    line[w - 2] = *pframe++;
                    line[w - 3] = *pframe++;
                    line[w] = *pframe++;
                }
                memcpy(src_frame + (h * sizeline), line, sizeline);
            }
        }


        void yuyv_upturn (uint8_t* src_frame , int width , int height)
        {
            int h = 0;
            int sizeline = width * 2;
            uint8_t* line1[sizeline - 1];
            uint8_t* line2[sizeline - 1];
            for (h = 0; h < height / 2; h++) {
                memcpy(line1, src_frame + h * sizeline, sizeline);
                memcpy(line2, src_frame + (height - 1 - h) * sizeline, sizeline);
                memcpy(src_frame + h * sizeline, line2, sizeline);
                memcpy(src_frame + (height - 1 - h) * sizeline, line1, sizeline);
            }
        }



        status_t compressRawImage(int width, int height, int quality);


        void getCompressedImage(void* buff)
        {
            mStream.copyTo(buff);
        }


        size_t getCompressedSize() const
        {
            return mStream.getOffset();
        }
     
        void resetSkstream(void)
        {
            mStream.reset();
        }
 
    public:

        CameraCompressor(compress_params_t* yuvImage, bool mirror,int rot);

        virtual ~CameraCompressor()
        {
            if (NULL != mEncoder)
                {
                    delete mEncoder;
                    mEncoder = NULL;
                }
        }

        status_t compress_to_jpeg(ExifElementsTable* exif,camera_memory_t** jpegMem);

    };

};

#endif
