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

#ifndef CAMERA_COLORCONVERT_H_
#define CAMERA_COLORCONVERT_H_

#include "CameraDeviceCommon.h"

namespace android {

    class CameraColorConvert {

    public:

        CameraColorConvert ();
        virtual ~CameraColorConvert ();

    public:

        void convert_yuv420p_to_rgb565(CameraYUVMeta *yuvMeta, uint8_t *dstAttr);

        void cimyu420b_to_ipuyuv420b(CameraYUVMeta* yuvMeta);

        void cimvyuy_to_tile420(uint8_t* src_data,int srcwidth, int srcheight,
                                uint8_t* dest,int start_mbrow, int mbrow_nums);


        void tile420_to_rgb565(CameraYUVMeta* yuvMeta, uint8_t* dst);

        void YUV422P_To_RGB24_init();

        void cimyuv420b_to_tile420(CameraYUVMeta* yuvMeta);

        void cimyuv420b_to_tile420(CameraYUVMeta* yuvMeta,uint8_t* dest_frame);

        void cimyuv420b_to_yuv420p(CameraYUVMeta* yuvMeta, uint8_t* dstAddr);

        void yuyv_to_rgb24 (uint8_t *pyuv, int pyuvstride, uint8_t *prgb,
                            int prgbstride, int width, int height);

        void yuyv_to_rgb32 (uint8_t *pyuv, int pyuvstride, uint8_t *prgb,
                            int prgbstride, int width, int height);

        void yuyv_to_bgr24 (uint8_t *pyuv, int pyuvstride, uint8_t *pbgr, 
                            int pbgrstride, int width, int height);

        void yuyv_to_bgr32 (uint8_t *pyuv, int pyuvstride, uint8_t *pbgr, 
                            int pbgrstride, int width, int height);

        void yuyv_to_rgb565 (uint8_t *pyuv, int pyuvstride, uint8_t *prgb,
                             int prgbstride, int width, int height);

        void yuv420tile_to_yuv420sp(CameraYUVMeta* yuvMeta, uint8_t* dest);

        void yuyv_to_yuv420sp (uint8_t* src_frame , uint8_t* dst_frame , int width ,
                               int height);


        void yuyv_to_yvu422p(uint8_t *dst,int dstStride, int dstHeight, 
                             uint8_t *src, int srcStride, int width, int height);  

        void yuyv_to_yvu420p(uint8_t *dst,int dstStride, int dstHeight,
                             uint8_t *src, int srcStride, int width, int height);


        void yuyv_to_yvu420sp(uint8_t *dst,int dstStride, int dstHeight, 
                              uint8_t *src, int srcStride, int width, int height);

        void tile420_to_yuv420p(CameraYUVMeta* yuvMeta, uint8_t* dest);

        void yuyv_to_yuv420p(uint8_t *dst,int dstStride, int dstHeight, 
                             uint8_t *src, int srcStride, int width, int height);

        void uyvy_to_yuyv (uint8_t *dst,int dstStride, 
                           uint8_t *src, int srcStride, int width, int height);

        void yvyu_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, 
                           int srcStride, int width, int height);

        void yyuv_to_yuyv (uint8_t *dst,int dstStride, 
                           uint8_t *src, int srcStride, int width, int height);

        void yuv420_to_yuyv (uint8_t *dst,int dstStride, 
                             uint8_t *src, int width, int height);

        void yvu420_to_yuyv (uint8_t *dst,int dstStride, 
                             uint8_t *src, int width, int height);

        void nv12_to_yuyv (uint8_t *dst,int dstStride, 
                           uint8_t *src, int width, int height);

        void nv21_to_yuyv (uint8_t *dst,int dstStride, 
                           uint8_t *src, int width, int height);

        void nv16_to_yuyv (uint8_t *dst,int dstStride, 
                           uint8_t *src, int width, int height);

        void grey_to_yuyv (uint8_t *dst,int dstStride, 
                           uint8_t *src, int srcStride, int width, int height);

        void y16_to_yuyv (uint8_t *dst,int dstStride, 
                          uint8_t *src, int srcStride, int width, int height);

        void y41p_to_yuyv (uint8_t *dst,int dstStride, 
                           uint8_t *src, int width, int height);

        void rgb_to_yuyv(uint8_t *pyuv, int dstStride, uint8_t *prgb, 
                         int srcStride, int width, int height);

        void bgr_to_yuyv(uint8_t *pyuv, int dstStride, uint8_t *pbgr, 
                         int srcStride, int width, int height);

        void nv61_to_yuyv (uint8_t *dst,int dstStride, 
                           uint8_t *src, int width, int height);

        void yuyv_mirror (uint8_t* src_frame , int width , int height);

        void yuyv_upturn (uint8_t* src_frame , int width , int height);

        void yuyv_negative (uint8_t* src_frame , int width , int height);

        void yuyv_monochrome (uint8_t* src_frame , int width , int height);

        void yuyv_pieces (uint8_t* src_frame , int width , int height ,
                          int piece_size);

        void yvu422sp_to_yuyv (uint8_t *src_frame , uint8_t *dst_frame , int width ,
                               int height);

        void yuv422sp_to_yuv420p(uint8_t* dest, uint8_t* src_frame, int width, int height);

        void yuv422sp_to_yuv420sp(uint8_t* dest, uint8_t* src_frame, int width, int height);

        void yvu420sp_to_yuyv (uint8_t* src_frame , uint8_t* dst_frame , int width ,
                               int height);

        void yuyv_to_yuv422sp (uint8_t* src_frame , uint8_t* dst_frame , int width ,
                               int height);

        void yuv420b_64u_64v_to_rgb565(CameraYUVMeta* yuvMeta, uint8_t* dst,
                                       int rgbwidth, int rgbheight, int rgbstride, int destFmt);

        void yuv420p_to_tile420(CameraYUVMeta* yuvMeta, char *yuv420t);

        void yuv420p_to_yuv420sp(uint8_t* src_frame, uint8_t* dest_frame,
                                 int width, int height);

        void yuv420p_to_yuv422sp(uint8_t* src_frame, uint8_t* dest_frame,
                                 int width, int height);


        void yuv420p_to_rgb565 (uint8_t* src_frame , uint8_t* dst_frame , int width ,
                                int height);

        void yuv420sp_to_yuv420p(uint8_t* src_frame, uint8_t* dst_frame,
                                 int width, int height);

        void yuv420sp_to_rgb565 (uint8_t* src_frame , uint8_t* dst_frame , int width ,
                                 int height);


        void yuv420sp_to_argb8888 (uint8_t* src_frame , uint8_t* dst_frame ,
                                   int width , int height);

        class ColorConvertSMPThread : public Thread {
        public:
            ColorConvertSMPThread(CameraColorConvert * ccc){
                mCameraColorConvert = ccc;
            }

            ~ColorConvertSMPThread(){
            }

            void startthread(){
                mTask_todo=0;
                mTask_done=0;
                run ("ColorConvertSMPThread", ANDROID_PRIORITY_URGENT_DISPLAY, 0);
            }

            void stopthread(){
                requestExit();
                {
                    Mutex::Autolock _l(converter_lock);
                    converter_guest_condition.signal();
                }
                requestExitAndWait();
            }

            bool threadLoop(){
                {
                    Mutex::Autolock _l(converter_lock);
                    if(!mTask_todo)
                        converter_guest_condition.wait(converter_lock);
                    mTask_todo=0;
                    if(exitPending())
                        return false;
                }
                mCameraColorConvert->cimvyuy_to_tile420((uint8_t*)mYuvMeta->yAddr,
                                                        mYuvMeta->width,
                                                        mYuvMeta->height,
                                                        mDestaddr,
                                                        mStart_row,
                                                        mRow_nums);
                {
                    Mutex::Autolock _l(converter_lock);
                    mTask_done=1;
                    converter_host_condition.signal();
                }
                return true;
            }

            void start_guest(){
                Mutex::Autolock _l(converter_lock);
                mTask_todo=1;
                converter_guest_condition.signal();
            }
            void wait_guest(){
                Mutex::Autolock _l(converter_lock);
                if(!mTask_done)
                    converter_host_condition.wait(converter_lock);
                mTask_done=0;
            }
            void SetConverterParameters(CameraYUVMeta* yuvMeta, uint8_t* dest, int start_row, int row_nums){
                Mutex::Autolock _l(converter_lock);
                mYuvMeta = yuvMeta;
                mDestaddr = dest;
                mStart_row = start_row;
                mRow_nums = row_nums;
            }

        private:
            CameraColorConvert * mCameraColorConvert;
            CameraYUVMeta* mYuvMeta;
            uint8_t* mDestaddr;
            int mStart_row;
            int mRow_nums;
            int mTask_todo;
            int mTask_done;
            mutable Mutex converter_lock;
            Condition converter_host_condition;
            Condition converter_guest_condition;

        };

        sp<ColorConvertSMPThread> mCC_SMPThread;
    private:

        void initClip (void);

    private:

        const signed kClipMin;
        const signed kClipMax;
        int mtmp_uv_size;

        uint8_t *mClip;
        uint8_t* mtmp_uv;
        uint8_t* mtmp_sweap;
        uint8_t* msrc;
        const int csY_coeff_16;
        const int csU_blue_16;
        const int csU_green_16;
        const int csV_green_16;
        const int csV_red_16;
        unsigned char _color_table[256 * 3];
        const unsigned char* color_table;

        int Ym_tableEx[256];
        int Um_blue_tableEx[256];
        int Um_green_tableEx[256];
        int Vm_green_tableEx[256];
        int Vm_red_tableEx[256];
    };

};

#endif /* CAMERACOLORCONVERT_H_ */
