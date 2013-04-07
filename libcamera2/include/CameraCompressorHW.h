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

#ifndef ANDROID_HARDWARE_CAMERAJPEG_HARDWARE_HW
#define ANDROID_HARDWARE_CAMERAJPEG_HARDWARE_HW

#include "CameraDeviceCommon.h"

namespace android {

    typedef struct compress_params_hw{
        uint8_t* pictureYUV420_y;    //need to get in getPrameters()
        uint8_t* pictureYUV420_c;    //need to get in getPrameters()

        int pictureWidth;
        int pictureHeight;
        int pictureQuality;  //0-85 LOW 85-95 MEDIUMS 95-100 HIGH 

        int thumbnailWidth;
        int thumbnailHeight;
        int thumbnailQuality;  //0-85 LOW 85-95 MEDIUMS 95-100 HIGH

        int format;

        unsigned char* jpeg_out;
        int* jpeg_size;

        unsigned char* th_jpeg_out;
        int* th_jpeg_size;

        unsigned int tlb_addr;//need to get in getPrameters()

        camera_request_memory requiredMem;
    }compress_params_hw_t;

    typedef compress_params_hw_t* compress_params_hw_ptr;

    class CameraCompressorHW {

    private:
        compress_params_hw_t mcparamsHW;

    public :
        CameraCompressorHW();
        virtual ~CameraCompressorHW(){};
        int setPrameters(compress_params_hw_ptr hw_cinfo);
        int hw_compress_to_jpeg();
        void yuv422i_to_yuv420_block(unsigned char *dsty,unsigned char *dstc,char *inyuv,int w,int h);
        void camera_mem_free(struct camera_buffer* buf);
        int camera_mem_alloc(struct camera_buffer* buf,int size,int nr);
        void rgb565_to_jpeg(unsigned char* dest_img, int *dest_size, unsigned char* rgb,
                                            int width, int height,int quality);

    private:
        int getpictureYuv420Data( unsigned char* yuv_y, unsigned char* yuv_c);
        int getthumbnailYuv420Data(unsigned char* th_y, unsigned char* th_c);
        int put_image_jpeg(unsigned char* yuv_y, unsigned char* yuv_c, 
                           unsigned char* bsa, unsigned int* reginfo, int w, int h,
                           int quality, unsigned char* jpeg_out, int *size, unsigned int tlb_addr);
    };
};


#endif
