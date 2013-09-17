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

#define LOG_TAG "CameraColorConvert"
//#define LOG_NDEBUG 0
#include "CameraColorConvert.h"
#include <jzsoc/jzmedia.h>

#define CLIP(value) (uint8_t)(((value)>0xFF)?0xff:(((value)<0)?0:(value)))
#define i_pref(hint,base,offset)        \
({ __asm__ __volatile__("pref %0,%2(%1)"::"i"(hint),"r"(base),"i"(offset):"memory");})

namespace android {

    CameraColorConvert::CameraColorConvert ()
        :kClipMin(-278),
         kClipMax(535),
         mtmp_uv_size(0),
         mtmp_uv(NULL),
         msrc(NULL),
         csY_coeff_16(1.164383 * (1 << 16)),
         csU_blue_16(2.017232 * (1 << 16)),
         csU_green_16((-0.391762) * (1 << 16)),
         csV_green_16((-0.812968) * (1 << 16)),
         csV_red_16(1.596027 * (1 << 16)){

        initClip();
        YUV422P_To_RGB24_init();
        mtmp_uv_size = 2*1024*1024;
        mtmp_uv = (uint8_t*)malloc(mtmp_uv_size);
        color_table = &_color_table[256];
        
        mCC_SMPThread = new ColorConvertSMPThread(this);
    }

    CameraColorConvert::~CameraColorConvert ()
    {
        if (mClip != NULL) {
            delete[] mClip;
            mClip = NULL;
        }
        if (mtmp_uv != NULL) {
            free(mtmp_uv);
            mtmp_uv = NULL;
        }
        if (msrc != NULL) {
            free(msrc);
            msrc = NULL;
        }
    }

    /*------------------------------- Color space conversions --------------------*/


    /* rrrr rggg gggb bbbb */

    static inline uint16_t  make565(int red, int green, int blue)
    {
        return (uint16_t)( ((red   << 8) & 0xf800) |
                           ((green << 3) & 0x07e0) |
                           ((blue  >> 3) & 0x001f) );
    }

#define FIX1P8(x) ((int)((x) * (1<<8)))

    static inline int clip(int x) {
        if (x > 255)
            x = 255;
        if (x < 0)
            x = 0;
        return x;
    }

    void CameraColorConvert::cimyu420b_to_ipuyuv420b(CameraYUVMeta* yuvMeta) {

        if (yuvMeta->yAddr == 0) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }

        int y_size = yuvMeta->width * yuvMeta->height;
        int image_size = y_size * 12 / 8;
        int32_t* tmpdest = (int32_t*)(yuvMeta->yAddr + y_size);
        int32_t* u = (int32_t*)(yuvMeta->yAddr + image_size);
        int32_t* v = u + 16;
        int32_t* last_ptr = (int32_t*)(yuvMeta->yAddr + y_size*2);

        if ((mtmp_uv == NULL) || (y_size/2 > mtmp_uv_size)) {
            ALOGE("%s: mtmp_uv is null, uv_num = %d, mtmp_uv_size = %d", 
                  __FUNCTION__, y_size/2, mtmp_uv_size);
            return;
        }

        int32_t* dest = (int32_t*)mtmp_uv;
        while (1) {
            for (int i = 0; i < 8; ++i) {
                *dest++ = *u++;
                *dest++ = *u++;
                *dest++ = *v++;
                *dest++ = *v++;
            }
            if ((int)v == (int)last_ptr)
                break;
            u += 16;
            v += 16;
        }
        dest = (int32_t*)mtmp_uv;

        tmpdest = (int32_t*)(((int)tmpdest + 4096 - 1) & ~(4096 -1));
        int uv_stride = ((yuvMeta->width*8) + (2048-1)) & (~(2048-1));
        int uv_line_len = yuvMeta->width*8;
        int32_t* tmp_last_ptr = (int32_t*)(mtmp_uv + y_size/2);

        while (1) {
            if ((int)tmpdest >= (int)last_ptr)
                break;
            if ((int)dest >= (int)tmp_last_ptr)
                break;
            memcpy((int32_t*)tmpdest, (int32_t*)dest, uv_line_len/4);
            tmpdest += (uv_stride/4);
            dest += (uv_line_len/4);
        }
    }

    void CameraColorConvert::cimvyuy_to_tile420(uint8_t* src_data,int srcwidth, int srcheight,
                                                uint8_t* dest,int start_mbrow, int mbrow_nums) {

        if (src_data == 0) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }
        if ((srcwidth % 16) || (srcheight % 16)){
            ALOGE("%s: srcwidth is not >=16 aligned",__FUNCTION__);
            return;
        }

        if(((int)dest)%32){
            ALOGE("%s: dest not 32 aligned, optimization may cause error",__FUNCTION__);
            return;
        }

        int width = srcwidth;
        int height = srcheight;
        int y_size = width * height;
        unsigned char * dest_y = dest-4+(start_mbrow*width*16);
        unsigned char * dest_u = dest-4 + y_size + (start_mbrow*width*8);
        unsigned char * src = (unsigned char *)(src_data-2*width + (start_mbrow*width*16*2));//-2*width for mxu loop 

#if 0
        FILE * hw_rlt_fp = fopen("/data/yuv422","w+");

        int actual=fwrite(src+2*width,1,width * height*2,hw_rlt_fp);
        LOGE("actual write size=0x%x, xr16=0x%x",actual,S32M2I(xr16));
        *(volatile int *)0x80000001 = 0;
#endif

        //cimvyuy = 0xVY1UY0
        for(int mbrow=0; mbrow<mbrow_nums; mbrow++){
            for(int mbcol=0; mbcol<(width>>4); mbcol++){
                unsigned char* src_temp= src;
                //per loop handle two rows, odd rows uv will be not used
                for (int i = 0; i < 8; i++) {

                    i_pref(30, dest_y, 4);
                    if(!(i&1))
                        i_pref(30, dest_u, 4);

                    //even row, uv will be used
                    S32LDIV(xr1,src_temp,width,1);
                    S32LDD(xr2,src_temp,4);
                    S32LDD(xr3,src_temp,8);
                    S32LDD(xr4,src_temp,12);
                    S32LDD(xr5,src_temp,16);
                    S32LDD(xr6,src_temp,20);
                    S32LDD(xr7,src_temp,24);
                    S32LDD(xr8,src_temp,28);

                    S32SFL(xr2,xr2,xr1,xr1,1);//xr2=0xV1 U1 V0 U0, xr1=0xY3 Y2 Y1 Y0
                    S32SFL(xr4,xr4,xr3,xr3,1);//xr4=0xV3 U3 V2 U2, xr3=0xY7 Y6 Y5 Y4
                    S32SFL(xr4,xr4,xr2,xr2,1);//xr4=0xV3 V2 V1 V0, xr2=0xU3 U2 U1 U0
                    S32SFL(xr6,xr6,xr5,xr5,1);//xr6=0xV5 U5 V4 U4, xr5=0xY11 Y10 Y9 Y8
                    S32SFL(xr8,xr8,xr7,xr7,1);//xr8=0xV7 U7 V6 U6, xr7=0xY15 Y14 Y13 Y12
                    S32SFL(xr8,xr8,xr6,xr6,1);//xr8=0xV7 V6 V5 V4, xr6=0xU7 U6 U5 U4

                    S32SDI(xr1,dest_y,4);
                    S32SDI(xr3,dest_y,4);
                    S32SDI(xr5,dest_y,4);
                    S32SDI(xr7,dest_y,4);
                    S32SDI(xr2,dest_u,4);//u0-u3
                    S32SDI(xr6,dest_u,4);//u4-u7
                    S32SDI(xr4,dest_u,4);//v0-v3
                    S32SDI(xr8,dest_u,4);//v4-v7

                    //odd row, uv will be discarded
                    S32LDIV(xr1,src_temp,width,1);
                    S32LDD(xr2,src_temp,4);
                    S32LDD(xr3,src_temp,8);
                    S32LDD(xr4,src_temp,12);
                    S32LDD(xr5,src_temp,16);
                    S32LDD(xr6,src_temp,20);
                    S32LDD(xr7,src_temp,24);
                    S32LDD(xr8,src_temp,28);

                    S32SFL(xr2,xr2,xr1,xr1,1);//xr2=0xV1 U1 V0 U0, xr1=0xY3 Y2 Y1 Y0
                    S32SFL(xr4,xr4,xr3,xr3,1);//xr4=0xV3 U3 V2 U2, xr3=0xY7 Y6 Y5 Y4
                    S32SFL(xr6,xr6,xr5,xr5,1);//xr6=0xV5 U5 V4 U4, xr5=0xY11 Y10 Y9 Y8
                    S32SFL(xr8,xr8,xr7,xr7,1);//xr8=0xV7 U7 V6 U6, xr7=0xY15 Y14 Y13 Y12

                    S32SDI(xr1,dest_y,4);
                    S32SDI(xr3,dest_y,4);
                    S32SDI(xr5,dest_y,4);
                    S32SDI(xr7,dest_y,4);
                }
                src+=32;//src proceed to next MB
            }
            src += 15*2*width;//src proceed to next mbrow
        }
    }

    void CameraColorConvert::cimyuv420b_to_tile420(CameraYUVMeta* yuvMeta) {

        if (yuvMeta->yAddr == 0) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }

        int y_size = yuvMeta->width * yuvMeta->height;
        int image_size = y_size * 12 / 8;
        int32_t* dest = (int32_t*)(yuvMeta->yAddr + y_size);
        int32_t* u = (int32_t*)(yuvMeta->yAddr + image_size);
        int32_t* v = u + 16;
        int32_t* last_ptr = (int32_t*)(yuvMeta->yAddr + y_size*2);

        while (1) {
            for (int i = 0; i < 8; ++i) {
                *dest++ = *u++;
                *dest++ = *u++;
                *dest++ = *v++;
                *dest++ = *v++;
            }
            if ((int)v == (int)last_ptr)
                break;
            u += 16;
            v += 16;
        }
    }

    void CameraColorConvert::cimyuv420b_to_tile420(CameraYUVMeta* yuvMeta,uint8_t* dest_frame) {

        if (yuvMeta->yAddr == 0) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }

        int y_size = yuvMeta->width * yuvMeta->height;
        int image_size = y_size * 12 / 8;
        int32_t* dest = (int32_t*)(dest_frame+y_size);
        int32_t* u = (int32_t*)(yuvMeta->yAddr + image_size);
        int32_t* v = u + 16;
        int32_t* last_ptr = (int32_t*)(yuvMeta->yAddr + y_size*2);

        memcpy((int32_t*)dest_frame, (int32_t*)(yuvMeta->yAddr), y_size/4);

        while (1) {
            for (int i = 0; i < 8; ++i) {
                *dest++ = *u++;
                *dest++ = *u++;
                *dest++ = *v++;
                *dest++ = *v++;
            }
            if ((int)v == (int)last_ptr)
                break;
            u += 16;
            v += 16;
        }
    }

    /* 64 u 64 v -> yuv420p */
    void CameraColorConvert::cimyuv420b_to_yuv420p(CameraYUVMeta* yuvMeta, uint8_t* dstAddr) {

        if (yuvMeta->yAddr == 0) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }

        char *y_p = (char*)dstAddr;
        int width = yuvMeta->width;
        int height = yuvMeta->height;
        unsigned int y_p_size = width*height;
        char *u_p = y_p+y_p_size;
        unsigned int u_p_size = y_p_size / 4;
        char *v_p = u_p+u_p_size;
        unsigned int v_p_size = u_p_size;

        char *y_t = (char*)(yuvMeta->yAddr);
        unsigned int y_t_size = y_p_size;
        char *uv_t = y_t + y_t_size;
        unsigned int uv_t_size = u_p_size+v_p_size;

        int i_w, j_h;
        int y_w_mcu = width / 16;
        int y_h_mcu = height / 16;

        int u_w_mcu = y_w_mcu;
        int u_h_mcu = y_h_mcu;

        int v_w_mcu = u_w_mcu;
        int v_h_mcu = u_h_mcu;

        int i_mcu;

        char *y_mcu = y_p;
        char *u_mcu = u_p;
        char *v_mcu = v_p;
        char *y_tmp = y_p;

        char *y_t_tmp = y_t;
        for(j_h = 0; j_h < y_h_mcu; j_h++)
            {
                y_mcu = y_p + j_h*y_w_mcu*256;
                for(i_w = 0; i_w < y_w_mcu; i_w++)
                    {
                        y_tmp = y_mcu;
                        for(i_mcu = 0; i_mcu < 16; i_mcu++)
                            {
                                memcpy(y_tmp, y_t_tmp, 16);
                                y_t_tmp += 16;
                                y_tmp += width;
                            }
                        y_mcu += 16;
                    }
            }

        char *u_tmp = u_p;
        char *v_tmp = v_p;
        char *u_uv_t_tmp = uv_t;
        char *v_uv_t_tmp = uv_t+64;
        for(j_h = 0; j_h < u_h_mcu; j_h++)
            {
                u_mcu = u_p + j_h*u_w_mcu*64;
                v_mcu = v_p + j_h*v_w_mcu*64;
                for(i_w = 0; i_w < u_w_mcu; i_w++)
                    {
                        u_tmp = u_mcu;
                        v_tmp = v_mcu;
                        for(i_mcu = 0; i_mcu < 8; i_mcu++)
                            {
                                memcpy(u_tmp, u_uv_t_tmp, 8);
                                memcpy(v_tmp, v_uv_t_tmp, 8);

                                u_uv_t_tmp += 8;
                                v_uv_t_tmp += 8;

                                u_tmp += width / 2;
                                v_tmp += width / 2;
                            }
                        u_uv_t_tmp += 64;
                        v_uv_t_tmp += 64;

                        u_mcu += 8;
                        v_mcu += 8;
                    }
            }
    }

inline long border_color(long color) {
    if (color > 255)
        return 255;
    else if (color < 0)
        return 0;
    else
        return color;
}

//init tables used to speed up color transform
void CameraColorConvert::YUV422P_To_RGB24_init() {
    int i;
    for (i = 0; i < 256 * 3; ++i)
        _color_table[i] = border_color(i - 256);
    for (i = 0; i < 256; ++i) {
        Ym_tableEx[i] = (csY_coeff_16 * (i - 16)) >> 16;
        Um_blue_tableEx[i] = (csU_blue_16 * (i - 128)) >> 16;
        Um_green_tableEx[i] = (csU_green_16 * (i - 128)) >> 16;
        Vm_green_tableEx[i] = (csV_green_16 * (i - 128)) >> 16;
        Vm_red_tableEx[i] = (csV_red_16 * (i - 128)) >> 16;
    }
}



#if 1
    /*
    yuv to rgb888
    R=1.164(Y-16)+1.596(V-128)
    G=1.164(Y-16)-0.391(U-128)-0.812(V-128)
    B=1.164(Y-16)+2.017(U-128)
    */
    void CameraColorConvert::tile420_to_rgb565(CameraYUVMeta* yuvMeta, uint8_t* dstAddr) {

        if (yuvMeta->yAddr == 0) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }
        int line = 0, col = 0;
        int y0 = 0, y1 = 0, u = 0, v = 0;
        int r0 = 0, g0 = 0, b0 = 0;
        int r1 = 0, g1 = 0, b1 = 0;
        const unsigned char *py = NULL, *pu = NULL, *pv = NULL;
        int width = yuvMeta->width;     //frame width
        int height = yuvMeta->height;   //frame height
        int ySize = width*height;       //space length of element y
        unsigned short* dst = (unsigned short*)dstAddr;
        if ((width % 2) != 0 || (height % 2) != 0){
            for( line = 0; line < ySize; line++){
                *dst++=0;
            }
            return;
        }
        py = (unsigned char*)(yuvMeta->yAddr);  //start address of element y
        pu = py + ySize;    //start address of element u
        pv = pu + 8;        //start address of element v

        //micro-block: 16*16 for element y, 8*8 for elements u&v
        int iLoop = 0;  //horizontal offset in a micro-block (0,1,2...,16)
        int jLoop = 0;  //number of micro-block in a frame's horizontal direction (0,1,2...,screen_width/16)
        int kLoop = 0;  //line number in a micre-block (0,1,2...,15)
        int dxy = 0;    //horizontal offset relative to dy
        int stride_y = width<<4;
        int stride_uv = width<<3;   //u&v_microblock: 8*8

        //variables used to transform between yuv and rgb
        int Ye0,Ye1,Ue_blue,Ue_green,Ve_green,Ve_red,UeVe_green;

        for (line = 0; line < height; line++) {
            for (col = 0; col < width; col += 2 ) {
                if ( iLoop == 16 ){ //means ends of a micro-block in horizontal direction,will jump to next one
                    jLoop++;
                    iLoop = 0;
                }
                dxy = iLoop + jLoop * 256;
                iLoop += 2;         //it will transform 2 pixels for a line in a loop

                y0 = *(py+dxy);
                y1 = *(py+dxy+1);
                u = *(pu+dxy/2);
                v = *(pv+dxy/2);
                Ye0 = Ym_tableEx[y0];
                Ye1 = Ym_tableEx[y1];
                Ue_blue = Um_blue_tableEx[u];
                Ue_green = Um_green_tableEx[u];
                Ve_green = Vm_green_tableEx[v];
                UeVe_green = Ue_green + Ve_green;
                Ve_red = Vm_red_tableEx[v];

                r0 = (int)color_table[(Ye0 + Ve_red)];
                r1 = (int)color_table[(Ye1 + Ve_red)];
                g0 = (int)color_table[(Ye0 + UeVe_green)];
                g1 = (int)color_table[(Ye1 + UeVe_green)];
                b0 = (int)color_table[(Ye0 + Ue_blue)];
                b1 = (int)color_table[(Ye1 + Ue_blue)];

                //before fill rgb data to dst,transform rgb888 to rgb565 first
                *dst++= (((unsigned short)r0>>3)<<11)
                    | (((unsigned short)g0>>2)<<5)
                    | (((unsigned short)b0>>3)<<0);

                *dst++= (((unsigned short)r1>>3)<<11)
                    | (((unsigned short)g1>>2)<<5)
                    | (((unsigned short)b1>>3)<<0);
            } //end for (col = 0; col < width; col += 2 )

            if ( kLoop > 0 && kLoop % 15 == 0 ) {
                py += stride_y + 16 - 256;
                pu += stride_uv + 16 - 128;
                pv = pu + 8;
                iLoop = 0; jLoop = 0; kLoop = 0;
            }
            else if ( kLoop & 1 ) {
                py += 16;
                pu += 16;
                pv = pu + 8;
                iLoop = 0; jLoop = 0; kLoop++;
            }
            else {
                py += 16;
                iLoop = 0; jLoop = 0;
                kLoop++;
            }
        } //end for (line = 0; line < height; line++)
    }

#else
    /* 8u 8v*/
    void CameraColorConvert::tile420_to_rgb565(CameraYUVMeta* yuvMeta, uint8_t* dstAddr) {

        if (yuvMeta->yAddr == 0) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }

        int line = 0, col = 0, linewidth = 0;
        int y = 0, u = 0, v = 0, yy = 0, vr = 0, ug = 0, vg = 0, ub = 0;
        int r = 0, g = 0, b = 0;
        const unsigned char *py = NULL, *pu = NULL, *pv = NULL;
        int width = yuvMeta->width;
        int height = yuvMeta->height;
        int ySize = width*height;
        unsigned short* dst = (unsigned short*)dstAddr;

        py = (unsigned char*)(yuvMeta->yAddr);
        pu = py + ySize;
        pv = pu + 8;

        int iLoop = 0, jLoop = 0, kLoop = 0, dxy = 0;
        int stride_y = width*16;
        int stride_uv = width*8;

        for (line = 0; line < height; line++) {
            for (col = 0; col < width; col++) {
                if ( iLoop > 0 && iLoop % 16 == 0 ) {
                    jLoop++;
                    iLoop = 0;
                    dxy = jLoop*256;
                    iLoop++;
                } else {
                    dxy = iLoop + jLoop * 256;
                    iLoop++;
                }

                y = *(py+dxy);
                yy = y << 8;
                u = *(pu+dxy/2) - 128;
                ug = 88 * u;
                ub = 454 * u;
                v = *(pv+dxy/2) - 128;
                vg = 183 * v;
                vr = 359 * v;

                r = (yy + vr) >> 8;
                g = (yy - ug - vg) >> 8;
                b = (yy + ub ) >> 8;

                if (r < 0) r = 0;
                if (r > 255) r = 255;
                if (g < 0) g = 0;
                if (g > 255) g = 255;
                if (b < 0) b = 0;
                if (b > 255) b = 255;
                *dst++ = (((unsigned short)r>>3)<<11) 
                    | (((unsigned short)g>>2)<<5)
                    | (((unsigned short)b>>3)<<0);
            } 

            if ( kLoop > 0 && kLoop % 15 == 0 ) {
                py += stride_y + 16 - 256;
                pu += stride_uv + 16 - 128;
                pv = pu + 8;
                iLoop = 0; jLoop = 0; kLoop = 0;
            }
            else if ( kLoop & 1 ) {
                py += 16;
                pu += 16;
                pv = pu + 8;
                iLoop = 0; jLoop = 0; kLoop++;
            }
            else {
                py += 16;
                iLoop = 0; jLoop = 0;
                kLoop++;
            }
        } 
    }

#endif
    static void yuyv_to_rgb565_line (uint8_t *pyuv, uint8_t *prgb, int width)
    {
        int l=0;
        int ln = width >> 1;
        uint16_t *p = (uint16_t *)prgb;

        for(l=0; l<ln; l++)
            {/*iterate every 4 bytes*/

                int u  = pyuv[1] - 128;
                int v  = pyuv[3] - 128;

                int ri = (                       + FIX1P8(1.402)   * v) >> 8;
                int gi = ( - FIX1P8(0.34414) * u - FIX1P8(0.71414) * v) >> 8;
                int bi = ( + FIX1P8(1.772)   * u                      ) >> 8;

                /* standart: r = y0 + 1.402 (v-128) */
                /* logitech: r = y0 + 1.370705 (v-128) */
                /* standart: g = y0 - 0.34414 (u-128) - 0.71414 (v-128)*/
                /* logitech: g = y0 - 0.337633 (u-128)- 0.698001 (v-128)*/
                /* standart: b = y0 + 1.772 (u-128) */
                /* logitech: b = y0 + 1.732446 (u-128) */

                int y0 = pyuv[0];
                *p++ = make565(
                               clip(y0 + ri),
                               clip(y0 + gi),
                               clip(y0 + bi)
                               );
                int y1 = pyuv[2];
                *p++ = make565(
                               clip(y1 + ri),
                               clip(y1 + gi),
                               clip(y1 + bi)
                               );
                pyuv += 4;
            }
    }

    /* regular yuv (YUYV) to rgb565*/
    void CameraColorConvert::yuyv_to_rgb565 (uint8_t *pyuv, int pyuvstride, uint8_t *prgb,int prgbstride, int width, int height)
    {
        int h=0;
        ALOGV("%s: yuv stride = %d, rgbstride = %d , size = %dx%d", 
        __FUNCTION__,pyuvstride, prgbstride, width, height);

        if (pyuv == NULL) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }

        for(h=0;h<height;h++)
            {
                yuyv_to_rgb565_line (pyuv,prgb,width);
                pyuv += pyuvstride;
                prgb += prgbstride;
            }
    }


    static void yuyv_to_rgb24_line (uint8_t *pyuv, uint8_t *prgb, int width)
    {
        int l=0;
        int ln = width >> 1;

        for(l=0; l<ln; l++)
            {/*iterate every 4 bytes*/

                int u  = pyuv[1] - 128;
                int v  = pyuv[3] - 128;

                int ri = (                       + FIX1P8(1.402)   * v) >> 8;
                int gi = ( - FIX1P8(0.34414) * u - FIX1P8(0.71414) * v) >> 8;
                int bi = ( + FIX1P8(1.772)   * u                      ) >> 8;

                /* standart: r = y0 + 1.402 (v-128) */
                /* logitech: r = y0 + 1.370705 (v-128) */
                /* standart: g = y0 - 0.34414 (u-128) - 0.71414 (v-128)*/
                /* logitech: g = y0 - 0.337633 (u-128)- 0.698001 (v-128)*/
                /* standart: b = y0 + 1.772 (u-128) */
                /* logitech: b = y0 + 1.732446 (u-128) */

                int y0 = pyuv[0];
                *prgb++ = clip(y0 + ri);
                *prgb++ = clip(y0 + gi);
                *prgb++ = clip(y0 + bi);

                int y1 = pyuv[2];
                *prgb++ = clip(y1 + ri);
                *prgb++ = clip(y1 + gi);
                *prgb++ = clip(y1 + bi);

                pyuv += 4;
            }
    }

    /* regular yuv (YUYV) to rgb24*/
    void CameraColorConvert::yuyv_to_rgb24 (uint8_t *pyuv, int pyuvstride, uint8_t *prgb,int prgbstride, int width, int height)
    {
        int h=0;
        ALOGV("%s: yuv stride = %d, rgbstride = %d , size = %dx%d", __FUNCTION__,pyuvstride, prgbstride, width, height);

        if (pyuv == NULL) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }

        for(h=0;h<height;h++)
            {
                yuyv_to_rgb24_line (pyuv,prgb,width);
                pyuv += pyuvstride;
                prgb += prgbstride;
            }
    }

    static void yuyv_to_rgb32_line (uint8_t *pyuv, uint8_t *prgb, int width)
    {
        int l=0;
        int ln = width >> 1;

        for(l=0; l<ln; l++)
            {/*iterate every 4 bytes*/

                int u  = pyuv[1] - 128;
                int v  = pyuv[3] - 128;

                int ri = (                       + FIX1P8(1.402)   * v) >> 8;
                int gi = ( - FIX1P8(0.34414) * u - FIX1P8(0.71414) * v) >> 8;
                int bi = ( + FIX1P8(1.772)   * u                      ) >> 8;

                /* standart: r = y0 + 1.402 (v-128) */
                /* logitech: r = y0 + 1.370705 (v-128) */
                /* standart: g = y0 - 0.34414 (u-128) - 0.71414 (v-128)*/
                /* logitech: g = y0 - 0.337633 (u-128)- 0.698001 (v-128)*/
                /* standart: b = y0 + 1.772 (u-128) */
                /* logitech: b = y0 + 1.732446 (u-128) */

                int y0 = pyuv[0];
                *prgb++ = clip(y0 + ri);
                *prgb++ = clip(y0 + gi);
                *prgb++ = clip(y0 + bi);
                prgb++;

                int y1 = pyuv[2];
                *prgb++ = clip(y1 + ri);
                *prgb++ = clip(y1 + gi);
                *prgb++ = clip(y1 + bi);
                prgb++;

                pyuv += 4;
            }
    }

    /* regular yuv (YUYV) to rgb32*/
    void CameraColorConvert::yuyv_to_rgb32 (uint8_t *pyuv, int pyuvstride, uint8_t *prgb,int prgbstride, int width, int height)
    {
        int h=0;
        ALOGV("%s: yuv stride = %d, rgbstride = %d , size = %dx%d", __FUNCTION__,pyuvstride, prgbstride, width, height);

        if (pyuv == NULL) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }

        for(h=0;h<height;h++)
            {
                yuyv_to_rgb32_line (pyuv,prgb,width);
                pyuv += pyuvstride;
                prgb += prgbstride;
            }
    }


    static  void yuyv_to_bgr24_line (uint8_t *pyuv, uint8_t *pbgr, int width)
    {
        int l=0;
        int ln = width >> 1;
        for(l=0;l<ln;l++)
            {/*iterate every 4 bytes*/
                int u  = pyuv[1] - 128;
                int v  = pyuv[3] - 128;

                int ri = (                       + FIX1P8(1.402)   * v) >> 8;
                int gi = ( - FIX1P8(0.34414) * u - FIX1P8(0.71414) * v) >> 8;
                int bi = ( + FIX1P8(1.772)   * u                      ) >> 8;

                /* standart: r = y0 + 1.402 (v-128) */
                /* logitech: r = y0 + 1.370705 (v-128) */
                /* standart: g = y0 - 0.34414 (u-128) - 0.71414 (v-128)*/
                /* logitech: g = y0 - 0.337633 (u-128)- 0.698001 (v-128)*/
                /* standart: b = y0 + 1.772 (u-128) */
                /* logitech: b = y0 + 1.732446 (u-128) */

                int y0 = pyuv[0];
                *pbgr++ = clip(y0 + ri);
                *pbgr++ = clip(y0 + gi);
                *pbgr++ = clip(y0 + bi);
                pbgr++;

                int y1 = pyuv[2];
                *pbgr++ = clip(y1 + ri);
                *pbgr++ = clip(y1 + gi);
                *pbgr++ = clip(y1 + bi);
                pbgr++;

                pyuv += 4;
            }
    }

    /* used for rgb video (fourcc="RGB ")           */
    /* lines are on correct order                   */
    void CameraColorConvert::yuyv_to_bgr24 (uint8_t *pyuv, int pyuvstride, uint8_t *pbgr, int pbgrstride, int width, int height)
    {
        int h=0;
        ALOGV("%s: yuv stride = %d, rgbstride = %d , size = %dx%d", __FUNCTION__, pyuvstride, pbgrstride, width, height);

        if (pyuv == NULL) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }

        for(h=0;h<height;h++)
            {
                yuyv_to_bgr24_line (pyuv,pbgr,width);
                pyuv += pyuvstride;
                pbgr += pbgrstride;
            }
    }

    static void yuyv_to_bgr32_line (uint8_t *pyuv, uint8_t *pbgr, int width)
    {
        int l=0;
        int ln = width >> 1;
        for(l=0;l<ln;l++)
            {/*iterate every 4 bytes*/
                int u  = pyuv[1] - 128;
                int v  = pyuv[3] - 128;

                int ri = (                       + FIX1P8(1.402)   * v) >> 8;
                int gi = ( - FIX1P8(0.34414) * u - FIX1P8(0.71414) * v) >> 8;
                int bi = ( + FIX1P8(1.772)   * u                      ) >> 8;

                /* standart: r = y0 + 1.402 (v-128) */
                /* logitech: r = y0 + 1.370705 (v-128) */
                /* standart: g = y0 - 0.34414 (u-128) - 0.71414 (v-128)*/
                /* logitech: g = y0 - 0.337633 (u-128)- 0.698001 (v-128)*/
                /* standart: b = y0 + 1.772 (u-128) */
                /* logitech: b = y0 + 1.732446 (u-128) */

                int y0 = pyuv[0];
                *pbgr++ = clip(y0 + ri);
                *pbgr++ = clip(y0 + gi);
                *pbgr++ = clip(y0 + bi);
                pbgr++;

                int y1 = pyuv[2];
                *pbgr++ = clip(y1 + ri);
                *pbgr++ = clip(y1 + gi);
                *pbgr++ = clip(y1 + bi);
                pbgr++;

                pyuv += 4;
            }
    }

    /* used for rgb video (fourcc="RGB ")           */
    /* lines are on correct order                   */
    void CameraColorConvert::yuyv_to_bgr32 (uint8_t *pyuv, int pyuvstride, uint8_t *pbgr, int pbgrstride, int width, int height)
    {
        int h=0;
        ALOGV("%s: yuv stride = %d, rgbstride = %d , size = %dx%d", __FUNCTION__,pyuvstride, pbgrstride, width, height);

        if (pyuv == NULL) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }

        for(h=0;h<height;h++)
            {
                yuyv_to_bgr32_line (pyuv,pbgr,width);
                pyuv += pyuvstride;
                pbgr += pbgrstride;
            }
    }

    /* convert yuyv to YVU422P */
    /* This format assumes that the horizontal strides (luma and chroma) are multiple of 16 pixels */
    void CameraColorConvert::yuyv_to_yvu422p(uint8_t *dst,int dstStride, int dstHeight, uint8_t *src, int srcStride, int width, int height)
    {
        ALOGV("%s: dstStride = %d, dstHeight = %d, srcStride = %d, size = %dx%d", __FUNCTION__,dstStride, dstHeight, srcStride,
                 width, height);
        // Calculate the chroma plane stride
        int dstVUStride = ((dstStride >> 1) + 15) & (-16);

        // Start of Y plane
        uint8_t* dstY = dst;

        // Calculate start of U plane
        uint8_t* dstV = dst + dstStride * dstHeight;

        // Calculate start of V plane
        uint8_t* dstU = dstV + (dstVUStride * dstHeight);

        int h=0;
        int w=0;
        int dy  = dstStride - width;
        int dvu = dstVUStride - (width >> 1);
        int sw  = srcStride - (width<<1);
        for (h = 0; h<height; h ++) {
            for (w=0; w < width; w += 2) {
                *dstY++ = *src++;// Y0
                *dstU++ = *src++;// U
                *dstY++ = *src++;// Y1
                *dstV++ = *src++;// V
            }
            src  += sw;
            dstY += dy;
            dstU += dvu;
            dstV += dvu;
        }
    }


    /* convert yuyv to YVU420P */
    /* This format assumes that the horizontal strides (luma and chroma) are multiple of 16 pixels */
    void CameraColorConvert::yuyv_to_yvu420p(uint8_t *dst,int dstStride, int dstHeight, uint8_t *src, int srcStride, int width, int height)
    {
        ALOGV("%s: dstStride = %d, dstHeight = %d, srcStride = %d, size = %dx%d",
      __FUNCTION__, dstStride, dstHeight, srcStride,
                 width, height);

        // Calculate the chroma plane stride
        int dstVUStride = ((dstStride >> 1) + 15) & (-16);

        // Start of Y plane
        uint8_t* dstY = dst;

        // Calculate start of V plane
        uint8_t* dstV = dst + dstStride * dstHeight;

        // Calculate start of U plane
        uint8_t* dstU = dstV + (dstVUStride * dstHeight >> 1);

        int h=0;
        int w=0;
        int dy  = dstStride - width;
        int dvu = dstVUStride - (width >> 1);
        int sw  = srcStride - (width<<1);
        for (h = 0; h<height; h +=2) {
            for (w=0; w < width; w += 2) {
                *dstY++ = *src++;// Y0
                *dstU++ = (src[0] + src[srcStride]) >> 1;// U
                src++;
                *dstY++ = *src++;// Y1
                *dstV++ = (src[0] + src[srcStride]) >> 1;// V
                src++;
            }
            src   += sw;
            dstY  += dy;
            dstU += dvu;
            dstV += dvu;
            for (w=0; w < width; w += 2) {
                *dstY++  = *src;// Y0
                src += 2;
                *dstY++  = *src;// Y1
                src += 2;
            }
            src   += sw;
            dstY  += dy;
        }
    }

    /* convert yuyv to YVU420SP */
    void CameraColorConvert::yuyv_to_yvu420sp(uint8_t *dst,int dstStride, int dstHeight, uint8_t *src, int srcStride, int width, int height)
    {
        ALOGV("%s: dstStride = %d, dstHeight = %d, srcStride = %d, size = %dx%d", 
                 __FUNCTION__,dstStride, dstHeight, srcStride,
                 width, height);
        // Start of Y plane
        uint8_t* dstY = dst;

        // Calculate start of VU plane
        uint8_t* dstVU = dst + dstStride * dstHeight;

        int h=0;
        int w=0;
        int dyvu = dstStride - width;
        int sw   = srcStride - (width<<1);
        for (h = 0; h<height; h +=2) {
            for (w=0; w < width; w += 2) {
                *dstY++  = *src++;// Y0
                dstVU[1] = (src[0] + src[srcStride]) >> 1;// U
                src++;
                *dstY++  = *src++;// Y1
                dstVU[0] = (src[0] + src[srcStride]) >> 1;// V
                src++;
                dstVU+=2;
            }
            src   += sw;
            dstY  += dyvu;
            dstVU += dyvu;
            for (w=0; w < width; w += 2) {
                *dstY++  = *src;// Y0
                src += 2;
                *dstY++  = *src;// Y1
                src += 2;
            }
            src   += sw;
            dstY  += dyvu;
        }
    }
 
    void CameraColorConvert::tile420_to_yuv420p(CameraYUVMeta* yuvMeta, uint8_t* dest) {


        if (yuvMeta->yAddr == 0) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }

        char *y_p = (char*)dest;
        int width = yuvMeta->width;
        int height = yuvMeta->height;
        unsigned int y_p_size = width*height;
        char *u_p = y_p+y_p_size;
        unsigned int u_p_size = y_p_size>>2;
        char *v_p = u_p+u_p_size;
        unsigned int v_p_size = u_p_size;

        char *y_t = (char*)(yuvMeta->yAddr);
        unsigned int y_t_size = y_p_size;
        char *uv_t = y_t + y_t_size;
        unsigned int uv_t_size = u_p_size+v_p_size;

        int i_w, j_h;
        int y_w_mcu = width>>4;
        int y_h_mcu = height>>4;

        int u_w_mcu = y_w_mcu;
        int u_h_mcu = y_h_mcu;

        int v_w_mcu = u_w_mcu;
        int v_h_mcu = u_h_mcu;

        int i_mcu;

        char *y_mcu = y_p;
        char *u_mcu = u_p;
        char *v_mcu = v_p;
        char *y_tmp = y_p;

        char *y_t_tmp = y_t;
        for(j_h = 0; j_h < y_h_mcu; j_h++)
            {
                y_mcu = y_p + j_h*y_w_mcu*256;
                for(i_w = 0; i_w < y_w_mcu; i_w++)
                    {
                        y_tmp = y_mcu;
                        for(i_mcu = 0; i_mcu < 16; i_mcu++)
                            {
                                memcpy(y_tmp, y_t_tmp, 16);
                                y_t_tmp += 16;
                                y_tmp += width;
                            }
                        y_mcu += 16;
                    }
            }

        char *u_tmp = u_p;
        char *v_tmp = v_p;
        char *u_uv_t_tmp = uv_t;
        char *v_uv_t_tmp = uv_t+8;
        for(j_h = 0; j_h < u_h_mcu; j_h++)
            {
                u_mcu = u_p + j_h*u_w_mcu*64;
                v_mcu = v_p + j_h*v_w_mcu*64;
                for(i_w = 0; i_w < u_w_mcu; i_w++)
                    {
                        u_tmp = u_mcu;
                        v_tmp = v_mcu;
                        for(i_mcu = 0; i_mcu < 8; i_mcu++)
                            {
                                memcpy(u_tmp, u_uv_t_tmp, 8);
                                memcpy(v_tmp, v_uv_t_tmp, 8);

                                u_uv_t_tmp += 16;
                                v_uv_t_tmp += 16;

                                u_tmp += width / 2;
                                v_tmp += width / 2;
                            }

                        u_mcu += 8;
                        v_mcu += 8;
                    }
            }
    }

    /* This format assumes that the horizontal strides (luma and chroma) are multiple of 16 pixels */
    void CameraColorConvert::yuyv_to_yuv420p(uint8_t *dst,int dstStride, int dstHeight, 
                                             uint8_t *src, int srcStride, int width, int height)
    {
        ALOGV("%s: dstStride = %d, dstHeight = %d, srcStride = %d, size = %dx%d",
               __FUNCTION__, dstStride, dstHeight, srcStride,
                 width, height);
        // Calculate the chroma plane stride
        int dstUVStride = ((dstStride >> 1) + 15) & (-16);

        // Start of Y plane
        uint8_t* dstY = dst;

        // Calculate start of U plane
        uint8_t* dstU = dst + dstStride * dstHeight;

        // Calculate start of V plane
        uint8_t* dstV = dstU + (dstUVStride * dstHeight >> 1);

        int h=0;
        int w=0;
        int dy  = dstStride - width;
        int dvu = dstUVStride - (width >> 1);
        int sw  = srcStride - (width<<1);
        for (h = 0; h<height; h +=2) {
            for (w=0; w < width; w += 2) {
                *dstY++ = *src++;// Y0
                *dstU++ = (src[0] + src[srcStride]) >> 1;// U
                src++;
                *dstY++ = *src++;// Y1
                *dstV++ = (src[0] + src[srcStride]) >> 1;// V
                src++;
            }
            src   += sw;
            dstY  += dy;
            dstU += dvu;
            dstV += dvu;
            for (w=0; w < width; w += 2) {
                *dstY++  = *src;// Y0
                src += 2;
                *dstY++  = *src;// Y1
                src += 2;
            }
            src   += sw;
            dstY  += dy;
        }
    }


    /*convert uyvy (packed) to yuyv (packed)
     * args:
     *      dst: pointer to frame buffer (yuyv)
     *      dstStride: stride of framebuffer
     *      src: pointer to temp buffer containing uyvy packed data frame
     *      width: picture width
     *      height: picture height
     */
    void CameraColorConvert::uyvy_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, int srcStride, int width, int height)
    {
        uint8_t *ptmp = src;
        uint8_t *pfmb = dst;
        int h=0;
        int w=0;
        int dw = dstStride - (width << 1);
        int sw = srcStride - (width << 1);

        for(h=0;h<height;h++)
            {
                for(w=0;w<width;w+=2)
                    {
                        /* Y0 */
                        *pfmb++ = ptmp[1];
                        /* U */
                        *pfmb++ = ptmp[0];
                        /* Y1 */
                        *pfmb++ = ptmp[3];
                        /* V */
                        *pfmb++ = ptmp[2];

                        ptmp += 4;
                    }
                pfmb += dw; // Correct for stride
                ptmp += sw;
            }
    }

    /*convert yvyu (packed) to yuyv (packed)
     * args:
     *      dst: pointer to frame buffer (yuyv)
     *      dstStride: stride of framebuffer
     *      src: pointer to temp buffer containing yvyu packed data frame
     *      width: picture width
     *      height: picture height
     */
    void CameraColorConvert::yvyu_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, int srcStride, int width, int height)
    {
        uint8_t *ptmp=NULL;
        uint8_t *pfmb=NULL;
        ptmp = src;
        pfmb = dst;

        int h=0;
        int w=0;
        int dw = dstStride - (width << 1);
        int sw = srcStride - (width << 1);

        for(h=0;h<height;h++)
            {
                for(w=0;w<width;w+=2)
                    {
                        /* Y0 */
                        *pfmb++ = ptmp[0];
                        /* U */
                        *pfmb++ = ptmp[3];
                        /* Y1 */
                        *pfmb++ = ptmp[2];
                        /* V */
                        *pfmb++ = ptmp[1];

                        ptmp += 4;
                    }
                pfmb += dw; // Correct for stride
                ptmp += sw;
            }
    }


    /*convert yyuv (packed) to yuyv (packed)
     * args:
     *      dst: pointer to frame buffer (yuyv)
     *      dstStride: stride of framebuffer
     *      src: pointer to temp buffer containing yyuv packed data frame
     *      width: picture width
     *      height: picture height
     */
    void CameraColorConvert::yyuv_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, int srcStride, int width, int height)
    {
        uint8_t *ptmp=NULL;
        uint8_t *pfmb=NULL;
        ptmp = src;
        pfmb = dst;

        int h=0;
        int w=0;
        int dw = dstStride - (width << 1);
        int sw = srcStride - (width << 1);

        for(h=0;h<height;h++)
            {
                for(w=0;w<width;w+=2)
                    {
                        /* Y0 */
                        *pfmb++ = ptmp[0];
                        /* U */
                        *pfmb++ = ptmp[2];
                        /* Y1 */
                        *pfmb++ = ptmp[1];
                        /* V */
                        *pfmb++ = ptmp[3];

                        ptmp += 4;
                    }
                pfmb += dw; // Correct for stride
                ptmp += sw;
            }
    }


    /*convert yuv 420 planar (yu12) to yuv 422
     * args:
     *      dst: pointer to frame buffer (yuyv)
     *      dstStride: stride of framebuffer
     *      src: pointer to temp buffer containing yuv420 planar data frame
     *      width: picture width
     *      height: picture height
     */
    void CameraColorConvert::yuv420_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, int width, int height)
    {
        uint8_t *py;
        uint8_t *pu;
        uint8_t *pv;

        int linesize = width * 2;
        int uvlinesize = width / 2;
        int offsety=0;
        int offsetuv=0;
        int dw = dstStride - (width << 1);

        py=src;
        pu=py+(width*height);
        pv=pu+(width*height/4);

        int h=0;
        int w=0;

        int wy=0;
        int wuv=0;

        offsety = 0;
        offsetuv = 0;

        for(h=0;h<height;h+=2)
            {
                wy=0;
                wuv=0;

                for(w=0;w<linesize;w+=4)
                    {
                        /*y00*/
                        *dst++        = py[wy + offsety];
                        /*y10*/
                        dst[dstStride-1] = py[wy + offsety + width];

                        /*u0*/
                        uint8_t u0 = pu[wuv + offsetuv];
                        *dst++        = u0;
                        /*u0*/
                        dst[dstStride-1] = u0;

                        /*y01*/
                        *dst++        = py[(wy + 1) + offsety];
                        /*y11*/
                        dst[dstStride-1] = py[(wy + 1) + offsety + width];

                        /*v0*/
                        uint8_t u1 = pv[wuv + offsetuv];
                        *dst++        = u1;
                        /*v0*/
                        dst[dstStride-1] = u1;

                        wuv++;
                        wy+=2;
                    }

                dst += dstStride + dw;
                offsety  += width * 2;
                offsetuv += uvlinesize;
            }
    }

    /*convert yvu 420 planar (yv12) to yuv 422
     * args:
     *      dst: pointer to frame buffer (yuyv)
     *      dstStride: stride of framebuffer
     *      src: pointer to temp buffer containing yuv420 planar data frame
     *      width: picture width
     *      height: picture height
     */
    void CameraColorConvert::yvu420_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, int width, int height)
    {
        uint8_t *py;
        uint8_t *pv;
        uint8_t *pu;

        int linesize = width * 2;
        int uvlinesize = width / 2;
        int offsety=0;
        int offsetuv=0;
        int dw = dstStride - (width << 1);

        py=src;
        pv=py+(width*height);
        pu=pv+((width*height)/4);

        int h=0;
        int w=0;

        int wy=0;
        int wuv=0;

        offsety = 0;
        offsetuv = 0;

        for(h=0;h<height;h+=2)
            {
                wy=0;
                wuv=0;

                for(w=0;w<linesize;w+=4)
                    {
                        /*y00*/
                        *dst++ = py[wy + offsety];
                        /*y10*/
                        dst[dstStride-1] = py[wy + offsety + width];

                        /*u0*/
                        uint8_t u0 = pu[wuv + offsetuv];
                        *dst++ = u0;
                        /*u0*/
                        dst[dstStride-1] = u0;

                        /*y01*/
                        *dst++ = py[(wy + 1) + offsety];
                        /*y11*/
                        dst[dstStride-1] = py[(wy + 1) + offsety +  width];

                        /*v0*/
                        uint8_t v0 = pv[wuv + offsetuv];
                        *dst++ = v0;
                        /*v0*/
                        dst[dstStride-1] = v0;

                        wuv++;
                        wy+=2;
                    }
                dst += dstStride + dw;
                offsety += width * 2;
                offsetuv += uvlinesize;

            }
    }


    /*convert yuv 420 planar (uv interleaved) (nv12) to yuv 422
     * args:
     *      dst: pointer to frame buffer (yuyv)
     *      dstStride: stride of framebuffer
     *      src: pointer to temp buffer containing yuv420 (nv12) planar data frame
     *      width: picture width
     *      height: picture height
     */
    void CameraColorConvert::nv12_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, int width, int height)
    {
        uint8_t *py;
        uint8_t *puv;

        int linesize = width * 2;
        int offsety=0;
        int offsetuv=0;
        int dw = dstStride - (width << 1);

        py=src;
        puv=py+(width*height);

        int h=0;
        int w=0;

        int wy=0;
        int wuv=0;

        for(h=0;h<height;h+=2)
            {
                wy=0;
                wuv=0;

                for(w=0;w<linesize;w+=4)
                    {
                        /*y00*/
                        *dst++ = py[wy + offsety];
                        /*y10*/
                        dst[dstStride-1] = py[wy + offsety + width];

                        /*u0*/
                        uint8_t u0 = puv[wuv + offsetuv];
                        *dst++ = u0;
                        /*u0*/
                        dst[dstStride-1] = u0;

                        /*y01*/
                        *dst++ = py[(wy + 1) + offsety];
                        /*y11*/
                        dst[dstStride-1] = py[(wy + 1) + offsety + width];

                        /*v0*/
                        uint8_t v0 = puv[(wuv + 1) + offsetuv];
                        *dst++ = v0;
                        /*v0*/
                        dst[dstStride-1] = v0;

                        wuv+=2;
                        wy+=2;
                    }

                dst += dstStride + dw;
                offsety += width * 2;
                offsetuv +=  width;
            }
    }


    /*convert yuv 420 planar (vu interleaved) (nv21) to yuv 422
     * args:
     *      dst: pointer to frame buffer (yuyv)
     *      dstStride: stride of framebuffer
     *      src: pointer to temp buffer containing yuv420 (nv21) planar data frame
     *      width: picture width
     *      height: picture height
     */
    void CameraColorConvert::nv21_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, int width, int height)
    {
        uint8_t *py;
        uint8_t *puv;

        int linesize = width * 2;
        int offsety=0;
        int offsetuv=0;
        int dw = dstStride - (width << 1);

        py=src;
        puv=py+(width*height);

        int h=0;
        int w=0;

        int wy=0;
        int wuv=0;

        for(h=0;h<height;h+=2)
            {
                wy=0;
                wuv=0;

                for(w=0;w<linesize;w+=4)
                    {
                        /*y00*/
                        *dst++ = py[wy + offsety];
                        /*y10*/
                        dst[dstStride-1] = py[wy + offsety + width];

                        /*u0*/
                        uint8_t u0 = puv[(wuv + 1) + offsetuv];
                        *dst++ = u0;
                        /*u0*/
                        dst[dstStride-1] = u0;

                        /*y01*/
                        *dst++ = py[(wy + 1) + offsety];

                        /*y11*/
                        dst[dstStride-1] = py[(wy + 1) + offsety + width];

                        /*v0*/
                        uint8_t v0 = puv[wuv + offsetuv];
                        *dst++ = v0;
                        /*v0*/
                        dst[dstStride-1] = v0;

                        wuv+=2;
                        wy+=2;
                    }

                dst += dstStride + dw;
                offsety += width * 2;
                offsetuv += width;

            }
    }

    /*convert yuv 422 planar (uv interleaved) (nv16) to yuv 422
     * args:
     *      dst: pointer to frame buffer (yuyv)
     *      dstStride: stride of framebuffer
     *      src: pointer to temp buffer containing yuv422 (nv16) planar data frame
     *      width: picture width
     *      height: picture height
     */
    void CameraColorConvert::nv16_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, int width, int height)
    {
        uint8_t *py;
        uint8_t *puv;

        int linesize = width * 2;
        int offsety=0;
        int offsetuv=0;
        int dw = dstStride - (width << 1);

        py=src;
        puv=py+(width*height);

        int h=0;
        int w=0;

        int wy=0;
        int wuv=0;

        for(h=0;h<height;h++)
            {
                wy=0;
                wuv=0;

                for(w=0;w<linesize;w+=4)
                    {
                        /*y00*/
                        *dst++ = py[wy + offsety];
                        /*u0*/
                        *dst++ = puv[wuv + offsetuv];
                        /*y01*/
                        *dst++ = py[(wy + 1) + offsety];
                        /*v0*/
                        *dst++ = puv[(wuv + 1) + offsetuv];

                        wuv+=2;
                        wy+=2;
                    }
                dst += dw;
                offsety += width;
                offsetuv += width;
            }
    }


    /*convert yuv 422 planar (vu interleaved) (nv61) to yuv 422
     * args:
     *      dst: pointer to frame buffer (yuyv)
     *      dstStride: stride of framebuffer
     *      src: pointer to temp buffer containing yuv422 (nv61) planar data frame
     *      width: picture width
     *      height: picture height
     */
    void CameraColorConvert::nv61_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, int width, int height)
    {
        uint8_t *py;
        uint8_t *puv;

        int linesize = width * 2;
        int offsety=0;
        int offsetuv=0;
        int dw = dstStride - (width << 1);

        py=src;
        puv=py+(width*height);

        int h=0;
        int w=0;

        int wy=0;
        int wuv=0;

        for(h=0;h<height;h++)
            {
                wy=0;
                wuv=0;
                for(w=0;w<linesize;w+=4)
                    {
                        /*y00*/
                        *dst++ = py[wy + offsety];
                        /*u0*/
                        *dst++ = puv[(wuv + 1) + offsetuv];
                        /*y01*/
                        *dst++ = py[(wy + 1) + offsety];
                        /*v0*/
                        *dst++ = puv[wuv + offsetuv];

                        wuv+=2;
                        wy+=2;
                    }
                dst += dw;
                offsety += width;
                offsetuv += width;

            }
    }

    /*convert yuv 411 packed (y41p) to yuv 422
     * args:
     *      dst: pointer to frame buffer (yuyv)
     *      dstStride: stride of framebuffer
     *      src: pointer to temp buffer containing y41p data frame
     *      width: picture width
     *      height: picture height
     */
    void CameraColorConvert::y41p_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, int width, int height)
    {
        int h=0;
        int w=0;
        int linesize = width * 3 /2;
        int offset = 0;
        int dw = dstStride - (width << 1);

        for(h=0;h<height;h++)
            {
                offset = linesize * h;
                for(w=0;w<linesize;w+=12)
                    {
                        *dst++=src[w+1 + offset]; //Y0
                        *dst++=src[w   + offset]; //U0
                        *dst++=src[w+3 + offset]; //Y1
                        *dst++=src[w+2 + offset]; //V0
                        *dst++=src[w+5 + offset]; //Y2
                        *dst++=src[w   + offset]; //U0
                        *dst++=src[w+7 + offset]; //Y3
                        *dst++=src[w+2 + offset]; //V0
                        *dst++=src[w+8 + offset]; //Y4
                        *dst++=src[w+4 + offset]; //U4
                        *dst++=src[w+9 + offset]; //Y5
                        *dst++=src[w+6 + offset]; //V4
                        *dst++=src[w+10+ offset]; //Y6
                        *dst++=src[w+4 + offset]; //U4
                        *dst++=src[w+11+ offset]; //Y7
                        *dst++=src[w+6 + offset]; //V4
                    }
                dst += dw;
            }
    }

    /*convert yuv mono (grey) to yuv 422
     * args:
     *      dst: pointer to frame buffer (yuyv)
     *      dstStride: stride of framebuffer
     *      src: pointer to temp buffer containing grey (y only) data frame
     *      width: picture width
     *      height: picture height
     */
    void CameraColorConvert::grey_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, int srcStride, int width, int height)
    {
        int h=0;
        int w=0;
        int dw = dstStride - (width << 1);
        int sw = srcStride - width;

        for(h=0;h<height;h++)
            {
                for(w=0;w<width;w++)
                    {
                        *dst++=*src++; //Y
                        *dst++=0x80;   //U or V
                    }
                dst += dw;
                src += sw;
            }
    }


    /*convert y16 (grey) to yuyv (packed)
     * args:
     *      dst: pointer to frame buffer (yuyv)
     *      dstStride: stride of framebuffer
     *      src: pointer to temp buffer containing y16 (grey) data frame
     *      width: picture width
     *      height: picture height
     */
    void CameraColorConvert::y16_to_yuyv (uint8_t *dst,int dstStride, uint8_t *src, int srcStride, int width, int height)
    {
        uint16_t *ptmp= (uint16_t *) src;

        int h=0;
        int w=0;
        int dw = dstStride - (width << 1);
        int sw = (srcStride>>1) - width;

        for(h=0;h<height;h++)
            {
                for(w=0;w<width;w+=2)
                    {
                        /* Y0 */
                        *dst++ = (uint8_t) (ptmp[0] & 0xFF00) >> 8;
                        /* U */
                        *dst++ = 0x7F;
                        /* Y1 */
                        *dst++ = (uint8_t) (ptmp[1] & 0xFF00) >> 8;
                        /* V */
                        *dst++ = 0x7F;

                        ptmp += 2;
                    }
                dst  += dw; // Correct for stride
                ptmp += sw;
            }
    }


    void CameraColorConvert::rgb_to_yuyv(uint8_t *pyuv, int dstStride, uint8_t *prgb, int srcStride, int width, int height)
    {

        int h;
        int dw = dstStride - (width << 1);
        for (h=0;h<height;h++) {
            int i=0;
            for(i=0;i<(width*3);i=i+6)
                {
                    /* y */
                    *pyuv++ =CLIP(0.299 * (prgb[i] - 128) + 0.587 * (prgb[i+1] - 128) + 0.114 * (prgb[i+2] - 128) + 128);
                    /* u */
                    *pyuv++ =CLIP(((- 0.147 * (prgb[i] - 128) - 0.289 * (prgb[i+1] - 128) + 0.436 * (prgb[i+2] - 128) + 128) +
                                   (- 0.147 * (prgb[i+3] - 128) - 0.289 * (prgb[i+4] - 128) + 0.436 * (prgb[i+5] - 128) + 128))/2);
                    /* y1 */
                    *pyuv++ =CLIP(0.299 * (prgb[i+3] - 128) + 0.587 * (prgb[i+4] - 128) + 0.114 * (prgb[i+5] - 128) + 128);
                    /* v*/
                    *pyuv++ =CLIP(((0.615 * (prgb[i] - 128) - 0.515 * (prgb[i+1] - 128) - 0.100 * (prgb[i+2] - 128) + 128) +
                                   (0.615 * (prgb[i+3] - 128) - 0.515 * (prgb[i+4] - 128) - 0.100 * (prgb[i+5] - 128) + 128))/2);
                }
            pyuv += dw;
            prgb += srcStride;
        }
    }

    void CameraColorConvert::bgr_to_yuyv(uint8_t *pyuv, int dstStride, uint8_t *pbgr, int srcStride, int width, int height)
    {
        int h;
        int dw = dstStride - (width << 1);
        for (h=0;h<height;h++) {
            int i=0;
            for(i=0;i<(width*3);i=i+6)
                {
                    /* y */
                    *pyuv++ =CLIP(0.299 * (pbgr[i+2] - 128) + 0.587 * (pbgr[i+1] - 128) + 0.114 * (pbgr[i] - 128) + 128);
                    /* u */
                    *pyuv++ =CLIP(((- 0.147 * (pbgr[i+2] - 128) - 0.289 * (pbgr[i+1] - 128) + 0.436 * (pbgr[i] - 128) + 128) +
                                   (- 0.147 * (pbgr[i+5] - 128) - 0.289 * (pbgr[i+4] - 128) + 0.436 * (pbgr[i+3] - 128) + 128))/2);
                    /* y1 */
                    *pyuv++ =CLIP(0.299 * (pbgr[i+5] - 128) + 0.587 * (pbgr[i+4] - 128) + 0.114 * (pbgr[i+3] - 128) + 128);
                    /* v*/
                    *pyuv++ =CLIP(((0.615 * (pbgr[i+2] - 128) - 0.515 * (pbgr[i+1] - 128) - 0.100 * (pbgr[i] - 128) + 128) +
                                   (0.615 * (pbgr[i+5] - 128) - 0.515 * (pbgr[i+4] - 128) - 0.100 * (pbgr[i+3] - 128) + 128))/2);
                }
            pyuv += dw;
            pbgr += srcStride;
        }
    }


    void CameraColorConvert::yuyv_mirror (uint8_t* src_frame , int width , int height)
    {
        ALOGV("%s: size = %dx%d", __FUNCTION__, width, height);
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

    void CameraColorConvert::yuyv_upturn (uint8_t* src_frame , int width , int height)
    {
        ALOGV("%s: size = %dx%d", __FUNCTION__, width, height);
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

    void CameraColorConvert::yuyv_negative (uint8_t* src_frame , int width , int height)
    {
        ALOGV("%s: size = %dx%d", __FUNCTION__, width, height);
        int size = width * height * 2;
        int i = 0;
        for (i = 0; i < size; i++)
            src_frame[i] = ~src_frame[i];
    }

    void CameraColorConvert::yuyv_monochrome (uint8_t* src_frame , int width , int height)
    {
        int size = width * height * 2;
        int i = 0;
        ALOGV("%s: size = %dx%d", __FUNCTION__, width, height);
        for (i = 0; i < size; i = i + 4) {
            src_frame[i + 1] = 0x80;
            src_frame[i + 3] = 0x80;
        }
    }

    void CameraColorConvert::yuyv_pieces (uint8_t* src_frame , int width , int height ,
                                          int piece_size = 16)
    {
        ALOGV("%s: size = %dx%d", __FUNCTION__, width, height);
        int numx = width / piece_size;
        int numy = height / piece_size;
        uint8_t* piece = (uint8_t*) malloc(piece_size * piece_size * 2);
        int i = 0, j = 0, row = 0, line = 0, column = 0, linep = 0, px = 0, py = 0;
        srand (time (NULL));int
                                rot = 0;

        for (j = 0; j < numy; j++) {
            row = j * piece_size;
            for (i = 0; i < numx; i++) {
                column = i * piece_size * 2;
                for (py = 0; py < piece_size; py++) {
                    linep = py * piece_size * 2;
                    line = (py + row) * width * 2;
                    for (px = 0; px < piece_size * 2; px++) {
                        piece[px + linep] = src_frame[(px + column) + line];
                    }
                }
                rot = rand() % 8;
                switch (rot) {
                case 0:
                    break;
                case 5:
                case 1:
                    yuyv_mirror(piece, piece_size, piece_size);
                    break;
                case 6:
                case 2:
                    yuyv_upturn(piece, piece_size, piece_size);
                    break;
                case 4:
                case 3:
                    yuyv_upturn(piece, piece_size, piece_size);
                    yuyv_mirror(piece, piece_size, piece_size);
                    break;
                default:
                    break;
                }

                for (py = 0; py < piece_size; py++) {
                    linep = py * piece_size * 2;
                    line = (py + row) * width * 2;
                    for (px = 0; px < piece_size * 2; px++) {
                        src_frame[(px + column) + line] = piece[px + linep];
                    }
                }
            }
        }
        free(piece);
        piece = NULL;
    }

    //HAL_PIXEL_FORMAT_YCrCb_420_SP
    void CameraColorConvert::yvu420sp_to_yuyv (uint8_t* src_frame , uint8_t* dst_frame ,
                                               int width , int height)
    {
        ALOGV("%s: size = %dx%d", __FUNCTION__, width, height);
        uint8_t* py;
        uint8_t* puv;

        int linesize = width * 2;
        int offset = 0;
        int offset1 = 0;
        int offsety = 0;
        int offsety1 = 0;
        int offsetuv = 0;

        py = src_frame;
        puv = py + (width * height);

        int h = 0;
        int w = 0;

        int wy = 0;
        int huv = 0;
        int wuv = 0;

        for (h = 0; h < height; h += 2) {
            wy = 0;
            wuv = 0;
            offset = h * linesize;
            offset1 = (h + 1) * linesize;
            offsety = h * width;
            offsety1 = (h + 1) * width;
            offsetuv = (huv * width);
            for (w = 0; w < linesize; w += 4) {
                /*y00*/
                dst_frame[w + offset] = py[wy + offsety];
                /*u0*/
                dst_frame[(w + 1) + offset] = puv[(wuv + 1) + offsetuv];
                /*y01*/
                dst_frame[(w + 2) + offset] = py[(wy + 1) + offsety];
                /*v0*/
                dst_frame[(w + 3) + offset] = puv[wuv + offsetuv];

                /*y10*/
                dst_frame[w + offset1] = py[wy + offsety1];
                /*u0*/
                dst_frame[(w + 1) + offset1] = puv[(wuv + 1) + offsetuv];
                /*y11*/
                dst_frame[(w + 2) + offset1] = py[(wy + 1) + offsety1];
                /*v0*/
                dst_frame[(w + 3) + offset1] = puv[wuv + offsetuv];

                wuv += 2;
                wy += 2;
            }
            huv++;
        }
    }

    //HAL_PIXEL_FORMAT_YCbCr_422_SP
    void CameraColorConvert::yvu422sp_to_yuyv (uint8_t *src_frame , uint8_t *dst_frame ,
                                               int width , int height)
    {
        ALOGV("%s: size = %dx%d", __FUNCTION__, width, height);
        uint8_t *py;
        uint8_t *puv;

        int linesize = width * 2;
        int offset = 0;
        int offsety = 0;
        int offsetuv = 0;

        py = src_frame;
        puv = py + (width * height);

        int h = 0;
        int w = 0;

        int wy = 0;
        int huv = 0;
        int wuv = 0;

        for (h = 0; h < height; h++) {
            wy = 0;
            wuv = 0;
            offset = h * linesize;
            offsety = h * width;
            offsetuv = huv * width;
            for (w = 0; w < linesize; w += 4) {
                /*y00*/
                dst_frame[w + offset] = py[wy + offsety];
                /*u0*/
                dst_frame[(w + 1) + offset] = puv[wuv + offsetuv];
                /*y01*/
                dst_frame[(w + 2) + offset] = py[(wy + 1) + offsety];
                /*v0*/
                dst_frame[(w + 3) + offset] = puv[(wuv + 1) + offsetuv];

                wuv += 2;
                wy += 2;
            }
            huv++;
        }
    }
     
    void CameraColorConvert::yuv422sp_to_yuv420sp(uint8_t* dest, uint8_t* src_frame, 
                                                  int width, int height) {
        int y_size = width*height;
        uint8_t* dest_u = dest + y_size;
        uint8_t* src_u = src_frame + y_size;
        uint8_t* last_ptr = src_u + (y_size>>1);

        while (1) {
            *(dest_u+1) = *src_u++; //u
            *dest_u = *src_u++;
            if ((int)src_u == (int)last_ptr)
                break;
            dest_u += 2;
        }
        memcpy((int32_t*)dest, (int32_t*)src_frame, y_size/4);
    }

    void CameraColorConvert::yuv422sp_to_yuv420p(uint8_t* dest, uint8_t* src_frame, 
                                                 int width, int height) {
        int y_size = width * height;
        uint8_t* src_u = src_frame + y_size;
        uint8_t* dest_u = dest + y_size;
        uint8_t* dest_v = dest + (y_size>>2);
        uint8_t* last_ptr = src_u + (y_size>>1);

        while (1) {
            *dest_u++ = *src_u++; //u
            *dest_v++ = *src_u++; //v
            if ((int)src_u == (int)last_ptr)
                break;
        }
        memcpy((int32_t*)dest, (int32_t*)src_frame, y_size/4);
    }

    void CameraColorConvert::yuyv_to_yuv422sp (uint8_t* src_frame , uint8_t* dst_frame ,
                                               int width , int height)
    {
        ALOGV("%s: size = %dx%d", __FUNCTION__, width, height);
        int i = 0, j = 0;
        unsigned char* outy = NULL; //y
        unsigned char* outcb = NULL; //u
        unsigned char* outcr = NULL; //v
        uint8_t* inyuv_4 = NULL;
        int outYsize = 0, offset = 0;

        int lpitch = ((width + 15) >> 4) << 4;
        int lheight = height;

        outYsize = (lpitch * width);

        inyuv_4 = src_frame;

        outy = dst_frame;
        outcb = outy + outYsize;
        outcr = outcb + 1;

        offset = lpitch - width;

        for (; i < width; i++) {
            for (; j < (width / 2); j++) {
                *outcb = static_cast<char>(inyuv_4[1]);
                outcb += 2;
                *outy = static_cast<char>(inyuv_4[0]);
                outy++;
                *outcr = static_cast<char>(inyuv_4[3]);
                outcr += 2;
                *outy = static_cast<char>(inyuv_4[2]);
                inyuv_4 += 4;
            }
            outcb += (offset >> 1);
            outcr += (offset >> 1);
            outy += offset;
        }
    }

    /* cim out 64u 64v convert -> rgb565 */
    void CameraColorConvert::yuv420b_64u_64v_to_rgb565(CameraYUVMeta* yuvMeta, uint8_t* dstAddr,
                                                       int rgbwidth, int rgbheight, 
                                                       int rgbstride, int destFmt) {
        if (yuvMeta->yAddr == 0) {
            ALOGE("%s: data is null", __FUNCTION__);
            return;
        }

        int line, col, linewidth;
        int y, u, v, yy, vr, ug, vg, ub;
        int r, g, b;
        const unsigned char *py = NULL, *pu = NULL, *pv = NULL;
        int width = yuvMeta->width;
        int height = yuvMeta->height;
        unsigned short* dst = (unsigned short*)dstAddr;

        py = (unsigned char*)(yuvMeta->yAddr);
        pu = py + (width * height);
        pv = pu + 64;

        int iLoop = 0, jLoop = 0, kLoop = 0, dxy = 0;
        int stride_y = width*16;
        int stride_uv = width*8;

        for (line = 0; line < height; line++) {
            for (col = 0; col < width; col++) {
                if ( iLoop > 0 && iLoop % 16 == 0 ) {
                    jLoop++;
                    iLoop = 0;
                    dxy = jLoop*256;
                    iLoop++;
                } else {
                    dxy = iLoop + jLoop * 256;
                    iLoop++;
                }

                y = *(py+dxy);
                yy = y << 8;
                u = *(pu+dxy/2) - 128;
                ug = 88 * u;
                ub = 454 * u;
                v = *(pv+dxy/2) - 128;
                vg = 183 * v;
                vr = 359 * v;

                r = (yy + vr) >> 8;
                g = (yy - ug - vg) >> 8;
                b = (yy + ub ) >> 8;

                if (r < 0) r = 0;
                if (r > 255) r = 255;
                if (g < 0) g = 0;
                if (g > 255) g = 255;
                if (b < 0) b = 0;
                if (b > 255) b = 255;

                *dst++ = (((unsigned short)r>>3)<<11)
                    | (((unsigned short)g>>2)<<5) | (((unsigned short)b>>3)<<0);
            }

            if ( kLoop > 0 && kLoop % 15 == 0 ) {
                py += stride_y + 16 - 256;
                pu += stride_uv + 8 - 64;
                pv = pu + 64;
                iLoop = 0; jLoop = 0; kLoop = 0;
            }
            else if ( kLoop & 1 ) {
                py += 16;
                pu += 8;
                pv += 8;
                iLoop = 0; jLoop = 0; kLoop++;
            }
            else {
                py += 16;
                iLoop = 0; jLoop = 0;
                kLoop++;
            }
        } 
    }

    void CameraColorConvert::convert_yuv420p_to_rgb565(CameraYUVMeta *yuvMeta, uint8_t *dstAttr)
    {
        int line, col, linewidth;
        int y, u, v, yy, vr, ug, vg, ub;
        int r, g, b;
        const unsigned char *py, *pu, *pv;

        unsigned short *dst = (unsigned short *)dstAttr;
        int width = yuvMeta->width;
        int height = yuvMeta->height;

        linewidth = width >> 1;
        py = (unsigned char *)yuvMeta->yAddr;
        pu = py + (width * height);
        pv = pu + (width * height) / 4;

        y = *py++;
        yy = y << 8;
        u = *pu - 128;
        ug = 88 * u;
        ub = 454 * u;
        v = *pv - 128;
        vg = 183 * v;
        vr = 359 * v;

        for (line = 0; line < height; line++) {
            for (col = 0; col < width; col++) {
                r = (yy + vr) >> 8;
                g = (yy - ug - vg) >> 8;
                b = (yy + ub ) >> 8;

                if (r < 0) r = 0;
                if (r > 255) r = 255;
                if (g < 0) g = 0;
                if (g > 255) g = 255;
                if (b < 0) b = 0;
                if (b > 255) b = 255;
                *dst++ = (((unsigned short)r>>3)<<11) | (((unsigned short)g>>2)<<5) | (((unsigned short)b>>3)<<0); 

                y = *py++;
                yy = y << 8;
                if (col & 1) {
                    pu++;
                    pv++;

                    u = *pu - 128;
                    ug = 88 * u;
                    ub = 454 * u;
                    v = *pv - 128;
                    vg = 183 * v;
                    vr = 359 * v;
                }
            }
            if ((line & 1) == 0) {
                pu -= linewidth;
                pv -= linewidth;
            }
        }
    }

    void CameraColorConvert::yuv420p_to_yuv420sp(uint8_t* src_frame, uint8_t* dest_frame,
                                                 int width, int height) {
        if (src_frame == 0) {
            ALOGE("%s: data is null",__FUNCTION__);
            return;
        }

        int y_size = width * height;
        int uv_size = y_size >> 1;
        uint8_t* dest_u_base = dest_frame + y_size;
        uint8_t* tmp_u = src_frame + y_size;
        uint8_t* tmp_v = tmp_u + (uv_size>>1);

        memcpy(dest_frame, src_frame, y_size);

        for (int i = 1; i <= uv_size; ++i) {
            if (i % 2 == 0) {
                *dest_u_base++ = *tmp_u++;
            } else {
                *dest_u_base++ = *tmp_v++;
            }
        }
    }

    void CameraColorConvert::yuv420tile_to_yuv420sp(CameraYUVMeta* yuvMeta, uint8_t* dest) {
        ALOGV("%s: width = %d, height = %d, format = %d",
                 __FUNCTION__, yuvMeta->width, yuvMeta->height, yuvMeta->format);

    }

    void CameraColorConvert::yuv420p_to_rgb565 (uint8_t* src_frame , uint8_t* dst_frame ,
                                                int width , int height) {
        ALOGV("%s: size = %dx%d", __FUNCTION__, width, height);
        uint8_t* kAdjustedClip = &mClip[-kClipMin]; 

        uint16_t* dst_ptr = (uint16_t*) dst_frame;

        const uint8_t* src_y = (const uint8_t*) src_frame;
        const uint8_t* src_u = (const uint8_t*) src_y + width * height;
        const uint8_t* src_v = src_u + (width / 2) * (height / 2);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; x += 2) {
                signed y1 = (signed) src_y[x] - 16;
                signed y2 = (signed) src_y[x + 1] - 16;

                signed u = (signed) src_u[x / 2] - 128;
                signed v = (signed) src_v[x / 2] - 128;

                signed u_b = u * 517;
                signed u_g = -u * 100;
                signed v_g = -v * 208;
                signed v_r = v * 409;

                signed tmp1 = y1 * 298;
                signed b1 = (tmp1 + u_b) / 256;
                signed g1 = (tmp1 + v_g + u_g) / 256;
                signed r1 = (tmp1 + v_r) / 256;

                signed tmp2 = y2 * 298;
                signed b2 = (tmp2 + u_b) / 256;
                signed g2 = (tmp2 + v_g + u_g) / 256;
                signed r2 = (tmp2 + v_r) / 256;

                uint32_t rgb1 = ((kAdjustedClip[r1] >> 3) << 11)
                    | ((kAdjustedClip[g1] >> 2) << 5) | (kAdjustedClip[b1] >> 3);

                uint32_t rgb2 = ((kAdjustedClip[r2] >> 3) << 11)
                    | ((kAdjustedClip[g2] >> 2) << 5) | (kAdjustedClip[b2] >> 3);

                if (x + 1 < width) {
                    *(uint32_t *) (&dst_ptr[x]) = (rgb2 << 16) | rgb1;
                } else {
                    dst_ptr[x] = rgb1;
                }
            }
            src_y += width;
            if (y & 1) {
                src_u += width / 2;
                src_v += width / 2;
            }
            dst_ptr += width;
        }
    }

    void CameraColorConvert::yuv420p_to_tile420(CameraYUVMeta* yuvMeta, char *yuv420t) {

        if (yuvMeta->yAddr == 0) {
            ALOGE("%s: data is null", __FUNCTION__);
            return;
        }

        char *y_p = (char*)(yuvMeta->yAddr);
        int width = yuvMeta->width;
        int height = yuvMeta->height;
        unsigned int y_p_size = width*height;
        char *u_p = y_p+y_p_size;
        unsigned int u_p_size = y_p_size / 4;
        char *v_p = u_p+u_p_size;
        unsigned int v_p_size = u_p_size;

        char *y_t = yuv420t;
        unsigned int y_t_size = y_p_size;
        char *uv_t = y_t + y_t_size;
        unsigned int uv_t_size = u_p_size+v_p_size;

        int i_w, j_h;
        int y_w_mcu = width / 16;
        int y_h_mcu = height / 16;

        int u_w_mcu = y_w_mcu;
        int u_h_mcu = y_h_mcu;

        int v_w_mcu = u_w_mcu;
        int v_h_mcu = u_h_mcu;

        int i_mcu;

        char *y_mcu = y_p;
        char *u_mcu = u_p;
        char *v_mcu = v_p;
        char *y_tmp = y_p;

        char *y_t_tmp = y_t;
        for(j_h = 0; j_h < y_h_mcu; j_h++)
            {
                y_mcu = y_p + j_h*y_w_mcu*256;
                for(i_w = 0; i_w < y_w_mcu; i_w++)
                    {
                        y_tmp = y_mcu;
                        for(i_mcu = 0; i_mcu < 16; i_mcu++)
                            {
                                memcpy(y_t_tmp, y_tmp, 16);
                                y_t_tmp += 16;
                                y_tmp += width;
                            }
                        y_mcu += 16;
                    }
            }

        char *u_tmp = u_p;
        char *v_tmp = v_p;
        char *u_uv_t_tmp = uv_t;
        char *v_uv_t_tmp = uv_t+8;
        for(j_h = 0; j_h < u_h_mcu; j_h++)
            {
                u_mcu = u_p + j_h*u_w_mcu*64;
                v_mcu = v_p + j_h*v_w_mcu*64;
                for(i_w = 0; i_w < u_w_mcu; i_w++)
                    {
                        u_tmp = u_mcu;
                        v_tmp = v_mcu;
                        for(i_mcu = 0; i_mcu < 8; i_mcu++)
                            {
                                memcpy(u_uv_t_tmp, u_tmp, 8);
                                memcpy(v_uv_t_tmp, v_tmp, 8);
				
                                u_uv_t_tmp += 16;
                                v_uv_t_tmp += 16;

                                u_tmp += width / 2;
                                v_tmp += width / 2;
                            }

                        u_mcu += 8;
                        v_mcu += 8;
                    }
            }
    }

    void CameraColorConvert::yuv420sp_to_yuv420p(uint8_t* src_frame, uint8_t* dst_frame,
                                                 int width, int height) {
        int y_size = width*height;
        int u_size = y_size>>2;
        int uv_size = y_size>>1;
        int64_t i = 1;
        uint8_t* dest_u_base = dst_frame + y_size;
        uint8_t* dest_v_base = dest_u_base + u_size;
        uint8_t* src_u_base = src_frame + y_size;
        uint8_t* last_ptr = src_u_base + uv_size; 

        memcpy(dst_frame, src_frame, y_size);
        while (1) {
            if ((int)src_u_base == (int)last_ptr)
                break;
            if (i++ % 2 == 0) {
                *dest_u_base++ = *src_u_base++; //u
            } else {
                *dest_v_base++ = *src_u_base++; //v
            }
        }
    }

    void CameraColorConvert::yuv420p_to_yuv422sp(uint8_t* src_frame, uint8_t* dest_frame,
                                                 int width, int height) {

        int y_size = width*height;
        uint8_t* src_u = src_frame + y_size;
        uint8_t* src_v = src_u + (y_size>>2);
        uint8_t* last_ptr = src_v + (y_size>>2);
        uint8_t* dest_u = dest_frame + y_size;
        int64_t i = 1;

        while (1) {
            if (i % 2 == 0) {
                *dest_u++ = *src_v++;
            } else {
                *dest_u++ = *src_u++;
            }
            if ((int)src_v == (int)last_ptr)
                break;
        }
        memcpy((int32_t*)dest_frame, (int32_t*)src_frame, y_size/4); 
    }

    void CameraColorConvert::yuv420sp_to_rgb565 (uint8_t* src_frame , uint8_t* dst_frame ,
                                                 int width , int height)
    {
        ALOGV("%s: size = %dx%d", __FUNCTION__, width, height);
        uint8_t* kAdjustedClip = &mClip[-kClipMin];

        const uint8_t* src_y = (const uint8_t*) src_frame;
        const uint8_t* src_u = (const uint8_t*) src_y + width * height;
        uint16_t* dst_ptr = (uint16_t*) dst_frame;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; x += 2) {
                signed y1 = (signed) src_y[x] - 16;
                signed y2 = (signed) src_y[x + 1] - 16;

                signed u = (signed) src_u[x & ~1] - 128;
                signed v = (signed) src_u[(x & ~1) + 1] - 128;

                signed u_b = u * 517;
                signed u_g = -u * 100;
                signed v_g = -v * 208;
                signed v_r = v * 409;

                signed tmp1 = y1 * 298;
                signed b1 = (tmp1 + u_b) / 256;
                signed g1 = (tmp1 + v_g + u_g) / 256;
                signed r1 = (tmp1 + v_r) / 256;

                signed tmp2 = y2 * 298;
                signed b2 = (tmp2 + u_b) / 256;
                signed g2 = (tmp2 + v_g + u_g) / 256;
                signed r2 = (tmp2 + v_r) / 256;

                uint32_t rgb1 = ((kAdjustedClip[b1] >> 3) << 11)
                    | ((kAdjustedClip[g1] >> 2) << 5) | (kAdjustedClip[r1] >> 3);

                uint32_t rgb2 = ((kAdjustedClip[b2] >> 3) << 11)
                    | ((kAdjustedClip[g2] >> 2) << 5) | (kAdjustedClip[r2] >> 3);
                if (x + 1 < width) {
                    *(uint32_t*) (&dst_ptr[x]) = (rgb2 << 16) | rgb1;
                } else {
                    dst_ptr[x] = rgb1;
                }
            }
            src_y += width;
            if (y & 1)
                src_u += width;
            dst_ptr += width;
        }
    }

    void CameraColorConvert::yuv420sp_to_argb8888 (uint8_t* src_frame ,
                                                   uint8_t* dst_frame , int width , int height)
    {
        ALOGV("%s: size = %dx%d", __FUNCTION__, width, height);
        int frame_size = width * height;
        int uvp = 0;
        int i = 0, j = 0;
        int u = 0, v = 0;
        int yp = 0;
        uint8_t* yuv420sp = src_frame;
        uint* rgb = reinterpret_cast<uint*>(dst_frame);

        for (j = 0; j < height; j++) {
            uvp = frame_size + (j >> 1) * width;
            u = v = 0;
            for (; i < width; i++, yp++) {
                int y = (0xff & (int) yuv420sp[yp]) - 16;
                if (y < 0)
                    y = 0;
                if ((i & 1) == 0) {
                    v = (0xff & yuv420sp[uvp++]) - 128;
                    u = (0xff & yuv420sp[uvp++]) - 128;
                }

                int y1192 = 1192 * y;
                int r = (y1192 + 1634 * v);
                int g = (y1192 - 833 * v - 400 * u);
                int b = (y1192 + 2066 * u);

                if (r < 0)
                    r = 0;
                else if (r > 262143)
                    r = 262143;
                if (g < 0)
                    g = 0;
                else if (g > 262143)
                    g = 262143;
                if (b < 0)
                    b = 0;
                else if (b > 262143)
                    b = 262143;

                rgb[yp] = (0xff000000 | ((r << 6) & 0xff0000) | ((g >> 2) & 0xff00)
                           | ((b >> 10) & 0xff));
            }
        }
    }

    void CameraColorConvert::initClip (void)
    {
        if (mClip == NULL) {
            mClip = new uint8_t[kClipMax - kClipMin + 1];

            for (signed i = kClipMin; i <= kClipMax; ++i)
                mClip[i - kClipMin] = (i < 0) ? 0 : (i > 255) ? 255 : (uint8_t) i;
        }
    }

};
